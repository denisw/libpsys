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
 * fallback_rpm.c - psys backend fallback implementation for RPM-based
 * distributions (RPM 4.x / 5.x)
 */

/* Needed for asprintf */
#define _GNU_SOURCE

/* Enable compatibility mode for RPM >= 4.6.0 */
#define _RPM_4_4_COMPAT

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <libgen.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <rpm/rpmlib.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmts.h>
#include <rpm/header.h>

#include <psys_impl.h>
#include "fallback_private.h"

/* Known RPM-based distros (lsb_release distributor IDs) */
/* TODO: Fill this list */
static const char *distros[] = {
	"Fedora",
	"MandrivaLinux",
	"SUSE LINUX",
	NULL
};

/*** Fallback matching functions **********************************************/

int rpm_fallback_match(void)
{
	return fallback_match_by_distro(distros);
}

int rpm_fallback_match_fuzzy(void)
{
	return system("rpm --version >/dev/null") == 0;
}

/*** General helper functions *************************************************/

static char *rpm_name(const char *vendor, const char *name)
{
	char *rpmname;

	if (asprintf(&rpmname, "lsb-%s-%s", vendor, name) < 0)
		return NULL;
	else
		return rpmname;
}

static const char *rpm_arch(const char *lsbarch, psys_err_t *err)
{
	const char *rpmarch;

	if (!strcmp(lsbarch, "amd64")) {
		rpmarch = "x86_64";
	} else if (!strcmp(lsbarch, "ia32")) {
		rpmarch = "i486";
	} else if (!strcmp(lsbarch, "ia64")) {
		rpmarch = "ia64";
	} else if (!strcmp(lsbarch, "noarch")) {
		rpmarch = "noarch";
	} else if (!strcmp(lsbarch, "ppc32")) {
		rpmarch = "ppc";
	} else if (!strcmp(lsbarch, "ppc64")) {
		rpmarch = "ppc64";
	} else if (!strcmp(lsbarch, "s390")) {
		rpmarch = "s390";
	} else if (!strcmp(lsbarch, "s390x")) {
		rpmarch = "s390x";
	} else {
		psys_err_set(err, PSYS_EARCH,
			     "Unknown machine architecture `%s'",
			     lsbarch);
		return NULL;
	}

	if (!rpmMachineScore(RPM_MACHTABLE_INSTARCH, rpmarch)) {
		psys_err_set(err, PSYS_EARCH,
			     "Incompatible machine architecture `%s'",
			     lsbarch);
		return NULL;
	}

	return rpmarch;
}

static rpmts create_transaction_set(psys_err_t *err)
{
	rpmts ts;

	if (rpmReadConfigFiles(NULL, NULL)) {
		psys_err_set(err, PSYS_EINTERNAL,
			     "Cannot read RPM configuration");
		return NULL;
    	}

	ts = rpmtsCreate();
	if (!ts)
		return NULL;

	if (rpmtsOpenDB(ts, O_RDWR)) {
		psys_err_set(err, PSYS_EINTERNAL,
			     "Cannot open RPM database");
		rpmtsFree(ts);
		return NULL;
	}

	return ts;
}

static unsigned int find_db_record(rpmts ts, unsigned int tag,
				   const char *value, const char *arch,
				   Header *header,
				   psys_err_t *err)
{
	rpmdbMatchIterator it;
	Header h;
	unsigned int offset;

	it = rpmtsInitIterator(ts, tag, value, 0);
	h = rpmdbNextIterator(it);
	if (!h) {
		/*
		 * We set the error message to an empty string as it
		 * depends on which find_by_*() function was called;
		 * that function will overwrite the error object created
		 * here with its own
		 */
		psys_err_set(err, PSYS_ENOENT, "");
		rpmdbFreeIterator(it);
		return -1;
	}

	offset = UINT_MAX;
	if (header)
		*header = NULL;

	for (; h; h = rpmdbNextIterator(it)) {
		int match;

		if (arch) {
			char *harch;
			if (!headerGetEntry(h, RPMTAG_ARCH, NULL,
					    (void **) &harch, NULL))
				continue;
			else
				match = !strcmp(harch, arch);
		} else {
			match = 1;
		}

		if (match) {
			offset = (int) rpmdbGetIteratorOffset(it);

			if (header) {
				*header = headerCopy(h);
				if (!(*header)) {
					psys_err_set_nomem(err);
					rpmdbFreeIterator(it);
					return UINT_MAX;
				}
			}

			break;
		}
	}

	if (offset == UINT_MAX) {
		psys_err_set(err, PSYS_EARCH,
			     "Installed package `%s' does not have "
			     "required architecture `%s'",
			     value, arch);
	}

	rpmdbFreeIterator(it);
	return offset;
}

static int find_by_name(rpmts ts, const char *name, const char *arch,
			Header *header, psys_err_t *err)
{
	unsigned int offset;

	offset = find_db_record(ts, RPMTAG_NAME, name, arch, header, err);
	if (offset == UINT_MAX && psys_err_code(*err) == PSYS_ENOENT) {
		psys_err_free(*err);
		psys_err_set(err, PSYS_ENOENT,
			     "No package named `%s' is installed",
			     name);
	}
	return offset;
}

static int find_by_provide_name(rpmts ts, const char *name, const char *arch,
				Header *header, psys_err_t *err)
{
	unsigned int offset;

	offset = find_db_record(ts, RPMTAG_PROVIDENAME, name, arch, header,
			        err);
	if (offset == UINT_MAX && psys_err_code(*err) == PSYS_ENOENT) {
		psys_err_free(*err);
		psys_err_set(err, PSYS_ENOENT,
			     "No installed package provides `%s'",
			     name);
	}
	return offset;
}

static char *canonicalize_path(const char *path, psys_err_t *err)
{
	char *rpath;

	rpath = realpath(path, NULL);
	if (!rpath) {
		psys_err_set(err, PSYS_EINTERNAL,
			     "Cannot canonicalize path `%s': %s",
			     path, strerror(errno));
		return NULL;
	}
	return rpath;
}

/*** Sanity checks ************************************************************/

static int ensure_not_installed(rpmts ts, const char *name, psys_err_t *err)
{
	if (rpmdbCountPackages(rpmtsGetRdb(ts), name) > 0) {
		psys_err_set(err, PSYS_EEXIST,
			     "A package named `%s' already exists in the "
			     "package database",
			     name);
		return -1;
	}
	return 0;
}

static int ensure_dependencies_installed(rpmts ts, psys_pkg_t pkg,
					 psys_err_t *err)
{
	const char *rpmarch;
	Header h;
	char *version;

	rpmarch = rpm_arch(psys_pkg_arch(pkg), err);
	if (!rpmarch)
		return -1;

	/*
	 * If the package is architecture-independent, we don't care which
	 * architecture the "lsb" package has
	 */
	if (!strcmp(rpmarch, "noarch"))
		rpmarch = NULL;

	if (find_by_provide_name(ts, "lsb", rpmarch, &h, err) == UINT_MAX) {
		if (psys_err_code(*err) == PSYS_ENOENT) {
			psys_err_t tmp;
			tmp = *err;
			psys_err_set(err, PSYS_ELSBVER,
				     "The system does not seem to comply to "
				     "the Linux Standard Base (LSB) standard: "
				     "%s",
				     psys_err_msg(tmp));
			psys_err_free(tmp);
		}
		return -1;
	}

	if (!headerGetEntry(h, RPMTAG_VERSION, NULL, (void **) &version,
			    NULL)) {
		psys_err_set(err, PSYS_EINTERNAL,
			     "Error in headerGetEntry() (RPMTAG_VERSION)");
		return -1;
	}

	if (psys_pkg_lsbvercmp(pkg, version) < 0) {
		psys_err_set(err, PSYS_ELSBVER,
			     "The system does not comply to a new-enough "
			     "version of the Linux Standard Base (LSB) "
			     "standard (version %s of package `lsb' is "
			     "installed, while %s is required)",
			     version, psys_pkg_lsbversion(pkg));
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

static int ensure_no_new_conflicting_extras(psys_pkg_t pkg, Header installed,
					    psys_err_t *err)
{
	int ret;
	char **basenames = NULL;
	char **dirnames = NULL;
	int *dirindexes;
	unsigned int basenames_cnt, dirnames_cnt, dirindexes_cnt;
	psys_plist_t p;

	if (!headerGetEntry(installed, RPMTAG_BASENAMES, NULL,
			    (void **) &basenames, &basenames_cnt)) {
		psys_err_set(err, PSYS_EINTERNAL,
			     "Error in headerGetEntry() (RPMTAG_BASENAMES)");
		ret = -1;
		goto out;
	}
	if (!headerGetEntry(installed, RPMTAG_DIRNAMES, NULL,
			    (void **) &dirnames, &dirnames_cnt)) {
		psys_err_set(err, PSYS_EINTERNAL,
			     "Error in headerGetEntry() (RPMTAG_DIRNAMES)");
		ret = -1;
		goto out;
	}
	if (!headerGetEntry(installed, RPMTAG_DIRINDEXES, NULL,
			    (void **) &dirindexes, &dirindexes_cnt)) {
		psys_err_set(err, PSYS_EINTERNAL,
			     "Error in headerGetEntry() (RPMTAG_DIRINDEXES)");
		ret = -1;
		goto out;
	}	

	if (basenames_cnt > dirindexes_cnt) {
		psys_err_set(err, PSYS_EINTERNAL,
			     "Malformed RPM header: more BASENAMES than "
			     "DIRINDEXES");
		ret = -1;
		goto out;
	}

	for (p = psys_pkg_extras(pkg); p; p = psys_plist_next(p)) {
		int i;

		/*
		 * Check if the extra file is already part of the installed
		 * package's version. If so, everything is fine.
		 */
		for (i = 0; i < basenames_cnt; i++) {
			char path[PATH_MAX];
			char *cpath1, *cpath2;
			int diff;

			if (dirindexes[i] >= dirnames_cnt) {
				psys_err_set(err, PSYS_EINTERNAL,
					     "Malformed RPM header: "
					     "out-of-bounds DIRINDEX");
				ret = -1;
				goto out;
			}

			snprintf(path, PATH_MAX, "%s/%s",
				 dirnames[dirindexes[i]], basenames[i]);

			cpath1 = canonicalize_path(psys_plist_path(p), err);
			if (!cpath1) {
				ret = -1;
				goto out;		
			}
			cpath2 = canonicalize_path(path, err);
			if (!cpath2) {
				free(cpath1);
				ret = -1;
				goto out;		
			}

			diff = strcmp(cpath1, cpath2);
			free(cpath1);
			free(cpath2);
			if (diff == 0)
				goto nextfile;
		}
		
		/*
		 * If the extra file is new to this version of the package,
		 * check that it does not exist yet.
		 */
		if (ensure_no_conflict(p, err))
			return -1;
nextfile:
		continue;
	}

	ret = 0;
out:
	if (dirnames)
		free(dirnames);
	if (basenames)
		free(basenames);
	return ret;
}

static int ensure_version_newer(psys_pkg_t pkg, Header installed,
				psys_err_t *err)
{
	const char *version;
	int diff;

	if (!headerGetEntry(installed, RPMTAG_VERSION, NULL,
			    (void **) &version, NULL)) {
		psys_err_set(err, PSYS_EINTERNAL,
			     "Error in headerGetEntry() (RPMTAG_VERSION)");
		return -1;
	}

	diff = psys_pkg_vercmp(pkg, version);
	if (diff < 0) {
		psys_err_set(err, PSYS_EVER,
			     "Installed package is newer than version %s (%s)",
			     psys_pkg_version(pkg), version);
		return -1;
	} else if (diff == 0) {
		psys_err_set(err, PSYS_EVER,
			     "Package is already at version %s",
			     version);
		return -1;
	} else {
		return 0;
	}
}

/*** Adding package metadata **************************************************/

static void add_i18n_entry(Header header, int tag, psys_tlist_t l)
{
	for (; l; l = psys_tlist_next(l)) {
		headerAddI18NString(header, tag, psys_tlist_value(l),
				    psys_tlist_locale(l));
	}
}

static void add_dependency_entries(Header header, psys_pkg_t pkg) {
	const char *val_str;
	int val_i32;

	/* REQUIRENAME */
	val_str = "lsb";
	headerAddEntry(header, RPMTAG_REQUIRENAME, RPM_STRING_ARRAY_TYPE,
		       &val_str, 1);

        /* REQUIREFLAGS */
	val_i32 = RPMSENSE_EQUAL | RPMSENSE_GREATER;
	headerAddEntry(header, RPMTAG_REQUIREFLAGS, RPM_INT32_TYPE,
		       &val_i32, 1);

	/* REQUIREVER */
	val_str = psys_pkg_lsbversion(pkg);
	headerAddEntry(header, RPMTAG_REQUIREVERSION, RPM_STRING_ARRAY_TYPE,
		       &val_str, 1);
}

/*** Adding file metadata *****************************************************/

static int get_dirindex(Header header, const char *dir)
{
	int_32 dirindex;
	uint_32 dircount;
	char **dirnames;

	dirindex = -1;
	dircount = 0;
	dirnames = NULL;

	if (headerGetEntry(header, RPMTAG_DIRNAMES, NULL, (void **) &dirnames,
		           &dircount)) {
		int i;
		for (i = 0; i < dircount; i++) {
			if (!strcmp(dirnames[i], dir))
				dirindex = i;
		}
	}

	if (dirindex == -1) {
		const char *dirvar;

		/* DIRNAMES */
		dirvar = dir;
		headerAddOrAppendEntry(header, RPMTAG_DIRNAMES,
				       RPM_STRING_ARRAY_TYPE, &dirvar, 1);

		dirindex = dircount;
	}

	assert(dirindex >= 0);
	return dirindex;
}

static int add_filename_entries(Header header, psys_flist_t file,
				psys_err_t *err)
{
	int ret;
	const char *path;
	char *pathc1, *pathc2;
	char *base, *dir;
	int dirindex;

	path = psys_flist_path(file);
	pathc1 = strdup(path);
	pathc2 = strdup(path);
	if (!pathc1 || !pathc2) {
		psys_err_set_nomem(err);
		ret = -1;
		goto out;
	}

	base = basename(pathc1);
	dir = NULL;
	if (asprintf(&dir, "%s/", dirname(pathc2)) < 0) {
		psys_err_set_nomem(err);
		ret = -1;
		goto out;
	}

	dirindex = get_dirindex(header, dir);

	/* BASENAMES */
	headerAddOrAppendEntry(header, RPMTAG_BASENAMES, RPM_STRING_ARRAY_TYPE,
			       &base, 1);

	/* DIRINDEXES */
	headerAddOrAppendEntry(header, RPMTAG_DIRINDEXES, RPM_INT32_TYPE,
			       &dirindex, 1);
	ret = 0;

out:
	if (pathc1)
		free(pathc1);
	if (pathc2)
		free(pathc2);
	return ret;
}

static int add_fileowner_entries(Header header, psys_flist_t file,
				 psys_err_t *err)
{
	const struct stat *st;
	struct passwd *usr;
	struct group *grp;

	st = psys_flist_stat(file);

	usr = getpwuid(st->st_uid);
	if (!usr) {
		psys_err_set(err, PSYS_EINTERNAL,
			     "Cannot get user name from uid of file `%s'",
			     psys_flist_path(file));
		return -1;
	}

	grp = getgrgid(st->st_gid);
	if (!grp) {
		psys_err_set(err, PSYS_EINTERNAL,
			     "Cannot get group name from gid of file `%s'",
			     psys_flist_path(file));
		return -1;
	}

	/* FILEUSERNAME */
	headerAddOrAppendEntry(header, RPMTAG_FILEUSERNAME,
			       RPM_STRING_ARRAY_TYPE, &usr->pw_name, 1);

	/* FILEGROUPNAME */
	headerAddOrAppendEntry(header, RPMTAG_FILEGROUPNAME,
			       RPM_STRING_ARRAY_TYPE, &grp->gr_name, 1);

	return 0;
}

static int add_linkto_entry(Header header, psys_flist_t file,
			    psys_err_t *err)
{
	char *linkto;

	if (S_ISLNK(psys_flist_stat(file)->st_mode)) {
		const char *path;
		int size;

		path = psys_flist_path(file);
		size = 64;

		linkto = malloc(size);
		if (!linkto) {
			psys_err_set_nomem(err);
			return -1;
		}

		while (1) {
			int nchars;

			nchars = readlink(path, linkto, size);
			if (nchars < size) {
				linkto[nchars] = '\0';
				break;
			} else if (nchars < 0) {
				psys_err_set(err, PSYS_EINTERNAL,
					     "Cannot read symbolic link `%s': "
					     "%s",
					     path, strerror(errno));
				free(linkto);
				return -1;
			} else {
				size *= 2;
				linkto = realloc(linkto, size);
				if (!linkto) {
					psys_err_set_nomem(err);
					return -1;
				}
			}
		}
	} else {
		linkto = "";
	}

	/* FILELINKTOS */
	headerAddOrAppendEntry(header, RPMTAG_FILELINKTOS,
			       RPM_STRING_ARRAY_TYPE, &linkto, 1);

	if (strlen(linkto) > 0)
		free(linkto);

	return 0;
}

static int add_md5_entry(Header header, psys_flist_t file,
			 psys_err_t *err)
{
	char *md5;

	md5 = psys_flist_md5sum(file, err);
	if (!md5)
		return -1;

	/* FILEMD5S */
	headerAddOrAppendEntry(header, RPMTAG_FILEMD5S,
			       RPM_STRING_ARRAY_TYPE, &md5, 1);
	return 0;
}

static int add_file_metadata(Header header, psys_flist_t list,
			     psys_err_t *err)
{
	psys_flist_t f;

	for (f = list; f; f = psys_flist_next(f)) {
		const struct stat *st;
		int_32 flags;
		const char *lang;

		if (add_filename_entries(header, f, err) ||
		    add_fileowner_entries(header, f, err) ||
		    add_linkto_entry(header, f, err) ||
		    add_md5_entry(header, f, err)) {
			return -1;
		}

		st = psys_flist_stat(f);

		/* FILESIZES */
		headerAddOrAppendEntry(header, RPMTAG_FILESIZES,
				       RPM_INT32_TYPE, &st->st_size, 1);

		/* FILEMODES */
		headerAddOrAppendEntry(header, RPMTAG_FILEMODES,
				       RPM_INT16_TYPE, &st->st_mode, 1);

		/* FILEMTIMES */
		headerAddOrAppendEntry(header, RPMTAG_FILEMTIMES,
				       RPM_INT32_TYPE, &st->st_mtime, 1);

		/* FILEDEVICES, FILERDEVS*/
		headerAddOrAppendEntry(header, RPMTAG_FILEDEVICES,
				       RPM_INT16_TYPE, &st->st_dev, 1);
		headerAddOrAppendEntry(header, RPMTAG_FILERDEVS,
				       RPM_INT16_TYPE, &st->st_rdev, 1);

		/* FILEINODES */
		headerAddOrAppendEntry(header, RPMTAG_FILEINODES,
				       RPM_INT32_TYPE, &st->st_ino, 1);

		/* FILEFLAGS */
		flags = RPMFILE_GHOST;
		headerAddOrAppendEntry(header, RPMTAG_FILEFLAGS,
				       RPM_INT32_TYPE, &flags, 1);

		/* FILELANGS */
		lang = "C";
		headerAddOrAppendEntry(header, RPMTAG_FILELANGS,
				       RPM_STRING_ARRAY_TYPE, &lang, 1);
	}

	return 0;
}

/*** psys_announce() **********************************************************/

int rpm_psys_announce(psys_pkg_t pkg, psys_err_t *err)
{
	int ret;
	rpmts ts;
	char *rpmname = NULL;

	pkg = psys_pkg_copy(pkg);
	if (!pkg) {
		psys_err_set_nomem(err);
		return -1;
	}
	psys_pkg_assert_valid(pkg);

	ts = create_transaction_set(err);
	if (!ts) {
		ret = -1;
		goto out;
	}

	rpmname = rpm_name(psys_pkg_vendor(pkg), psys_pkg_name(pkg));
	if (!rpmname) {
		psys_err_set_nomem(err);
		ret = -1;
		goto out;
	}

	if (ensure_not_installed(ts, rpmname, err)) {
		ret = -1;
		goto out;
	}

	if (ensure_dependencies_installed(ts, pkg, err)) {
		ret = -1;
		goto out;
	}

	if (ensure_no_conflicting_extras(pkg, err)) {
		ret = -1;
		goto out;
	}

	ret = 0;
out:
	if (rpmname)
		free(rpmname);
	if (ts)
		rpmtsFree(ts);
	psys_pkg_free(pkg);
	return ret;
}

/*** psys_register() **********************************************************/

static int do_register(rpmts ts, psys_pkg_t pkg, psys_err_t *err)
{
	int ret;
	char *rpmname = NULL;
	const char *rpmarch;
	Header header = NULL;
	psys_flist_t flist = NULL, f;
	int_32 val_i32;

	rpmname = rpm_name(psys_pkg_vendor(pkg), psys_pkg_name(pkg));
	if (!rpmname) {
		psys_err_set_nomem(err);
		ret = -1;
		goto out;
	}

	rpmarch = rpm_arch(psys_pkg_arch(pkg), err);
	if (!rpmarch) {
		ret = -1;
		goto out;
	}

	if (ensure_not_installed(ts, rpmname, err)) {
		ret = -1;
		goto out;
	}

	if (ensure_dependencies_installed(ts, pkg, err)) {
		ret = -1;
		goto out;
	}

	header = headerNew();

	/* NAME */
	headerAddEntry(header, RPMTAG_NAME, RPM_STRING_TYPE, rpmname, 1);
	free(rpmname);
	rpmname = NULL;

	/* EPOCH */
	val_i32 = 0;
	headerAddEntry(header, RPMTAG_EPOCH, RPM_INT32_TYPE, &val_i32, 1);

	/* VERSION, RELEASE */
	headerAddEntry(header, RPMTAG_VERSION, RPM_STRING_TYPE,
		       psys_pkg_version(pkg), 1);
	headerAddEntry(header, RPMTAG_RELEASE, RPM_STRING_TYPE, "1", 1);

	/* OS, ARCH */
	headerAddEntry(header, RPMTAG_OS, RPM_STRING_TYPE, "linux", 1);
	headerAddEntry(header, RPMTAG_ARCH, RPM_STRING_TYPE, rpmarch, 1);

	/* VENDOR */
	headerAddEntry(header, RPMTAG_VENDOR, RPM_STRING_TYPE,
		       psys_pkg_vendor(pkg), 1);

	/* SUMMARY, DESCRIPTION */
	add_i18n_entry(header, RPMTAG_SUMMARY,
			    psys_pkg_summary(pkg));
	add_i18n_entry(header, RPMTAG_DESCRIPTION,
			    psys_pkg_description(pkg));

	/* GROUP */
	headerAddEntry(header, RPMTAG_GROUP, RPM_STRING_TYPE, "", 1);

	/* Dependencies */
	add_dependency_entries(header, pkg);

	flist = psys_pkg_flist(pkg, err);
	if (!flist) {
		ret = -1;
		goto out;
	}

	val_i32 = 0;
	for (f = flist; f; f = psys_flist_next(f)) {
		val_i32 += psys_flist_stat(f)->st_size;
	}
	/* SIZE */
	headerAddEntry(header, RPMTAG_SIZE, RPM_INT32_TYPE, &val_i32, 1);

	/* File metadata */
	if (add_file_metadata(header, flist, err)) {
		ret = -1;
		goto out;
	}

	/* INSTALLTIME */
	val_i32 = rpmtsGetTid(ts);
	headerAddEntry(header, RPMTAG_INSTALLTIME, RPM_INT32_TYPE,
		       &val_i32, 1);

	if (rpmdbAdd(rpmtsGetRdb(ts), rpmtsGetTid(ts), header, ts, NULL)) {
		psys_err_set(err,PSYS_EINTERNAL,
			     "Adding package to RPM database failed");

		ret = -1;
		goto out;
	}

	ret = 0;
out:
	if (flist)
		psys_flist_free(flist);
	if (header)
		headerFree(header);
	if (rpmname)
		free(rpmname);
	return ret;
}

int rpm_psys_register(psys_pkg_t pkg, psys_err_t *err)
{
	rpmts ts = NULL;
	int ret;

	pkg = psys_pkg_copy(pkg);
	if (!pkg) {
		psys_err_set_nomem(err);
		return -1;
	}
	psys_pkg_assert_valid(pkg);

	ts = create_transaction_set(err);
	if (!ts) {
		psys_pkg_free(pkg);
		return -1;
	}
	
	ret = do_register(ts, pkg, err);
	rpmtsFree(ts);
	psys_pkg_free(pkg);
	return ret;
}

/*** psys_announce_update() ***************************************************/

int rpm_psys_announce_update(psys_pkg_t pkg, psys_err_t *err)
{
	int ret;
	rpmts ts = NULL;
	char *rpmname = NULL;
	const char *rpmarch;
	unsigned int recoffset;
	Header header = NULL;

	pkg = psys_pkg_copy(pkg);
	if (!pkg) {
		psys_err_set_nomem(err);
		return -1;
	}
	psys_pkg_assert_valid(pkg);

	ts = create_transaction_set(err);
	if (!ts) {
		ret = -1;
		goto out;
	}

	rpmname = rpm_name(psys_pkg_vendor(pkg), psys_pkg_name(pkg));
	if (!rpmname) {
		psys_err_set_nomem(err);
		ret = -1;
		goto out;
	}

	rpmarch = rpm_arch(psys_pkg_arch(pkg), err);
	if (!rpmarch) {
		ret = -1;
		goto out;
	}

	recoffset = find_by_name(ts, rpmname, rpmarch, &header, err);
	if (recoffset == UINT_MAX) {
		ret = -1;
		goto out;
	}

	if (ensure_version_newer(pkg, header, err)) {
		ret = -1;
		goto out;
	}

	if (ensure_dependencies_installed(ts, pkg, err)) {
		ret = -1;
		goto out;
	}

	if (ensure_no_new_conflicting_extras(pkg, header, err)) {
		ret = -1;
		goto out;
	}

	ret = 0;
out:
	if (header)
		headerFree(header);
	if (rpmname)
		free(rpmname);
	if (ts)
		rpmtsFree(ts);
	psys_pkg_free(pkg);
	return ret;
}

/*** psys_register_update() ***************************************************/

int rpm_psys_register_update(psys_pkg_t pkg, psys_err_t *err)
{
	int ret;
	rpmts ts = NULL;
	char *rpmname = NULL;
	const char *rpmarch;
	unsigned int recoffset;
	Header header = NULL;

	pkg = psys_pkg_copy(pkg);
	if (!pkg) {
		psys_err_set_nomem(err);
		return -1;
	}
	psys_pkg_assert_valid(pkg);
	pkg = psys_pkg_copy(pkg);
	if (!pkg) {
		psys_err_set_nomem(err);
		return -1;
	}
	psys_pkg_assert_valid(pkg);

	ts = create_transaction_set(err);
	if (!ts) {
		ret = -1;
		goto out;
	}

	rpmname = rpm_name(psys_pkg_vendor(pkg), psys_pkg_name(pkg));
	if (!rpmname) {
		psys_err_set_nomem(err);
		ret = -1;
		goto out;
	}

	rpmarch = rpm_arch(psys_pkg_arch(pkg), err);
	if (!rpmarch) {
		ret = -1;
		goto out;
	}

	recoffset = find_by_name(ts, rpmname, rpmarch, &header, err);
	if (recoffset == UINT_MAX) {
		ret = -1;
		goto out;
	}

	if (ensure_version_newer(pkg, header, err)) {
		ret = -1;
		goto out;
	}

	if (ensure_dependencies_installed(ts, pkg, err)) {
		ret = -1;
		goto out;
	}

	rpmdbRemove(rpmtsGetRdb(ts), 0, recoffset, ts, NULL);
	ret = 0;
out:
	if (header)
		headerFree(header);
	if (rpmname)
		free(rpmname);

	if (ret == 0)
		ret = do_register(ts, pkg, err);

	if (ts)
		rpmtsFree(ts);
	psys_pkg_free(pkg);

	return ret;
}

/*** psys_unannounce() ***************************************************/

int rpm_psys_unannounce(const char *vendor, const char *name,
			psys_err_t *err)
{
	rpmts ts;
	char *rpmname;
	unsigned int recoffset;

	ts = create_transaction_set(err);
	if (!ts)
		return -1;

	rpmname = rpm_name(vendor, name);
	if (!rpmname) {
		psys_err_set_nomem(err);
		rpmtsFree(ts);
		return -1;
	}

	recoffset = find_by_name(ts, rpmname, NULL, NULL, err);
	free(rpmname);
	rpmtsFree(ts);
	return (recoffset == UINT_MAX) ? -1 : 0;
}

/*** psys_unregister() ***************************************************/

int rpm_psys_unregister(const char *vendor, const char *name,
			psys_err_t *err)
{
	char *rpmname;
	rpmts ts;

	ts = create_transaction_set(err);
	if (!ts)
		return -1;

	rpmname = rpm_name(vendor, name);
	if (!rpmname) {
		psys_err_set_nomem(err);
		rpmtsFree(ts);
		return -1;
	}

	while (1) {
		unsigned int recoffset;

		recoffset = find_by_name(ts, rpmname, NULL, NULL, err);
		if (recoffset == UINT_MAX) {
			free(rpmname);
			rpmtsFree(ts);
			return 0;
		}
		rpmdbRemove(rpmtsGetRdb(ts), 0, recoffset, ts, NULL);
	}
}

