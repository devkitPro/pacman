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

/* libalpm */
#include "cache.h"
#include "alpm_list.h"
#include "log.h"
#include "alpm.h"
#include "util.h"
#include "error.h"
#include "package.h"
#include "group.h"
#include "db.h"

/* Returns a new package cache from db.
 * It frees the cache if it already exists.
 */
int _alpm_db_load_pkgcache(pmdb_t *db)
{
	pmpkg_t *info;
	int count = 0;

	ALPM_LOG_FUNC;

	if(db == NULL) {
		return(-1);
	}

	_alpm_db_free_pkgcache(db);

	_alpm_log(PM_LOG_DEBUG, _("loading package cache for repository '%s'"),
	          db->treename);

	_alpm_db_rewind(db);
	while((info = _alpm_db_scan(db, NULL)) != NULL) {
		_alpm_log(PM_LOG_FUNCTION, _("adding '%s' to package cache for db '%s'"),
							alpm_pkg_get_name(info), db->treename);
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
		_alpm_log(PM_LOG_DEBUG, _("error: pkgcache is NULL for db '%s'"), db->treename);
	}

	return(db->pkgcache);
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
	_alpm_log(PM_LOG_DEBUG, _("adding entry '%s' in '%s' cache"),
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

	_alpm_log(PM_LOG_DEBUG, _("removing entry '%s' from '%s' cache"),
						alpm_pkg_get_name(pkg), db->treename);

	db->pkgcache = alpm_list_remove(db->pkgcache, pkg, _alpm_pkg_cmp, &vdata);
	data = vdata;
	if(data == NULL) {
		/* package not found */
		_alpm_log(PM_LOG_DEBUG, _("cannot remove entry '%s' from '%s' cache: not found"),
							alpm_pkg_get_name(pkg), db->treename);
		return(-1);
	}

	FREEPKG(data);

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
		_alpm_log(PM_LOG_DEBUG, _("error: failed to get '%s' from NULL pkgcache"), target);
		return(NULL);
	}

	return(_alpm_pkg_find(target, pkgcache));
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

	_alpm_log(PM_LOG_DEBUG, _("loading group cache for repository '%s'"), db->treename);

	for(lp = _alpm_db_get_pkgcache(db); lp; lp = lp->next) {
		alpm_list_t *i;
		pmpkg_t *pkg = lp->data;

		for(i = alpm_pkg_get_groups(pkg); i; i = i->next) {
			if(!alpm_list_find_str(db->grpcache, i->data)) {
				pmgrp_t *grp = _alpm_grp_new();

				strncpy(grp->name, i->data, GRP_NAME_LEN);
				grp->name[GRP_NAME_LEN-1] = '\0';
				grp->packages = alpm_list_add_sorted(grp->packages,
																						 /* gross signature forces us to
																							* discard const */
																						 (void *)alpm_pkg_get_name(pkg),
																						 _alpm_grp_cmp);
				db->grpcache = alpm_list_add_sorted(db->grpcache, grp, _alpm_grp_cmp);
			} else {
				alpm_list_t *j;

				for(j = db->grpcache; j; j = j->next) {
					pmgrp_t *grp = j->data;

					if(strcmp(grp->name, i->data) == 0) {
						const char *pkgname = alpm_pkg_get_name(pkg);
						if(!alpm_list_find_str(grp->packages, pkgname)) {
							grp->packages = alpm_list_add_sorted(grp->packages, (void *)pkgname, _alpm_grp_cmp);
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
