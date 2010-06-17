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
 * psys.h - The psys library interface
 */

#ifndef _PSYS_H
#define _PSYS_H

/* Error codes */
enum {
	PSYS_EACCESS,
	PSYS_EARCH,
	PSYS_ECONFLICT,
	PSYS_EEXIST,
	PSYS_EINTERNAL,
	PSYS_ELSBVER,
	PSYS_ENOMEM,
	PSYS_ENOENT,
	PSYS_ENOTIMPL,
	PSYS_EVER
};

/* Package object type */
typedef struct _psys_pkg *psys_pkg_t;

/* Error object type */
typedef struct _psys_err *psys_err_t;

/* Translation list type */
typedef struct _psys_tlist *psys_tlist_t;

/* Path list type */
typedef struct _psys_plist *psys_plist_t;


/* Handling errors */
extern int psys_err_code(psys_err_t err);
extern const char *psys_err_msg(psys_err_t err);
extern void psys_err_free(psys_err_t err);

/* Traversing translation lists */
extern const char *psys_tlist_locale(psys_tlist_t elem);
extern const char *psys_tlist_value(psys_tlist_t elem);
extern psys_tlist_t psys_tlist_next(psys_tlist_t elem);

/* Traversing path lists */
extern const char *psys_plist_path(psys_plist_t elem);
extern psys_plist_t psys_plist_next(psys_plist_t elem);

/* Creating and freeing package objects */
extern psys_pkg_t psys_pkg_new(const char *vendor, const char *name,
			       const char *version, const char *lsbversion,
			       const char *arch);
extern void psys_pkg_free(psys_pkg_t pkg);

/* Retrieving the package data directory */
extern const char *psys_pkg_dir(psys_pkg_t pkg);

/* Retrieving core package metadata */
extern const char *psys_pkg_vendor(psys_pkg_t pkg);
extern const char *psys_pkg_name(psys_pkg_t pkg);
extern const char *psys_pkg_version(psys_pkg_t pkg);
extern const char *psys_pkg_lsbversion(psys_pkg_t pkg);
extern const char *psys_pkg_arch(psys_pkg_t pkg);

/* Retrieving optional package metadata */
extern psys_tlist_t psys_pkg_summary(psys_pkg_t pkg);
extern psys_tlist_t psys_pkg_description(psys_pkg_t pkg);

/* Adding optional package metadata */
extern void psys_pkg_add_summary(psys_pkg_t pkg, const char *locale,
				 const char *summary);
extern void psys_pkg_add_description(psys_pkg_t pkg, const char *locale,
				     const char *value);

/* Retrieving and adding package extra files */
extern psys_plist_t psys_pkg_extras(psys_pkg_t pkg);
extern int psys_pkg_add_extra(psys_pkg_t pkg, const char *path);

/* Adding packages to the system package database */
extern int psys_announce(psys_pkg_t pkg, psys_err_t *err);
extern int psys_register(psys_pkg_t pkg, psys_err_t *err);

/* Updating packages in the system package database */
extern int psys_announce_update(psys_pkg_t pkg, psys_err_t *err);
extern int psys_register_update(psys_pkg_t pkg, psys_err_t *err);

/* Removing packages from the system package database */
extern int psys_unannounce(const char *vendor, const char *name,
			   psys_err_t *err);
extern int psys_unregister(const char *vendor, const char *name,
			   psys_err_t *err);

#endif /* _PSYS_H */
