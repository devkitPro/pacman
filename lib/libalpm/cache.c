/*
 *  cache.c
 *
 *  Copyright (c) 2002-2007 by Judd Vinet <jvinet@zeroflux.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

/* libalpm */
#include "cache.h"
#include "alpm_list.h"
#include "log.h"
#include "alpm.h"
#include "util.h"
#include "package.h"
#include "group.h"
#include "db.h"

/* Returns a new package cache from db.
 * It frees the cache if it already exists.
 */
int _alpm_db_load_pkgcache(pmdb_t *db)
{
	ALPM_LOG_FUNC;

	if(db == NULL) {
		return(-1);
	}
	_alpm_db_free_pkgcache(db);

	_alpm_log(PM_LOG_DEBUG, "loading package cache for repository '%s'\n",
			db->treename);
	if(_alpm_db_populate(db) == -1) {
		_alpm_log(PM_LOG_DEBUG,
				"failed to load package cache for repository '%s'\n", db->treename);
		return(-1);
	}

	return(0);
}

void _alpm_db_free_pkgcache(pmdb_t *db)
{
	ALPM_LOG_FUNC;

	if(db == NULL || db->pkgcache == NULL) {
		return;
	}

	_alpm_log(PM_LOG_DEBUG, "freeing package cache for repository '%s'\n",
	                        db->treename);

	alpm_list_free_inner(db->pkgcache, (alpm_list_fn_free)_alpm_pkg_free);
	alpm_list_free(db->pkgcache);
	db->pkgcache = NULL;

	if(db->grpcache) {
		_alpm_db_free_grpcache(db);
	}
}

alpm_list_t *_alpm_db_get_pkgcache(pmdb_t *db)
{
	ALPM_LOG_FUNC;

	if(db == NULL) {
		return(NULL);
	}

	if(!db->pkgcache) {
		_alpm_db_load_pkgcache(db);
	}

	/* hmmm, still NULL ?*/
	if(!db->pkgcache) {
		_alpm_log(PM_LOG_DEBUG, "error: pkgcache is NULL for db '%s'\n", db->treename);
	}

	return(db->pkgcache);
}

/* "duplicate" pkg with BASE info (to spare some memory) then add it to pkgcache */
int _alpm_db_add_pkgincache(pmdb_t *db, pmpkg_t *pkg)
{
	pmpkg_t *newpkg;

	ALPM_LOG_FUNC;

	if(db == NULL || pkg == NULL) {
		return(-1);
	}

	newpkg = _alpm_pkg_new();
	if(newpkg == NULL) {
		return(-1);
	}
	newpkg->name = strdup(pkg->name);
	newpkg->version = strdup(pkg->version);
	if(newpkg->name == NULL || newpkg->version == NULL) {
		pm_errno = PM_ERR_MEMORY;
		_alpm_pkg_free(newpkg);
		return(-1);
	}
	newpkg->origin = PKG_FROM_CACHE;
	newpkg->origin_data.db = db;
	newpkg->infolevel = INFRQ_BASE; 

	_alpm_log(PM_LOG_DEBUG, "adding entry '%s' in '%s' cache\n",
						alpm_pkg_get_name(newpkg), db->treename);
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

	_alpm_log(PM_LOG_DEBUG, "removing entry '%s' from '%s' cache\n",
						alpm_pkg_get_name(pkg), db->treename);

	db->pkgcache = alpm_list_remove(db->pkgcache, pkg, _alpm_pkg_cmp, &vdata);
	data = vdata;
	if(data == NULL) {
		/* package not found */
		_alpm_log(PM_LOG_DEBUG, "cannot remove entry '%s' from '%s' cache: not found\n",
							alpm_pkg_get_name(pkg), db->treename);
		return(-1);
	}

	_alpm_pkg_free(data);

	_alpm_db_free_grpcache(db);

	return(0);
}

pmpkg_t *_alpm_db_get_pkgfromcache(pmdb_t *db, const char *target)
{
	ALPM_LOG_FUNC;

	if(db == NULL) {
		return(NULL);
	}

	alpm_list_t *pkgcache = _alpm_db_get_pkgcache(db);
	if(!pkgcache) {
		_alpm_log(PM_LOG_DEBUG, "error: failed to get '%s' from NULL pkgcache\n",
				target);
		return(NULL);
	}

	return(_alpm_pkg_find(pkgcache, target));
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
		_alpm_db_load_pkgcache(db);
	}

	_alpm_log(PM_LOG_DEBUG, "loading group cache for repository '%s'\n",
			db->treename);

	for(lp = _alpm_db_get_pkgcache(db); lp; lp = lp->next) {
		const alpm_list_t *i;
		pmpkg_t *pkg = lp->data;

		for(i = alpm_pkg_get_groups(pkg); i; i = i->next) {
			const char *grpname = i->data;
			alpm_list_t *j;
			pmgrp_t *grp = NULL;
			int found = 0;

			/* first look through the group cache for a group with this name */
			for(j = db->grpcache; j; j = j->next) {
				grp = j->data;

				if(strcmp(grp->name, grpname) == 0
						&& !alpm_list_find_ptr(grp->packages, pkg)) {
					grp->packages = alpm_list_add(grp->packages, pkg);
					found = 1;
					break;
				}
			}
			if(found) {
				continue;
			}
			/* we didn't find the group, so create a new one with this name */
			grp = _alpm_grp_new(grpname);
			grp->packages = alpm_list_add(grp->packages, pkg);
			db->grpcache = alpm_list_add(db->grpcache, grp);
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
		_alpm_grp_free(lg->data);
		lg->data = NULL;
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

pmgrp_t *_alpm_db_get_grpfromcache(pmdb_t *db, const char *target)
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
