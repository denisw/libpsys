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
 * psys_private.h - Private helper functions
 */

#include <limits.h>
#include <string.h>

#include "psys.h"

/* Concrete structure of psys_err_t */
struct _psys_err {
	int code;
	char *msg;
};

/*** Validating paths *********************************************************/

static int psys_path_is_canonical(const char *path)
{
	char pathcpy[PATH_MAX];
	char *run, *cmp;

	/*
	 * Don't copy the initial '/' to avoid the first token returned by
	 * strsep() being an empty string.
	 */
	strncpy(pathcpy, path + 1, PATH_MAX);

	run = pathcpy;
	while ((cmp = strsep(&run, "/")) != NULL) {
		if (!strlen(cmp) || !strcmp(cmp, ".") || !strcmp(cmp, ".."))
			return 0;
	}
	return 1;
}
