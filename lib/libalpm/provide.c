/*
 *  provide.c
 * 
 *  Copyright (c) 2002-2005 by Judd Vinet <jvinet@zeroflux.org>
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
#include <string.h>
/* pacman */
#include "cache.h"
#include "list.h"
#include "db.h"
#include "alpm.h"

/* return a PMList of packages in "db" that provide "package"
 */
PMList *_alpm_db_whatprovides(pmdb_t *db, char *package)
{
	PMList *pkgs = NULL;
	PMList *lp;

	if(db == NULL || package == NULL || strlen(package) == 0) {
		return(NULL);
	}

	for(lp = db_get_pkgcache(db); lp; lp = lp->next) {
		pmpkg_t *info = lp->data;

		if(pm_list_is_strin(package, info->provides)) {
			pkgs = pm_list_add(pkgs, info);
		}
	}

	return(pkgs);
}

/* vim: set ts=2 sw=2 noet: */
