/*
 *  cache.c
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
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
/* pacman */
#include "log.h"
#include "alpm.h"
#include "list.h"
#include "util.h"
#include "package.h"
#include "group.h"
#include "db.h"
#include "cache.h"

/* Returns a new package cache from db.
 * It frees the cache if it already exists.
 */
int db_load_pkgcache(pmdb_t *db)
{
	pmpkg_t *info;

	if(db == NULL) {
		return(-1);
	}

	db_free_pkgcache(db);

	_alpm_log(PM_LOG_DEBUG, "loading package cache for repository \"%s\"", db->treename);

	db_rewind(db);
	while((info = db_scan(db, NULL, INFRQ_DESC|INFRQ_DEPENDS)) != NULL) {
		info->origin = PKG_FROM_CACHE;
		info->data = db;
		/* add to the collective */
		db->pkgcache = pm_list_add_sorted(db->pkgcache, info, pkg_cmp);
	}

	return(0);
}

void db_free_pkgcache(pmdb_t *db)
{
	if(db == NULL) {
		return;
	}

	FREELISTPKGS(db->pkgcache);

	if(db->grpcache) {
		db_free_grpcache(db);
	}
}

PMList *db_get_pkgcache(pmdb_t *db)
{
	if(db == NULL) {
		return(NULL);
	}

	if(db->pkgcache == NULL) {
		db_load_pkgcache(db);
	}

	return(db->pkgcache);
}

int db_add_pkgincache(pmdb_t *db, pmpkg_t *pkg)
{
	pmpkg_t *newpkg;

	_alpm_log(PM_LOG_FUNCTION, "[db_add_pkgincache] called");

	if(db == NULL || pkg == NULL) {
		return(-1);
	}

	newpkg = pkg_dup(pkg);
	if(newpkg == NULL) {
		return(-1);
	}
	db->pkgcache = pm_list_add_sorted(db->pkgcache, newpkg, pkg_cmp);

	db_free_grpcache(db);

	return(0);
}

int db_remove_pkgfromcache(pmdb_t *db, char *name)
{
	PMList *i;
	int found = 0;

	if(db == NULL || name == NULL || strlen(name) == 0) {
		return(-1);
	}

	_alpm_log(PM_LOG_FUNCTION, "[db_remove_pkgfromcache] called");

	for(i = db->pkgcache; i && !found; i = i->next) {
		if(strcmp(((pmpkg_t *)i->data)->name, name) == 0) {
			_alpm_log(PM_LOG_DEBUG, "removing entry %s from \"%s\" cache", name, db->treename);
			db->pkgcache = _alpm_list_remove(db->pkgcache, i);
			/* ORE
			MLK: list_remove() does not free properly an entry from a packages list */
			found = 1;
		}
	}

	if(!found) {
		return(-1);
	}

	db_free_grpcache(db);

	return(0);
}

pmpkg_t *db_get_pkgfromcache(pmdb_t *db, char *target)
{
	PMList *i;

	if(db == NULL || target == NULL || strlen(target) == 0) {
		return(NULL);
	}

	for(i = db_get_pkgcache(db); i; i = i->next) {
		pmpkg_t *info = i->data;

		if(strcmp(info->name, target) == 0) {
			return(info);
		}
	}

	return(NULL);
}

/* Returns a new group cache from db.
 * It frees the cache if it already exists.
 */
int db_load_grpcache(pmdb_t *db)
{
	PMList *lp;

	if(db == NULL) {
		return(-1);
	}

	if(db->pkgcache == NULL) {
		db_load_pkgcache(db);
	}

	_alpm_log(PM_LOG_DEBUG, "loading group cache for repository \"%s\"", db->treename);

	for(lp = db->pkgcache; lp; lp = lp->next) {
		PMList *i;
		pmpkg_t *pkg = lp->data;

		for(i = pkg->groups; i; i = i->next) {
			if(!pm_list_is_strin(i->data, db->grpcache)) {
				pmgrp_t *grp = grp_new();

				STRNCPY(grp->name, (char *)i->data, GRP_NAME_LEN);
				grp->packages = pm_list_add_sorted(grp->packages, pkg->name, grp_cmp);
				db->grpcache = pm_list_add_sorted(db->grpcache, grp, grp_cmp);
			} else {
				PMList *j;

				for(j = db->grpcache; j; j = j->next) {
					pmgrp_t *grp = j->data;

					if(strcmp(grp->name, i->data) == 0) {
						if(!pm_list_is_strin(pkg->name, grp->packages)) {
							grp->packages = pm_list_add_sorted(grp->packages, (char *)pkg->name, grp_cmp);
						}
					}
				}
			}
		}
	}

	return(0);
}

void db_free_grpcache(pmdb_t *db)
{
	PMList *lg;

	if(db == NULL) {
		return;
	}

	for(lg = db->grpcache; lg; lg = lg->next) {
		pmgrp_t *grp = lg->data;

		FREELISTPTR(grp->packages);
		FREEGRP(lg->data);
	}
	FREELIST(db->grpcache);
}

PMList *db_get_grpcache(pmdb_t *db)
{
	if(db == NULL) {
		return(NULL);
	}

	if(db->grpcache == NULL) {
		db_load_grpcache(db);
	}

	return(db->grpcache);
}

pmgrp_t *db_get_grpfromcache(pmdb_t *db, char *target)
{
	PMList *i;

	if(db == NULL || target == NULL || strlen(target) == 0) {
		return(NULL);
	}

	for(i = db_get_grpcache(db); i; i = i->next) {
		pmgrp_t *info = i->data;

		if(strcmp(info->name, target) == 0) {
			return(info);
		}
	}

	return(NULL);
}

/* vim: set ts=2 sw=2 noet: */
