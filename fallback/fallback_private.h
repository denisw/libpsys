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
 * fallback_private.h - Helper functions for psys fallback backends
 */

/*** Fallback matching ********************************************************/

static int fallback_match_by_distro(const char **distros)
{
	char *dist;

	dist = psys_lsb_distributor_id();
	if (!dist) {
		return 0;
	} else {
		int matching;
		const char **d;

		matching = 0;
		for (d = distros; *d; d++) {
			if (!strcmp(*d, dist)) {
				matching = 1;
				break;
			}
		}

		free(dist);
		return matching;
	}
}
