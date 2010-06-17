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

/* Needed for asprintf */
#define _GNU_SOURCE

/* Always compile with assertions */
#undef NDEBUG

#include <config.h>

#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "psys.h"
#include "psys_impl.h"
#include "psys_private.h"

static const char *IMPL_LIB = "libpsys_impl.so";

/* struct _psys_err is defined in <psys_impl.h> */

struct _psys_tlist {
	struct _psys_tlist *next;
	char *locale;
	char *value;
};

struct _psys_plist {
	struct _psys_plist *next;
	char *path;
};

struct _psys_pkg {
	/* Data directory */
	char *dir;

	/* Core metadata */
	char *vendor;
	char *name;
	char *version;
	char *lsbversion;
	char *arch;

	/* Optional metadata */
	psys_tlist_t summary;
	psys_tlist_t description;

	/* Extra files */
	psys_plist_t extras;
};

/*** Handling errors **********************************************************/

int psys_err_code(psys_err_t err)
{
	assert(err != NULL);
	return err->code;
}

const char *psys_err_msg(psys_err_t err)
{
	assert(err != NULL);
	return err->msg;
}

void psys_err_free(psys_err_t err)
{
	if (err && err->code != PSYS_ENOMEM) {
		if (err->msg)
			free(err->msg);
		free(err);
	}
}

/*** Traversing translation lists *********************************************/

static void tlist_free(psys_tlist_t list)
{
	if (list) {
		psys_tlist_t l;
		psys_tlist_t next;

		l = list;
		while (l) {
			if (l->locale)
				free(l->locale);
			if (l->value)
				free(l->value);
			next = l->next;
			free(l);
			l = next;
		}
	}
}

static psys_tlist_t tlist_add(psys_tlist_t list, const char *locale,
			      const char *trans)
{
	assert(locale != NULL);
	assert(trans != NULL);

	if (!strcmp(locale, "POSIX"))
		locale = "C";

	/*
	 * Is there already a tranlation for the passed locale in the list?
	 * If yes, overwrite
	 */
	if (list) {
		psys_tlist_t l;

		for (l = list; l; l = psys_tlist_next(l)) {
			if (!strcmp(l->locale, locale)) {
				char *trans_dup;
				trans_dup = strdup(trans);
				if (!trans_dup) {
					return NULL;
				} else {
					assert(l->value != NULL);
					free(l->value);
					l->value = trans_dup;
					return list;
				}
			}
		}	
	}

	/* If not, create a new element */
	{
		psys_tlist_t elem;

		elem = malloc(sizeof(*elem));
		if (!elem)
			return NULL;

		elem->locale = strdup(locale);
		elem->value = strdup(trans);
		if (!elem->locale || !elem->value) {
			tlist_free(elem);
			return NULL;
		}

		if (list)
			/*
			 * Prepend because this is the most efficient, and
			 * the order of a translation list's elements is not
			 * defined anyway
			 */
			elem->next = list;
		else
			elem->next = NULL;

		return elem;
	}
}

const char *psys_tlist_locale(psys_tlist_t elem)
{
	assert(elem != NULL);
	return elem->locale;
}

const char *psys_tlist_value(psys_tlist_t elem)
{
	assert (elem != NULL);
	return elem->value;
}

psys_tlist_t psys_tlist_next(psys_tlist_t elem)
{
	assert (elem != NULL);
	return elem->next;
}

/*** Traversing path lists ****************************************************/

static void plist_free(psys_plist_t list)
{
	if (list) {
		psys_plist_t l;
		psys_plist_t next;

		l = list;
		while (l) {
			if (l->path)
				free(l->path);
			next = l->next;
			free(l);
			l = next;
		}
	}
}

static psys_plist_t plist_add(psys_plist_t list, const char *path)
{
	psys_plist_t elem;

	assert (path != NULL);
	assert (path[0] == '/' && "Path must be absolute");
	assert (psys_path_is_canonical(path));

	elem = malloc(sizeof(*elem));
	if (!elem)
		return NULL;

	elem->path = strdup(path);
	if (!elem->path) {
		plist_free(elem);
		return NULL;
	}

	/*
	 * Strip trailing slashes so that we get completely
	 * normalized paths
	 */
	if (elem->path[strlen(elem->path) - 1] == '/')
		elem->path[strlen(elem->path) - 1] = '\0';

	if (list)
		/*
		 * Like with translation lists, prepending is just the
		 * most efficient thing to do.
		 */
		elem->next = list;
	else
		elem->next = NULL;

	return elem;
}

const char *psys_plist_path(psys_plist_t elem)
{
	return elem->path;
}

psys_plist_t psys_plist_next(psys_plist_t elem)
{
	return elem->next;
}

/*** Creating and freeing package objects *************************************/

psys_pkg_t psys_pkg_new(const char *vendor, const char *name,
			const char *version, const char *lsbversion,
			const char *arch)
 {
 	psys_pkg_t pkg;

	assert(vendor != NULL);
	assert(name != NULL);
	assert(version != NULL);
	assert(lsbversion != NULL);
	assert(arch != NULL);

	pkg = malloc(sizeof(*pkg));
	if (!pkg)
		return NULL;

	if (asprintf(&pkg->dir, "/opt/%s/%s", vendor, name) < 0) {
		psys_pkg_free(pkg);
		return NULL;
	}

	pkg->vendor = strdup(vendor);
	pkg->name = strdup(name);
	pkg->version = strdup(version);
	pkg->lsbversion = strdup(lsbversion);
	pkg->arch = strdup(arch);
	if (!pkg->vendor || !pkg->name || !pkg->version || !pkg->lsbversion ||
	    !pkg->arch) {
		psys_pkg_free(pkg);
		return NULL;
	}

	pkg->summary = NULL;
	pkg->description = NULL;
	pkg->extras = NULL;

	psys_pkg_assert_valid(pkg);
	return pkg;
}

void psys_pkg_free(psys_pkg_t pkg)
{
	if (pkg) {
		if (pkg->dir)
			free(pkg->dir);
		if (pkg->vendor)
			free(pkg->vendor);
		if (pkg->name)
			free(pkg->name);
		if (pkg->version)
			free(pkg->version);
		if (pkg->lsbversion)
			free(pkg->lsbversion);
		if (pkg->arch)
			free(pkg->arch);

		tlist_free(pkg->summary);
		tlist_free(pkg->description);
		plist_free(pkg->extras);

		free(pkg);
	}
}

/*** Retrieving the package data directory ************************************/

const char *psys_pkg_dir(psys_pkg_t pkg)
{
	assert(pkg != NULL);
	return pkg->dir;
}

/*** Retrieving core package metadata *****************************************/

const char *psys_pkg_vendor(psys_pkg_t pkg)
{
	assert(pkg != NULL);
	return pkg->vendor;
}

const char *psys_pkg_name(psys_pkg_t pkg)
{
	assert(pkg != NULL);
	return pkg->name;
}

const char *psys_pkg_version(psys_pkg_t pkg)
{
	assert(pkg != NULL);
	return pkg->version;
}

const char *psys_pkg_lsbversion(psys_pkg_t pkg)
{
	assert(pkg != NULL);
	return pkg->lsbversion;
}

const char *psys_pkg_arch(psys_pkg_t pkg)
{
	assert(pkg != NULL);
	return pkg->arch;
}

/*** Retrieving optional package metadata *************************************/

psys_tlist_t psys_pkg_summary(psys_pkg_t pkg)
{
	assert(pkg != NULL);
	return pkg->summary;
}

psys_tlist_t psys_pkg_description(psys_pkg_t pkg)
{
	assert(pkg != NULL);
	return pkg->description;
}

/*** Adding optional package metadata ****************************************/

int psys_pkg_add_summary(psys_pkg_t pkg, const char *locale,
			 const char *summary)
{
	psys_tlist_t newlist;

	assert(pkg != NULL);
	assert(locale != NULL);
	assert(summary != NULL);

	newlist = tlist_add(pkg->summary, locale, summary);
	if (!newlist)
		return -1;
	pkg->summary = newlist;
	return 0;
}

int psys_pkg_add_description(psys_pkg_t pkg, const char *locale,
			     const char *description)
{
	psys_tlist_t newlist;

	assert(pkg != NULL);
	assert(locale != NULL);
	assert(description != NULL);

	newlist = tlist_add(pkg->description, locale, description);
	if (!newlist)
		return -1;
	pkg->description = newlist;
	return 0;
}

/*** Retrieving and adding package extra files ********************************/

psys_plist_t psys_pkg_extras(psys_pkg_t pkg)
{
	assert(pkg != NULL);
	return pkg->extras;
}

int psys_pkg_add_extra(psys_pkg_t pkg, const char *path)
{
	psys_plist_t newlist;

	assert(pkg != NULL);
	assert(path != NULL);

	newlist = plist_add(pkg->extras, path);
	if (!newlist)
		return -1;
	pkg->extras = newlist;
	return 0;
}

/*** Adding packages to the system package database ***************************/

static int announce_or_register(const char *sym, psys_pkg_t pkg,
				psys_err_t *err)
{
	void *impl;
	int (*fn)(psys_pkg_t, psys_err_t *);

	impl = dlopen(IMPL_LIB, RTLD_LAZY | RTLD_GLOBAL);
	if (impl) {
		fn = (int (*)(psys_pkg_t, psys_err_t *)) dlsym(impl, sym);
		if (fn) {
			int ret;
			ret = (*fn)(pkg, err);
			dlclose(impl);
			return ret;
		}
	}

	psys_err_set_notimpl(err);
	return -1;
}

int psys_announce(psys_pkg_t pkg, psys_err_t *err)
{
	return announce_or_register("_psys_announce", pkg, err);
}

int psys_register(psys_pkg_t pkg, psys_err_t *err)
{
	return announce_or_register("_psys_register", pkg, err);
}

/*** Updating packages in the system package database *************************/

int psys_announce_update(psys_pkg_t pkg, psys_err_t *err)
{
	return announce_or_register("_psys_announce_update", pkg, err);
}

int psys_register_update(psys_pkg_t pkg, psys_err_t *err)
{
	return announce_or_register("_psys_register_update", pkg, err);
}

/*** Removing packages from the system package database ***********************/

static int unannounce_or_unregister(const char *sym, const char *vendor,
				    const char *name, psys_err_t *err)
{
	void *impl;
	int (*fn)(const char *, const char *, psys_err_t *);

	impl = dlopen(IMPL_LIB, RTLD_LAZY | RTLD_GLOBAL);
	if (impl) {
		fn = (int (*)(const char *, const char *, psys_err_t *))
				dlsym(impl, sym);

		if (fn) {
			int ret;
			ret = (*fn)(vendor, name, err);
			dlclose(impl);
			return ret;
		}

		dlclose(impl);
	}

	psys_err_set_notimpl(err);
	return -1;
}

int psys_unannounce(const char *vendor, const char *name, psys_err_t *err)
{
	return unannounce_or_unregister("_psys_unannounce", vendor, name, err);
}

int psys_unregister(const char *vendor, const char *name, psys_err_t *err)
{
	return unannounce_or_unregister("_psys_unregister", vendor, name, err);
}
