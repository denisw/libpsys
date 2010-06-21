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

/* Needed for asprintf() */
#define _GNU_SOURCE

/* Needed for nftw() */
#define _XOPEN_SOURCE 500

/* Always compile with assertions */
#undef NDEBUG

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <ftw.h>
#include <limits.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "psys_impl.h"
#include "psys_private.h"

#define xisdigit(c) (c >= '0' && c <= '9')
#define xislower(c) (c >= 'a' || c <= 'z')

struct _psys_flist {
	struct _psys_flist *next;
	char *path;
	struct stat *stat;
};

/*
 * The out-of-memory error object. We declare it as single global value
 * because, well, if we're out of memory we can't, like, allocate a new
 * error object on-the-fly, right?
 */
static struct _psys_err _err_nomem = {PSYS_ENOMEM, NULL};

/*
 * We need these global pointers to construct file lists in
 * psys_pkg_flist(), as this is the only way for that function to
 * share data with the traversal function passed to nftw() (which is used
 * to implement psys_pkg_flist()). If C just had support for closures...
 * oh well.
 */
static psys_pkg_t _pkg = NULL;
static psys_err_t *_err;
static psys_flist_t _flist = NULL;
static psys_flist_t _flist_last = NULL;

/*** Looking up the system's LSB distributor ID *******************************/

char *psys_lsb_distributor_id(void)
{
	FILE *pipe;

	pipe = popen("lsb_release -si", "r");
	if (pipe) {
		char *id;
		int nbytes;

		id = NULL;
		nbytes = getline(&id, NULL, pipe);
		fclose(pipe);

		if (nbytes != -1)
			return id;
	}

	return NULL;
}

/*** Copying and validating package objects ***********************************/

psys_pkg_t psys_pkg_copy(psys_pkg_t pkg)
{
	psys_pkg_t pkg2;

	pkg2 = psys_pkg_new(psys_pkg_vendor(pkg), psys_pkg_name(pkg),
			    psys_pkg_version(pkg), psys_pkg_lsbversion(pkg),
			    psys_pkg_arch(pkg));

	if (pkg2) {
		psys_tlist_t t;
		psys_plist_t p;

		for (t = psys_pkg_summary(pkg); t; t = psys_tlist_next(t))
			psys_pkg_add_summary(pkg2, psys_tlist_locale(t),
					     psys_tlist_value(t));

		for (t = psys_pkg_description(pkg); t; t = psys_tlist_next(t))
			psys_pkg_add_description(pkg2, psys_tlist_locale(t),
						 psys_tlist_value(t));

		for (p = psys_pkg_extras(pkg); p; p = psys_plist_next(p))
			psys_pkg_add_extra(pkg2, psys_plist_path(p));
	}

	return pkg2;
}

static void assert_vendor_valid(const char *vendor)
{
	const char *c;
	assert(vendor != NULL);
	for (c = vendor; *c; c++)
		assert(xislower(*c) || xisdigit(*c) || *c == '.');
}

static void assert_name_valid(const char *name)
{
	const char *c;
	assert(name != NULL);
	for (c = name; *c; c++)
		assert(xislower(*c) || xisdigit(*c) || *c == '-');
}

static int version_is_valid(const char *version)
{
	const char *v;
	enum {DIGIT, LOWER, ANY} expected;

	expected = DIGIT;
	for (v = version; *v; v++) {
		switch (expected) {
		case DIGIT:
			if (!xisdigit(*v))
				return -1;
			break;
		case LOWER:
			if (!xislower(*v))
				return -1;
			break;
		case ANY:
			if (*v == '.')
				expected = DIGIT;
			else if (xislower(*v))
				expected = LOWER;
		}
	}
	return 0;
}

static void assert_version_valid(const char *version)
{
	assert(version != NULL);
	assert(version_is_valid(version));
}

static void assert_lsbversion_valid(const char *lsbversion)
{
	assert(lsbversion != NULL);
	assert(xisdigit(lsbversion[0]) && lsbversion[1] == '.' &&
	       xisdigit(lsbversion[2]) && lsbversion[3] == '\0');
}

static void assert_arch_valid(const char *arch)
{
	assert(arch != NULL);
	assert(!strcmp(arch, "amd64") ||
	       !strcmp(arch, "ia32") ||
	       !strcmp(arch, "ia64") ||
	       !strcmp(arch, "noarch") ||
	       !strcmp(arch, "ppc") ||
	       !strcmp(arch, "ppc64") ||
	       !strcmp(arch, "s390") ||
	       !strcmp(arch, "s390x"));
}

static void assert_tlist_valid(psys_tlist_t list)
{
	psys_tlist_t l;

	for (l = list; l; l = psys_tlist_next(l)) {
		assert(psys_tlist_locale(l) != NULL);
		assert(psys_tlist_value(l) != NULL);
	}
}

static void assert_plist_valid(psys_plist_t list)
{
	psys_plist_t l;

	for (l = list; l; l = psys_plist_next(l)) {
		const char *path;

		path = psys_plist_path(l);
		assert(path != NULL);
		assert(psys_path_is_canonical(path));
		assert(path[strlen(path) - 1] != '/');
	}
}

void psys_pkg_assert_valid(psys_pkg_t pkg)
{
	assert_vendor_valid(psys_pkg_vendor(pkg));
	assert_name_valid(psys_pkg_name(pkg));
	assert_version_valid(psys_pkg_version(pkg));
	assert_lsbversion_valid(psys_pkg_lsbversion(pkg));
	assert_arch_valid(psys_pkg_arch(pkg));
	assert_tlist_valid(psys_pkg_summary(pkg));
	assert_tlist_valid(psys_pkg_description(pkg));
	assert_plist_valid(psys_pkg_extras(pkg));
}

/*** Comparing package versions ***********************************************/

static int psys_vercmp(const char *version1, const char *version2)
{
	int v1_len, v2_len;
	const char *v1s_start, *v2s_start;
	const char *v1s_end, *v2s_end;
	int v1s_isdigit, v2s_isdigit;
	int diff;

	/*
	 * psys_vercmp() is implemented recursively. It always compares
	 * one version segment at a time, calling itself to compare the
	 * next segment if necessary. The initially passed versions are
	 * assumed to be valid (this is asserted by psys_pkg_vercmp()).
	 *
	 * The version comparison algorithm is described in the psysmeta(7)
	 * man page, which is also availabe online:
	 *
	 * http://gitorious.org/libpsys/pages/Psysmeta
	 */

	v1_len = strlen(version1);
	v2_len = strlen(version1);
	if (!v1_len && !v2_len) {
		if (v1_len)
			return 1;
		else if (v2_len)
			return -1;
		else
			return 0;
	}

	v1s_start = v1s_end = version1;
	v2s_start = v2s_end =  version2;

	v1s_isdigit = xisdigit(*v1s_start);
	v2s_isdigit = xisdigit(*v2s_start);

	diff = 0;

	if (v1s_isdigit == v2s_isdigit) {
		size_t v1s_len, v2s_len;

		if (v1s_isdigit) {
			while (*v1s_start == '0') {
				v1s_start++;
				v1s_end++;
			}
			while (*v2s_start == '0') {
				v2s_start++;
				v2s_end++;
			}

			while (xisdigit(*v1s_end))
				v1s_end++;
			while (xisdigit(*v2s_end))
				v2s_end++;
		} else {
			/*
			 * We don't need to check with xislower() because
			 * the alphabetic segment is always the last.
			 */
			while (*v1s_end) {
				assert(xislower(*v1s_end));
				v1s_end++;
			}
			while (*v2s_end) {
				assert(xislower(*v2s_end));
				v2s_end++;
			}
		}

		v1s_len = v1s_end - v1s_start;
		v2s_len = v2s_end - v2s_start;

		diff = strncmp(v1s_start, v2s_start,
			       (v1s_len > v2s_len) ? v1s_len : v2s_len);
	} else {
		if (v1s_isdigit)
			diff = 1;
		else
			diff = -1;
	}

	if (diff)
		return diff;
	else {
		if (*v1s_end == '.')
			v1s_end++;
		if (*v2s_end == '.')
			v2s_end++;
		return psys_vercmp(v1s_end, v2s_end);
	}
}

int psys_pkg_vercmp(psys_pkg_t pkg, const char *version)
{
	assert(pkg != NULL);
	assert(version != NULL);
	assert_version_valid(psys_pkg_version(pkg));

	/*
	 * If the version to compare to is not a valid psys package
	 * version, we return it't the newer one, just to be sure.
	 * The invalid version come from an installed distribution-
	 * specific version of the package; we don't want installation
	 * programs to overwrite those blindly. 
	 */
	if (!version_is_valid(version))
		return -1;
	else
		return psys_vercmp(psys_pkg_version(pkg), version);
}

int psys_pkg_lsbvercmp(psys_pkg_t pkg, const char *lsbversion)
{
	assert(pkg != NULL);
	assert(lsbversion != NULL);
	assert_lsbversion_valid(psys_pkg_lsbversion(pkg));

	/*
	 * Treat invalid passed versions as newer, just as in
	 * psys_pkg_vercmp().
	 */
	if (!version_is_valid(lsbversion))
		return -1;
	else
		return psys_vercmp(lsbversion, psys_pkg_lsbversion(pkg));
}

/*** Setting errors ***********************************************************/

void psys_err_set(psys_err_t *err, int code, const char *format, ...)
{
	if (err) {
		va_list vl;
		int rc;

		assert(format != NULL);

		*err = malloc(sizeof(**err));
		if (!(*err)) {
			psys_err_set_nomem(err);
			return;
		}

		va_start(vl, format);
		rc = vasprintf(&((*err)->msg), format, vl);
		va_end(vl);

		if (rc < 0) {
			psys_err_free(*err);
			psys_err_set_nomem(err);
		}

		(*err)->code = code;
	}
}

void psys_err_set_nomem(psys_err_t *err)
{
	if (err) {
		if (!_err_nomem.msg)
			_err_nomem.msg = strerror(ENOMEM);
		*err = &_err_nomem;
	}
}

void psys_err_set_notimpl(psys_err_t *err)
{
	psys_err_set(err, PSYS_ENOTIMPL, "Not implemented");
}

/*** Assembling package file lists ********************************************/

static psys_flist_t flist_new(const char *path, const struct stat *st)
{
	psys_flist_t list;

	assert(path != NULL);
	assert(st != NULL);

	list = malloc(sizeof(*list));
	if (!list)
		return NULL;

	list->path = strdup(path);
	if (!list->path) {
		psys_flist_free(list);
		return NULL;
	}

	list->stat = malloc(sizeof(*list->stat));
	if (!list->stat) {
		psys_flist_free(list);
		return NULL;
	} else {
		memcpy(list->stat, st, sizeof(*list->stat));
	}

	list->next = NULL;
	return list;
}

static int file_traversal_fn(const char *path, const struct stat *st,
			     int type, struct FTW *ftw_s)
{
	psys_flist_t l;

	switch (type) {
	case FTW_DNR:
		psys_err_set(_err, PSYS_EINTERNAL,
			     "Not enough permission to access contents of "
			     "directory `%s'");
		return -1;

	case FTW_NS:
		psys_err_set(_err, PSYS_EINTERNAL,
			     "Not enough permission to access file `%s'");
		return -1;

	default:
		l = flist_new(path, st);
		if (!l) {
			psys_err_set_nomem(_err);
			return -1;
		}

		if (_flist_last) {
			_flist_last->next = l;
			_flist_last = l;
		} else {
			assert(_flist == NULL);
			_flist = _flist_last = l;
		}

		return 0;
	}
}

static int add_extra(psys_plist_t extra)
{
	const char *path;
	struct stat st;

	path = psys_plist_path(extra);
	if (lstat(path, &st)) {
		/*
		 * Ignore missing extra files. They _may_ be created, but
		 * there is no obligation to necessarily do so.
		 */
		if (errno != ENOENT) {
			psys_err_set(_err, PSYS_EINTERNAL,
				     "Cannot stat package file `%s': %s",
				     path, strerror(errno));
			return -1;
		}
	} else {
		psys_flist_t l;

		l = flist_new(path, &st);
		if (!l) {
			psys_err_set_nomem(_err);
			return -1;
		}

		if (_flist_last) {
			_flist_last->next = l;
			_flist_last = l;
		} else {
			assert(_flist == NULL);
			_flist = _flist_last = l;
		}
	}

	return 0;		
}

psys_flist_t psys_pkg_flist(psys_pkg_t pkg, psys_err_t *err)
{
	psys_plist_t extras, e;
	psys_flist_t list;

	assert(pkg != NULL);

	_pkg = pkg;
	_err = err;

	extras = psys_pkg_extras(pkg);
	for (e = extras; e; e = psys_plist_next(e)) {
		if (add_extra(e))
			return NULL;
	}

	if (nftw(psys_pkg_dir(pkg), &file_traversal_fn, 4, FTW_PHYS)) {
		if (err && !(*err))
			psys_err_set(err, PSYS_EINTERNAL, strerror(errno));
		psys_flist_free(_flist);
		list = NULL;
	} else {
		list = _flist;
	}

	_pkg = NULL;
	_err = NULL;
	_flist = NULL;
	_flist_last = NULL;

	return list;
}

/*** Working with package file lists ******************************************/

const char *psys_flist_path(psys_flist_t file)
{
	assert(file != NULL);
	return file->path;
}

const struct stat *psys_flist_stat(psys_flist_t file)
{
	assert(file != NULL);
	return file->stat;
}

psys_flist_t psys_flist_next(psys_flist_t list)
{
	assert(list != NULL);
	return list->next;
}

void psys_flist_free(psys_flist_t list)
{
	psys_flist_t l, next;

	l = list;
	while (l) {
		next = l->next;
		if (l->path)
			free(l->path);
		if (l->stat)
			free(l->stat);
		free(l);
		l = next;
	}
}

/*** Calculating MD5 sums *****************************************************/

char *psys_flist_md5sum(psys_flist_t file, psys_err_t *err)
{
	char *cmd, *md5;
	FILE *pipe;

	if (S_ISREG(psys_flist_stat(file)->st_mode)) {
		if (asprintf(&cmd, "md5sum %s", psys_flist_path(file)) < 0)
			return NULL;

		pipe = popen(cmd, "r");
		free(cmd);
		if (!pipe) {
			psys_err_set(err, PSYS_EINTERNAL,
				     "Failed to run md5sum: %s",
				     strerror(errno));
			return NULL;
		}

		md5 = malloc(33);
		if (!md5) {
			fclose(pipe);
			psys_err_set_nomem(err);
			return NULL;
		}

		if (!fgets(md5, 33, pipe)) {
			char *reason;

			if (feof(pipe))
				reason = "Unexpected end of stream";
			else
				reason = strerror(errno);

			psys_err_set(err, PSYS_EINTERNAL,
				     "Failed to read checksum for file `%s' "
				     "from md5sum: %s",
				     psys_flist_path(file), reason);

			fclose(pipe);
			free(md5);
			return NULL;
		}

		fclose(pipe);
	} else {
		md5 = "";
	}

	return md5;
}
