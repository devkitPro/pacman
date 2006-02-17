/*
 *  cache.c
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

/* Helper function for comparing packages
 */
static int pkg_cmp(const void *p1, const void *p2)
{
	return(strcmp(((pmpkg_t *)p1)->name, ((pmpkg_t *)p2)->name));
}

/* Returns a new package cache from db.
 * It frees the cache if it already exists.
 */
int _alpm_db_load_pkgcache(pmdb_t *db)
{
	pmpkg_t *info;
	/* The group cache needs INFRQ_DESC as well */
	unsigned char infolevel = INFRQ_DEPENDS | INFRQ_DESC;

	if(db == NULL) {
		return(-1);
	}

	_alpm_db_free_pkgcache(db);

	_alpm_log(PM_LOG_DEBUG, "loading package cache (infolevel=%#x) for repository '%s'",
	                        infolevel, db->treename);

	_alpm_db_rewind(db);
	while((info = _alpm_db_scan(db, NULL, infolevel)) != NULL) {
		info->origin = PKG_FROM_CACHE;
		info->data = db;
		/* add to the collective */
		db->pkgcache = _alpm_list_add_sorted(db->pkgcache, info, pkg_cmp);
	}

	return(0);
}

void _alpm_db_free_pkgcache(pmdb_t *db)
{
	if(db == NULL || db->pkgcache == NULL) {
		return;
	}

	_alpm_log(PM_LOG_DEBUG, "freeing package cache for repository '%s'",
	                        db->treename);

	FREELISTPKGS(db->pkgcache);

	if(db->grpcache) {
		_alpm_db_free_grpcache(db);
	}
}

PMList *_alpm_db_get_pkgcache(pmdb_t *db)
{
	if(db == NULL) {
		return(NULL);
	}

	if(db->pkgcache == NULL) {
		_alpm_db_load_pkgcache(db);
	}

	return(db->pkgcache);
}

int _alpm_db_add_pkgincache(pmdb_t *db, pmpkg_t *pkg)
{
	pmpkg_t *newpkg;

	if(db == NULL || pkg == NULL) {
		return(-1);
	}

	newpkg = _alpm_pkg_dup(pkg);
	if(newpkg == NULL) {
		return(-1);
	}
	_alpm_log(PM_LOG_DEBUG, "adding entry %s in '%s' cache", newpkg->name, db->treename);
	db->pkgcache = _alpm_list_add_sorted(db->pkgcache, newpkg, pkg_cmp);

	_alpm_db_free_grpcache(db);

	return(0);
}

int _alpm_db_remove_pkgfromcache(pmdb_t *db, pmpkg_t *pkg)
{
	pmpkg_t *data;

	if(db == NULL || pkg == NULL) {
		return(-1);
	}

	db->pkgcache = _alpm_list_remove(db->pkgcache, pkg, pkg_cmp, (void **)&data);
	if(data == NULL) {
		/* package not found */
		return(-1);
	}

	_alpm_log(PM_LOG_DEBUG, "removing entry %s from '%s' cache", pkg->name, db->treename);
	FREEPKG(data);

	_alpm_db_free_grpcache(db);

	return(0);
}

pmpkg_t *_alpm_db_get_pkgfromcache(pmdb_t *db, char *target)
{
	if(db == NULL) {
		return(NULL);
	}

	return(_alpm_pkg_isin(target, _alpm_db_get_pkgcache(db)));
}

/* Returns a new group cache from db.
 * It frees the cache if it already exists.
 */
int _alpm_db_load_grpcache(pmdb_t *db)
{
	PMList *lp;

	if(db == NULL) {
		return(-1);
	}

	if(db->pkgcache == NULL) {
		_alpm_db_load_pkgcache(db);
	}

	_alpm_log(PM_LOG_DEBUG, "loading group cache for repository '%s'", db->treename);

	for(lp = db->pkgcache; lp; lp = lp->next) {
		PMList *i;
		pmpkg_t *pkg = lp->data;

		for(i = pkg->groups; i; i = i->next) {
			if(!_alpm_list_is_strin(i->data, db->grpcache)) {
				pmgrp_t *grp = _alpm_grp_new();

				STRNCPY(grp->name, (char *)i->data, GRP_NAME_LEN);
				grp->packages = _alpm_list_add_sorted(grp->packages, pkg->name, _alpm_grp_cmp);
				db->grpcache = _alpm_list_add_sorted(db->grpcache, grp, _alpm_grp_cmp);
			} else {
				PMList *j;

				for(j = db->grpcache; j; j = j->next) {
					pmgrp_t *grp = j->data;

					if(strcmp(grp->name, i->data) == 0) {
						if(!_alpm_list_is_strin(pkg->name, grp->packages)) {
							grp->packages = _alpm_list_add_sorted(grp->packages, (char *)pkg->name, _alpm_grp_cmp);
						}
					}
				}
			}
		}
	}

	return(0);
}

void _alpm_db_free_grpcache(pmdb_t *db)
{
	PMList *lg;

	if(db == NULL || db->grpcache == NULL) {
		return;
	}

	for(lg = db->grpcache; lg; lg = lg->next) {
		pmgrp_t *grp = lg->data;

		FREELISTPTR(grp->packages);
		FREEGRP(lg->data);
	}
	FREELIST(db->grpcache);
}

PMList *_alpm_db_get_grpcache(pmdb_t *db)
{
	if(db == NULL) {
		return(NULL);
	}

	if(db->grpcache == NULL) {
		_alpm_db_load_grpcache(db);
	}

	return(db->grpcache);
}

pmgrp_t *_alpm_db_get_grpfromcache(pmdb_t *db, char *target)
{
	PMList *i;

	if(db == NULL || target == NULL || strlen(target) == 0) {
		return(NULL);
	}

	for(i = _alpm_db_get_grpcache(db); i; i = i->next) {
		pmgrp_t *info = i->data;

		if(strcmp(info->name, target) == 0) {
			return(info);
		}
	}

	return(NULL);
}

/* vim: set ts=2 sw=2 noet: */
