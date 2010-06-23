/*
 * libpsys - Linux package manager interaction library
 *
 * Copyright (C) 2010  Denis Washington <dwashington@gmx.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301  USA
 */

/*
 * fallback_dpkg.c - psys backend fallback implementation for DPKG-based
 * distributions (Debian/Ubuntu and derivatives)
 */

/* Needed for asprintf */
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <search.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define LIBDPKG_VOLATILE_API
#include <dpkg/dpkg.h>
#include <dpkg/dpkg-db.h>

#include <psys_impl.h>
#include "fallback_private.h"

static const char *distros[] = {
	"Debian",
	"LinuxMint",
	"Ubuntu",
	NULL
};

/*
 * To ease understanding of the code in this file, some things about the
 * libdpkg API (as libdpkg is not really a library for public consumption
 * but rather thought for internal usage in dpkg and friends, it is
 * essentially undocumented; what I write here is what I have figured out
 * myself and may not be 100% accurate):
 *
 * - libdpkg has an error handling system similar to exception handling in
 *   most modern mainstream programming languages. If a libdpkg function
 *   encounters an error, it does a longjmp() to a location defined by a
 *   jump_buf passed to push_error_handler(). With the accompanying setjmp()
 *   call we can specify what happens in such an error condition. This
 *   includes invoking libdpkg's unwinding mechanism (error_unwind())
 *   which cleans up all resources acquired by any called dpkg function.
 *
 * - Closely interleaved with the unwinding mechanism are libdpkg's means
 *   of memory allocation, which is done with nfmalloc() instead of
 *   malloc(). nfmalloc() allocates all data on a single obstack (see
 *   the GNU C Library Manual section about obstacks) which can be freed
 *   completely in one go with nffreeall(). This prevents memory leaks
 *   when jumping to the error handler. nfmalloc() also differs from
 *   malloc() in that it aborts on error, so the return value does not
 *   need to be checked. There are also some convenience functions built
 *   on top of nfmalloc(), such as nfstrsave() (like strdup()).
 *
 * The existence of the setjmp()/longjmp() error handling mechanism and
 * nfmalloc() is the reason why return value checking or other means of
 * explicit error handling are absent where they would be needed with the
 * regular Unix/Linux C library functions.
 */

/* Required by libdpkg */
const char thisname[] = "libpsys";

#define ADMINDIR "/var/lib/dpkg"

#define init_error_handler(err, jbuf, label) \
	if (setjmp(jbuf)) { \
		error_unwind(ehflag_bombout); \
		if (!*err) \
			psys_err_set(err, PSYS_EINTERNAL, \
				     "Unknown internal error occurred"); \
		ret = -1; \
		goto label; \
	} \
	push_error_handler(&jbuf, (error_printer *) error_printer_fn, \
			   (const char *) err); \

#define set_error_handler(err, jbuf, label) \
	if (setjmp(jbuf)) { \
		error_unwind(ehflag_bombout); \
		if (!*err) \
			psys_err_set(err, PSYS_EINTERNAL, \
				     "Unknown internal error occurred"); \
		ret = -1; \
		goto label; \
	}

#define cleanup() \
	if (ret == 0) { \
		set_error_display(NULL, NULL); \
		error_unwind(ehflag_normaltidy); \
	} \
	nffreeall();

/*** Fallback matching functions **********************************************/

int dpkg_fallback_match(void)
{
	return fallback_match_by_distro(distros);
}

int dpkg_fallback_match_fuzzy(void)
{
	return system("dpkg --version >/dev/null") == 0;
}

/*** General helper functions *************************************************/

static char *dpkg_name(const char *vendor, const char *name)
{
	size_t len;
	char *dpkgname;

	len = snprintf(NULL, 0, "lsb-%s-%s", vendor, name);
	dpkgname = nfmalloc(len + 1);
	snprintf(dpkgname, len + 1, "lsb-%s-%s", vendor, name);

	return dpkgname;
}

static const char *dpkg_arch(const char *lsbarch, psys_err_t *err)
{
	const char *dpkgarch;

	if (!strcmp(lsbarch, "amd64")) {
		dpkgarch = "amd64";
	} else if (!strcmp(lsbarch, "ia32")) {
		dpkgarch = "i386";
	} else if (!strcmp(lsbarch, "ia64")) {
		dpkgarch = "ia64";
	} else if (!strcmp(lsbarch, "noarch")) {
		dpkgarch = "all";
	} else if (!strcmp(lsbarch, "powerpc")) {
		dpkgarch = "ppc";
	} else if (!strcmp(lsbarch, "powerpc")) {
		dpkgarch = "ppc64";
	} else if (!strcmp(lsbarch, "s390")) {
		dpkgarch = "s390";
	} else if (!strcmp(lsbarch, "s390x")) {
		dpkgarch = "s390";
	} else {
		psys_err_set(err, PSYS_EARCH,
			     "Unknown machine architecture `%s'",
			     lsbarch);
		return NULL;
	}

	return dpkgarch;
}

static void error_printer_fn(const char *emsg, psys_err_t *err)
{
	psys_err_set(err, PSYS_EINTERNAL, emsg);
}

/*** Sanity checks ************************************************************/

int ensure_installed(struct pkginfo *dpkg, psys_err_t *err)
{
	if (dpkg->status != stat_installed) {
		psys_err_set(err, PSYS_ENOENT,
			     "Package named `%s' is not installed",
			     dpkg->name);
		return -1;
	}
	return 0;
}

int ensure_not_installed(struct pkginfo *dpkg, psys_err_t *err)
{
	if (dpkg->status == stat_installed) {
		psys_err_set(err, PSYS_EEXIST,
			     "A package named `%s' is already installed",
			     dpkg->name);
		return -1;
	}
	return 0;
}

int ensure_dependencies_met(psys_pkg_t pkg, psys_err_t *err)
{
	struct pkginfo *lsb_dpkg;

	lsb_dpkg = findpackage("lsb");
	if (lsb_dpkg->status != stat_installed) {
		psys_err_set(err, PSYS_ELSBVER,
			     "The system is currently not Linux Standard "
			     "Base (LSB) compliant: Package `lsb' is not "
			     "installed");
		return -1;
	}

	if (psys_pkg_lsbvercmp(pkg, lsb_dpkg->installed.version.version) < 0) {
		psys_err_set(err, PSYS_ELSBVER,
			     "The system's Linux Standard Base (LSB) "
			     "compliance is not sufficient: version %s is "
			     "required, but package `lsb' is at version %s",
			     psys_pkg_lsbversion(pkg),
			     lsb_dpkg->installed.version);
		return -1;
	}

	return 0;
}

static int ensure_version_newer(psys_pkg_t pkg, struct pkginfo *dpkg,
				psys_err_t *err)
{
	const char *dpkgversion;
	int diff;

	dpkgversion = dpkg->installed.version.version;
	diff = psys_pkg_vercmp(pkg, dpkgversion);
	if (diff < 0) {
		psys_err_set(err, PSYS_EVER,
			     "Installed package `%s' is newer than version "
			     "%s (%s)",
			     dpkg->name, psys_pkg_version(pkg), dpkgversion);
		return -1;
	} else if (diff == 0) {
		psys_err_set(err, PSYS_EVER,
			     "Package `%s' is already at version %s",
			     dpkg->name, dpkgversion);
		return -1;
	}
	return 0;
}

static int ensure_no_conflict(psys_plist_t p, psys_err_t *err)
{
	int rc;

	rc = access(psys_plist_path(p), F_OK);
	if (rc == 0) {
		psys_err_set(err, PSYS_ECONFLICT,
			     "File name `%s' is already in use",
			     psys_plist_path(p));
		return -1;
	} else if (errno != ENOENT) {
		psys_err_set(err, PSYS_EINTERNAL,
			     "Could not check existence of file name "
			     "`%s': %s",
			     psys_plist_path(p), strerror(errno));
		return -1;
	}
	return 0;
}

static int ensure_no_conflicting_extras(psys_pkg_t pkg, psys_err_t *err) {
	psys_plist_t e;

	for (e = psys_pkg_extras(pkg); e; e = psys_plist_next(e)) {
		if (ensure_no_conflict(e, err))
			return -1;
	}

	return 0;
}

/*** Adding package metadata **************************************************/

static const char *tlist_value_c(psys_tlist_t list)
{
	psys_tlist_t t;

	for (t = list; t; t = psys_tlist_next(t)) {
		if (!strcmp(psys_tlist_locale(t), "C"))
			return psys_tlist_value(t);
	}
	return NULL;
}

static void set_description(struct pkginfo *dpkg, psys_pkg_t pkg)
{
	const char *pkg_summary;
	const char *pkg_description;
	char *dpkg_description;

	pkg_summary = tlist_value_c(psys_pkg_summary(pkg));
	pkg_description = tlist_value_c(psys_pkg_description(pkg));

	if (pkg_summary && pkg_description) {
		int len;
		len = snprintf(NULL, 0, "%s\n %s", pkg_summary, pkg_description);
		dpkg_description = nfmalloc(len + 1);
		snprintf(dpkg_description, len + 1, "%s\n %s", pkg_summary,
			 pkg_description);
	} else if (pkg_summary) {
		dpkg_description = nfstrsave(pkg_summary);	
	} else if (pkg_description) {
		dpkg_description = nfmalloc(strlen(pkg_description) + 2);
		dpkg_description[0] = '\n';
		strncpy(dpkg_description + 1, pkg_description,
			strlen(pkg_description));
	} else {
		dpkg_description = nfstrsave("");
	}

	dpkg->installed.description = dpkg_description;
}

static void set_installed_size(struct pkginfo *dpkg, psys_flist_t files)
{
	off_t installedsize;
	psys_flist_t f;
	size_t len;
	char *installedsize_str;

	installedsize = 0;
	for (f = files; f; f = psys_flist_next(f))
		installedsize += psys_flist_stat(f)->st_size;
	installedsize /= 1000;

	len = snprintf(NULL, 0, "%llu", (long long unsigned) installedsize);
	installedsize_str = nfmalloc(len + 1);
	snprintf(installedsize_str, len + 1, "%llu",
		 (long long unsigned) installedsize);

	dpkg->installed.installedsize = installedsize_str;
}

void add_dependencies(struct pkginfo *dpkg, psys_pkg_t pkg)
{
	struct dependency *dep;
	struct deppossi *depp;

	dep = nfmalloc(sizeof(*dep));
	dep->up = dpkg;
	dep->next = NULL;
	dep->type = dep_depends;

	depp = nfmalloc(sizeof(*depp));
	depp->up = dep;
	depp->next = NULL;
	depp->nextrev = NULL;
	depp->backrev = NULL;
	depp->ed = findpackage("lsb");
	depp->verrel = dvr_laterequal;
	parseversion(&depp->version, psys_pkg_lsbversion(pkg));

	dep->list = depp;
	dpkg->installed.depends = dep;
}

/*** Creating and remove package info files ***********************************/

static char *info_file_path(struct pkginfo *dpkg, const char *extension)
{
	size_t len;
	char *path;

	len = snprintf(NULL, 0, "/var/lib/dpkg/info/%s.%s", dpkg->name,
		       extension);
	path = nfmalloc(len + 1);
	snprintf(path, len + 1, "/var/lib/dpkg/info/%s.%s", dpkg->name,
		 extension);

	return path;
}

static int create_list(struct pkginfo *dpkg, char *extension, char **path,
		       FILE **list, psys_err_t *err)
{
	if (asprintf(path, "/var/lib/dpkg/info/%s.%s",
		     dpkg->name, extension) < 0) {
		psys_err_set_nomem(err);
		return -1;
	}

	*list = fopen(*path, "w");
	if (!*list) {
		psys_err_set(err, PSYS_EINTERNAL,
			     "Cannot create `%s.%s': %s",
			     dpkg->name, extension, strerror(errno));
		free(*path);
		return -1;
	}

	return 0;
}

static int pathcmp_fn(const void *a, const void *b)
{
	return strcmp((const char *) a, (const char *) b);
}

static void ftree_destroy_fn(void *data)
{
	/* 
	 * Do nothing. No, this is not a stub. See the tdestroy(3) man
	 * page and you'll know why we need this.
	 */
}

static int add_to_file_list(FILE *list, const char *path, void **ftree,
			    psys_err_t *err)
{
	char *found;

	found = tfind(path, ftree, pathcmp_fn);
	if (!found) {
		int rc;

		if (strcmp(path, "/") != 0)
			add_to_file_list(list, dirname(nfstrsave(path)),
					 ftree, err);

		if (!strcmp(path, "/"))
			rc = fprintf(list, "/.\n"); 
		else
			rc = fprintf(list, "%s\n", path);

		if (rc < 0) {
			psys_err_set(err, PSYS_EINTERNAL,
				     "Cannot write to file list: %s",
				     strerror(errno));
			return -1;
		}

		if (!tsearch(path, ftree, pathcmp_fn)) {
			psys_err_set_nomem(err);
			return -1;
		}
	}
	return 0;
}

char *create_file_list(struct pkginfo *dpkg, psys_flist_t files,
		       psys_err_t *err)
{
	char *path;
	FILE *list;
	void *ftree;
	psys_flist_t f;

	if (create_list(dpkg, "list", &path, &list, err))
		return NULL;

	ftree = NULL;
	for (f = files; f; f = psys_flist_next(f)) {
		if (add_to_file_list(list, psys_flist_path(f), &ftree, err)) {
			tdestroy(ftree, ftree_destroy_fn);
			fclose(list);
			remove(path);
			free(path);
			return NULL;
		}
	}

	tdestroy(ftree, ftree_destroy_fn);
	fclose(list);
	return path;
}

int add_to_md5sums_list(FILE *list, psys_flist_t file, psys_err_t *err)
{
	if (S_ISREG(psys_flist_stat(file)->st_mode)) {
		char *md5;
		int rc;

		md5 = psys_flist_md5sum(file, err);
		if (!md5)
			return -1;

		rc = fprintf(list, "%s %s\n", md5, psys_flist_path(file) + 1);
		free(md5);
		if (rc < 0) {
			psys_err_set(err, PSYS_EINTERNAL,
				     "Cannot write to md5sums list: %s",
				     strerror(errno));
			return -1;
		}
	}
	return 0;
}

char *create_md5sums_list(struct pkginfo *dpkg, psys_flist_t files,
			  psys_err_t *err)
{
	char *path;
	FILE *list;
	psys_flist_t f;

	if (create_list(dpkg, "md5sums", &path, &list, err))
		return NULL;

	for (f = files; f; f = psys_flist_next(f)) {
		if (add_to_md5sums_list(list, f, err)) {
			fclose(list);
			remove(path);
			free(path);
			return NULL;
		}
	}

	return path;
}

void remove_info_files(struct pkginfo *dpkg)
{
	remove(info_file_path(dpkg, "list"));
	remove(info_file_path(dpkg, "md5sums"));
}

/*** psys_announce() **********************************************************/

int dpkg_psys_announce(psys_pkg_t pkg, psys_err_t *err)
{
	int ret;
	jmp_buf buf;
	char *dpkgname = NULL;
	struct pkginfo *dpkg;

	pkg = psys_pkg_copy(pkg);
	if (!pkg) {
		psys_err_set_nomem(err);
		return -1;
	}
	psys_pkg_assert_valid(pkg);

	init_error_handler(err, buf, out);
	modstatdb_init(ADMINDIR, msdbrw_needsuperuser);

	dpkgname = dpkg_name(psys_pkg_vendor(pkg), psys_pkg_name(pkg));
	dpkg = findpackage(dpkgname);
	if (ensure_not_installed(dpkg, err)) {
		ret = -1;
		goto out;
	}

	if (ensure_dependencies_met(pkg, err)) {
		ret = -1;
		goto out;
	}

	if (ensure_no_conflicting_extras(pkg, err)) {
		ret = -1;
		goto out;
	}

	ret = 0;
out:
	modstatdb_shutdown();
	cleanup();
	psys_pkg_free(pkg);
	return ret;
}

/*** psys_register() **********************************************************/

static int do_register(psys_pkg_t pkg, psys_err_t *err, jmp_buf *buf)
{
	int ret;
	char *dpkgname = NULL;
	const char *dpkgarch;
	struct pkginfo *dpkg;
	psys_flist_t flist = NULL;
	char *filelist_path = NULL;
	char *md5list_path = NULL;

	set_error_handler(err, *buf, out);

	dpkgname = dpkg_name(psys_pkg_vendor(pkg), psys_pkg_name(pkg));
	dpkg = findpackage(dpkgname);
	if (ensure_not_installed(dpkg, err)) {
		ret = -1;
		goto out;
	}

	if (ensure_dependencies_met(pkg, err)) {
		ret = -1;
		goto out;
	}

	blankpackage(dpkg);
	blankpackageperfile(&dpkg->installed);

	/* Name, Version */
	dpkg->name = dpkgname;
	parseversion(&dpkg->installed.version, psys_pkg_version(pkg));

	/* Architecture */
	dpkgarch = dpkg_arch(psys_pkg_arch(pkg), err);
	if (!dpkgarch) {
		ret = -1;
		goto out;
	}
	dpkg->installed.architecture = dpkgarch;

	/* Description */
	set_description(dpkg, pkg);

	/* 
	 * Maintainer
	 *
	 * FIXME: Our Maintainer value does not conform to the format
	 * mandated by the Debian Policy Manual (which is "Name <E-Mail>"),
	 * but this is better than not specifying a Maintainer at all
	 * (which is a mandatory field)
	 */
	dpkg->installed.maintainer = nfstrsave(psys_pkg_vendor(pkg));

	/* Priority */
	dpkg->priority = pri_optional;

	/* Dependencies */
	add_dependencies(dpkg, pkg);

	flist = psys_pkg_flist(pkg, err);
	if (!flist) {
		ret = -1;
		goto out;
	}

	/* Installed Size */
	set_installed_size(dpkg, flist);

	/* File List */
	filelist_path = create_file_list(dpkg, flist, err);
	if (!filelist_path) {
		ret = -1;
		goto out;
	}

	/* MD5SUMS List */
	md5list_path = create_md5sums_list(dpkg, flist, err);
	if (!md5list_path) {
		ret = -1;
		goto out;
	}

	dpkg->want = want_install;
	dpkg->status = stat_installed;
	modstatdb_note(dpkg);

	ret = 0;
out:
	if (md5list_path) {
		if (ret == -1)
			remove(md5list_path);
		free(md5list_path);
	}
	if (filelist_path) {
		if (ret == -1)
			remove(filelist_path);
		free(filelist_path);
	}
	if (flist)
		psys_flist_free(flist);
	return ret;
}


int dpkg_psys_register(psys_pkg_t pkg, psys_err_t *err)
{
	int ret;
	jmp_buf buf;

	pkg = psys_pkg_copy(pkg);
	if (!pkg) {
		psys_err_set_nomem(err);
		return -1;
	}
	psys_pkg_assert_valid(pkg);

	init_error_handler(err, buf, out);
	modstatdb_init(ADMINDIR, msdbrw_needsuperuser);

	ret = do_register(pkg, err, &buf);
out:
	modstatdb_shutdown();
	cleanup();
	psys_pkg_free(pkg);
	return ret;
}

/*** psys_announce_update() ***************************************************/

int dpkg_psys_announce_update(psys_pkg_t pkg, psys_err_t *err)
{
	int ret;
	jmp_buf buf;
	char *dpkgname;
	struct pkginfo *dpkg;

	pkg = psys_pkg_copy(pkg);
	if (!pkg) {
		psys_err_set_nomem(err);
		return -1;
	}
	psys_pkg_assert_valid(pkg);

	init_error_handler(err, buf, out);
	modstatdb_init(ADMINDIR, msdbrw_needsuperuser);

	dpkgname = dpkg_name(psys_pkg_vendor(pkg), psys_pkg_name(pkg));
	dpkg = findpackage(dpkgname);
	if (ensure_installed(dpkg, err)) {
		ret = -1;
		goto out;
	}

	if (ensure_version_newer(pkg, dpkg, err)) {
		ret = -1;
		goto out;
	}

	ret = 0;
out:
	modstatdb_shutdown();
	cleanup();
	psys_pkg_free(pkg);
	return ret;
}

/*** psys_register_update() ***************************************************/

int dpkg_psys_register_update(psys_pkg_t pkg, psys_err_t *err)
{
	int ret;
	jmp_buf buf;
	char *dpkgname;
	struct pkginfo *dpkg;

	pkg = psys_pkg_copy(pkg);
	if (!pkg) {
		psys_err_set_nomem(err);
		return -1;
	}
	psys_pkg_assert_valid(pkg);

	init_error_handler(err, buf, out);
	modstatdb_init(ADMINDIR, msdbrw_needsuperuser);

	dpkgname = dpkg_name(psys_pkg_vendor(pkg), psys_pkg_name(pkg));
	dpkg = findpackage(dpkgname);
	if (ensure_installed(dpkg, err)) {
		ret = -1;
		goto out;
	}

	if (ensure_version_newer(pkg, dpkg, err)) {
		ret = -1;
		goto out;
	}

	dpkg->status = stat_notinstalled;
	ret = do_register(pkg, err, &buf);
out:
	modstatdb_shutdown();
	cleanup();
	psys_pkg_free(pkg);
	return ret;
}

/*** psys_unannounce() ********************************************************/

int dpkg_psys_unannounce(const char *vendor, const char *name, psys_err_t *err)
{
	int ret;
	jmp_buf buf;
	char *dpkgname;
	struct pkginfo *dpkg;

	init_error_handler(err, buf, out);
	modstatdb_init(ADMINDIR, msdbrw_needsuperuser);

	dpkgname = dpkg_name(vendor, name);
	dpkg = findpackage(dpkgname);
	if (ensure_installed(dpkg, err)) {
		ret = -1;
		goto out;
	}

	ret = 0;
out:
	modstatdb_shutdown();
	cleanup();
	return ret;
}

/*** psys_unregister() ********************************************************/

int dpkg_psys_unregister(const char *vendor, const char *name, psys_err_t *err)
{
	int ret;
	jmp_buf buf;
	char *dpkgname;
	struct pkginfo *dpkg;

	init_error_handler(err, buf, out);
	modstatdb_init(ADMINDIR, msdbrw_needsuperuser);

	dpkgname = dpkg_name(vendor, name);
	dpkg = findpackage(dpkgname);
	if (ensure_installed(dpkg, err)) {
		ret = -1;
		goto out;
	}

	remove_info_files(dpkg);
	dpkg->want = want_purge;
	dpkg->status = stat_notinstalled;
	blankpackageperfile(&dpkg->installed);
	modstatdb_note(dpkg);

	ret = 0;
out:
	modstatdb_shutdown();
	cleanup();
	return ret;	
}
