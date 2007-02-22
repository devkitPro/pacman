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
#include <libintl.h>
/* pacman */
#include "log.h"
#include "alpm.h"
#include "alpm_list.h"
#include "util.h"
#include "error.h"
#include "package.h"
#include "group.h"
#include "db.h"
#include "cache.h"

/* Returns a new package cache from db.
 * It frees the cache if it already exists.
 */
int _alpm_db_load_pkgcache(pmdb_t *db, pmdbinfrq_t infolevel)
{
	pmpkg_t *info;
	int count = 0;
	/* The group cache needs INFRQ_DESC as well */
	/* pmdbinfrq_t infolevel = INFRQ_DEPENDS | INFRQ_DESC;*/

	ALPM_LOG_FUNC;

	if(db == NULL) {
		return(-1);
	}

	_alpm_db_free_pkgcache(db);

	_alpm_log(PM_LOG_DEBUG, _("loading package cache (infolevel=%#x) for repository '%s'"),
	                        infolevel, db->treename);

	_alpm_db_rewind(db);
	while((info = _alpm_db_scan(db, NULL, infolevel)) != NULL) {
		_alpm_log(PM_LOG_FUNCTION, _("adding '%s' to package cache for db '%s'"), info->name, db->treename);
		info->origin = PKG_FROM_CACHE;
		info->data = db;
		/* add to the collection */
		db->pkgcache = alpm_list_add(db->pkgcache, info);
		count++;
	}

	db->pkgcache = alpm_list_msort(db->pkgcache, count, _alpm_pkg_cmp);
	return(0);
}

void _alpm_db_free_pkgcache(pmdb_t *db)
{
	ALPM_LOG_FUNC;

	if(db == NULL || db->pkgcache == NULL) {
		return;
	}

	_alpm_log(PM_LOG_DEBUG, _("freeing package cache for repository '%s'"),
	                        db->treename);

	FREELISTPKGS(db->pkgcache);

	if(db->grpcache) {
		_alpm_db_free_grpcache(db);
	}
}

alpm_list_t *_alpm_db_get_pkgcache(pmdb_t *db, pmdbinfrq_t infolevel)
{
	ALPM_LOG_FUNC;

	if(db == NULL) {
		return(NULL);
	}

	if(db->pkgcache == NULL) {
		_alpm_db_load_pkgcache(db, infolevel);
	}

	_alpm_db_ensure_pkgcache(db, infolevel);

	if(!db->pkgcache) {
		_alpm_log(PM_LOG_DEBUG, _("error: pkgcache is NULL for db %s"), db->treename);
	}
	return(db->pkgcache);
}

int _alpm_db_ensure_pkgcache(pmdb_t *db, pmdbinfrq_t infolevel)
{
	int reloaded = 0;
	/* for each pkg, check and reload if the requested
	 * info is not already cached
	 */

	ALPM_LOG_FUNC;

  alpm_list_t *p;
	for(p = db->pkgcache; p; p = p->next) {
		pmpkg_t *pkg = (pmpkg_t *)p->data;
		if(infolevel != INFRQ_NONE && !(pkg->infolevel & infolevel)) {
			if(_alpm_db_read(db, pkg, infolevel) == -1) {
				/* TODO should we actually remove from the filesystem here as well? */
				_alpm_db_remove_pkgfromcache(db, pkg);
			} else {
				reloaded = 1;
			}
		}
	}
	if(reloaded) {
		_alpm_log(PM_LOG_DEBUG, _("package cache reloaded (infolevel=%#x) for repository '%s'"),
							infolevel, db->treename);
	}
	return(0);
}

int _alpm_db_add_pkgincache(pmdb_t *db, pmpkg_t *pkg)
{
	pmpkg_t *newpkg;

	ALPM_LOG_FUNC;

	if(db == NULL || pkg == NULL) {
		return(-1);
	}

	newpkg = _alpm_pkg_dup(pkg);
	if(newpkg == NULL) {
		return(-1);
	}
	_alpm_log(PM_LOG_DEBUG, _("adding entry '%s' in '%s' cache"), newpkg->name, db->treename);
	db->pkgcache = alpm_list_add_sorted(db->pkgcache, newpkg, _alpm_pkg_cmp);

	_alpm_db_free_grpcache(db);

	return(0);
}

int _alpm_db_remove_pkgfromcache(pmdb_t *db, pmpkg_t *pkg)
{
	void *vdata;
	pmpkg_t *data;

	ALPM_LOG_FUNC;

	if(db == NULL || pkg == NULL) {
		return(-1);
	}

	db->pkgcache = alpm_list_remove(db->pkgcache, pkg, _alpm_pkg_cmp, &vdata);
	data = vdata;
	if(data == NULL) {
		/* package not found */
		return(-1);
	}

	_alpm_log(PM_LOG_DEBUG, _("removing entry '%s' from '%s' cache"), pkg->name, db->treename);
	FREEPKG(data);

	_alpm_db_free_grpcache(db);

	return(0);
}

pmpkg_t *_alpm_db_get_pkgfromcache(pmdb_t *db, char *target)
{
	ALPM_LOG_FUNC;

	if(db == NULL) {
		return(NULL);
	}

	alpm_list_t *pkgcache = _alpm_db_get_pkgcache(db, INFRQ_NONE);
	if(!pkgcache) {
		_alpm_log(PM_LOG_DEBUG, _("error: pkgcache is NULL for db '%s'"), db->treename);
		return(NULL);
	}

	return(_alpm_pkg_isin(target, pkgcache));
}

/* Returns a new group cache from db.
 */
int _alpm_db_load_grpcache(pmdb_t *db)
{
	alpm_list_t *lp;

	ALPM_LOG_FUNC;

	if(db == NULL) {
		return(-1);
	}

	if(db->pkgcache == NULL) {
		_alpm_db_load_pkgcache(db, INFRQ_DESC);
	}

	_alpm_log(PM_LOG_DEBUG, _("loading group cache for repository '%s'"), db->treename);

	for(lp = _alpm_db_get_pkgcache(db, INFRQ_DESC); lp; lp = lp->next) {
		alpm_list_t *i;
		pmpkg_t *pkg = lp->data;

		for(i = pkg->groups; i; i = i->next) {
			if(!alpm_list_find_str(db->grpcache, i->data)) {
				pmgrp_t *grp = _alpm_grp_new();

				STRNCPY(grp->name, (char *)i->data, GRP_NAME_LEN);
				grp->packages = alpm_list_add_sorted(grp->packages, pkg->name, _alpm_grp_cmp);
				db->grpcache = alpm_list_add_sorted(db->grpcache, grp, _alpm_grp_cmp);
			} else {
				alpm_list_t *j;

				for(j = db->grpcache; j; j = j->next) {
					pmgrp_t *grp = j->data;

					if(strcmp(grp->name, i->data) == 0) {
						if(!alpm_list_find_str(grp->packages, pkg->name)) {
							grp->packages = alpm_list_add_sorted(grp->packages, (char *)pkg->name, _alpm_grp_cmp);
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
	alpm_list_t *lg;

	ALPM_LOG_FUNC;

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

alpm_list_t *_alpm_db_get_grpcache(pmdb_t *db)
{
	ALPM_LOG_FUNC;

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
	alpm_list_t *i;

	ALPM_LOG_FUNC;

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
