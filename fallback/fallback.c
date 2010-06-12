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
 * fallback.c - Entry points for the psys fallback backend
 */

/* For asprintf */
#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psys_impl.h>

static const char *_fallbacks[] = {
	"dpkg",
	"rpm",
	NULL
};

/*** Helper functions *********************************************************/

/*
 * Forward-declare because fallback_sym() and fallback_find() circularly
 * depend on each other
 */
static const char *fallback_find(void *impl);

static void *fallback_sym(void *impl, const char *name, const char *fallback)
{
	char *sym_str;
	void *sym;

	if (!fallback) {
		fallback = fallback_find(impl);
		if (!fallback)
			return NULL;
	}

	if (asprintf(&sym_str, "%s_%s", fallback, name) < 0)
		return NULL;

	sym = dlsym(impl, sym_str);
	free(sym_str);
	return sym;
}

static const char *fallback_find(void *impl)
{
	const char **fb;
	
	for (fb = _fallbacks; *fb != NULL; fb++) {
		int (*fn)(void);
		fn = fallback_sym(impl, "fallback_match", *fb);
		if (fn && (*fn)())
			return *fb;
	}

	for (fb = _fallbacks; *fb != NULL; fb++) {
		int (*fn)();
		fn = fallback_sym(impl, "fallback_match_fuzzy", *fb);
		if (fn && (*fn)())
			return *fb;
	}
	
	return NULL;
}

/*** Adding packages to the system package database ***************************/

static int announce_or_register(const char *sym, psys_pkg_t pkg,
			        psys_err_t *err)
{
	void *impl;
	int (*fn)(psys_pkg_t, psys_err_t *);
	
	impl = dlopen(NULL, RTLD_LAZY);
	if (impl) {
		fn = (int (*)(psys_pkg_t, psys_err_t *))
				fallback_sym(impl, sym, NULL);

		if (fn) {
			int ret;
			ret = (*fn)(pkg, err);
			dlclose(impl);
			return ret;
		}

		dlclose(impl);
	}
	
	psys_err_set_notimpl(err);
	return -1;
}

int _psys_announce(psys_pkg_t pkg, psys_err_t *err)
{
	return announce_or_register("psys_announce", pkg, err);
}

int _psys_register(psys_pkg_t pkg, psys_err_t *err)
{
	return announce_or_register("psys_register", pkg, err);
}

/*** Updating packages in the system package database *************************/

int _psys_announce_update(psys_pkg_t pkg, psys_err_t *err)
{
	return announce_or_register("psys_announce_update", pkg, err);
}

int _psys_register_update(psys_pkg_t pkg, psys_err_t *err)
{
	return announce_or_register("psys_register_update", pkg, err);
}

/*** Removing packages from the system package database ***********************/

static int unannounce_or_unregister(const char *sym,
				    const char *vendor,
				    const char *name,
				    psys_err_t *err)

{
	void *impl;
	int (*fn)(const char *, const char *, psys_err_t *);
	
	impl = dlopen(NULL, RTLD_LAZY);
	if (impl) {
		fn = (int (*)(const char *, const char *, psys_err_t *))
				fallback_sym(impl, sym, NULL);

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

int _psys_unannounce(const char *vendor, const char *name, psys_err_t *err)
{
	return unannounce_or_unregister("psys_unannounce", vendor, name, err);
}

int _psys_unregister(const char *vendor, const char *name, psys_err_t *err)
{
	return unannounce_or_unregister("psys_unregister", vendor, name, err);
}
