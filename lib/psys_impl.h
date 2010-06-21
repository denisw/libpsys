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
 * psys_impl.h - Helper functions for psys library backend implementations
 */

#ifndef _PSYS_IMPL_H
#define _PSYS_IMPL_H

#include <sys/stat.h>
#include <psys.h>

/* File list type */
typedef struct _psys_flist *psys_flist_t;

/* Internal structure of psys_err_t */
struct _psys_err {
	int code;
	char *msg;
};

/* Looking up the system's LSB distributor ID */
extern char *psys_lsb_distributor_id(void);

/* Copying and validating package objects */
extern psys_pkg_t psys_pkg_copy(psys_pkg_t pkg);
extern void psys_pkg_assert_valid(psys_pkg_t pkg);

/* Comparing package versions */
extern int psys_pkg_vercmp(psys_pkg_t, const char *version);
extern int psys_pkg_lsbvercmp(psys_pkg_t, const char *lsbversion);

/* Setting errors */
extern void psys_err_set(psys_err_t *err, int code, const char *format, ...);
extern void psys_err_set_nomem(psys_err_t *err);
extern void psys_err_set_notimpl(psys_err_t *err);

/* Assembling package file lists */
extern psys_flist_t psys_pkg_flist(psys_pkg_t pkg, psys_err_t *err);

/* Traversing file lists */
extern const char *psys_flist_path(psys_flist_t file);
extern const struct stat *psys_flist_stat(psys_flist_t file);
extern psys_flist_t psys_flist_next(psys_flist_t file);

/* Freeing file lists */
extern void psys_flist_free(psys_flist_t list);

/* Calculating MD5 sums */
char *psys_flist_md5sum(psys_flist_t file, psys_err_t *err);

#endif
