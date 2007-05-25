/*
 *  deptest.c
 * 
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
 *  USA.
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

extern config_t *config;

int pacman_deptest(alpm_list_t *targets)
{
	int retval = 0;
	alpm_list_t *i;

	if(targets == NULL) {
		return(0);
	}
	
	for(i = targets; i; i = alpm_list_next(i)) {
		int found = 0;
		pmpkg_t *pkg;
		pmdepend_t *dep;
		const char *target;
		alpm_list_t *j, *provides;

		target = alpm_list_getdata(i);

		/* splitdep modifies the string... we'll compensate for now */
		char *saved_target = NULL;
		saved_target = calloc(strlen(target)+1, sizeof(char));
		strncpy(saved_target, target, strlen(target));

		dep = alpm_splitdep(target);

		pkg = alpm_db_get_pkg(alpm_option_get_localdb(), target);
		if(pkg && alpm_depcmp(pkg, dep)) {
			found = 1;
		} else {
			/* not found, can we find anything that provides this in the local DB? */
			provides = alpm_db_whatprovides(alpm_option_get_localdb(), target);
			for(j = provides; j; j = alpm_list_next(j)) {
				pmpkg_t *pkg;
				pkg = alpm_list_getdata(j);

				if(pkg && alpm_depcmp(pkg, dep)) {
					found = 1;
					break;
				}
			}
		}

		if(!found) {
			printf("%s\n", saved_target);
			retval = 1;
		}
		free(saved_target);
	}
	return(retval);
}

/* vim: set ts=2 sw=2 noet: */
