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
#include <libintl.h>

#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "deptest.h"
#include "util.h"
#include "conf.h"
#include "log.h"
#include "sync.h"

extern config_t *config;

int pacman_deptest(alpm_list_t *targets)
{
	int retval = 0;
	pmdb_t *local;
	pmpkg_t *pkg;
	alpm_list_t *i, *provides;

	if(targets == NULL) {
		return(0);
	}
	
	local = alpm_option_get_localdb();

	for(i = targets; i; i = alpm_list_next(i)) {
		const char *pkgname;
	 
		pkgname = alpm_list_getdata(i);
		/* find this package in the local DB */
		pkg = alpm_db_get_pkg(local, pkgname);

		if(!pkg) {
			/* not found, can we find anything that provides this in the local DB? */
			provides = alpm_db_whatprovides(local, pkgname);
			if(!provides) {
				/* nope, must be missing */
				MSG(NL, _("requires: %s"), pkgname);
				retval = 1;
			}
		}
	}
	return(retval);
}

/* vim: set ts=2 sw=2 noet: */
