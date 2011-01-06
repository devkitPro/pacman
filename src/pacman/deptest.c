/*
 *  deptest.c
 *
 *  Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "pacman.h"
#include "util.h"
#include "conf.h"

int pacman_deptest(alpm_list_t *targets)
{
	alpm_list_t *i;
	alpm_list_t *deps = NULL;
	pmdb_t *localdb = alpm_option_get_localdb();

	for(i = targets; i; i = alpm_list_next(i)) {
		char *target = alpm_list_getdata(i);

		if(!alpm_find_satisfier(alpm_db_get_pkgcache(localdb), target)) {
			deps = alpm_list_add(deps, target);
		}
	}

	if(deps == NULL) {
		return(0);
	}

	for(i = deps; i; i = alpm_list_next(i)) {
		const char *dep = alpm_list_getdata(i);

		printf("%s\n", dep);
	}
	alpm_list_free(deps);
	return(127);
}

/* vim: set ts=2 sw=2 noet: */
