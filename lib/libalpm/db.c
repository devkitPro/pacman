/*
 *  db.c
 *
 *  Copyright (c) 2006-2010 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2005 by Christian Hamar <krics@linuxforum.hu>
 *  Copyright (c) 2006 by David Kimpe <dnaku@frugalware.org>
 *  Copyright (c) 2005, 2006 by Miklos Vajna <vmiklos@frugalware.org>
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
#include <sys/stat.h>
#include <dirent.h>
#include <regex.h>
#include <time.h>

/* libalpm */
#include "db.h"
#include "alpm_list.h"
#include "log.h"
#include "util.h"
#include "handle.h"
#include "alpm.h"
#include "package.h"
#include "group.h"

/** \addtogroup alpm_databases Database Functions
 * @brief Functions to query and manipulate the database of libalpm
 * @{
 */

/** Register a sync database of packages.
 * @param treename the name of the sync repository
 * @return a pmdb_t* on success (the value), NULL on error
 */
pmdb_t SYMEXPORT *alpm_db_register_sync(const char *treename)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, NULL));
	ASSERT(treename != NULL && strlen(treename) != 0, RET_ERR(PM_ERR_WRONG_ARGS, NULL));
	/* Do not register a database if a transaction is on-going */
	ASSERT(handle->trans == NULL, RET_ERR(PM_ERR_TRANS_NOT_NULL, NULL));

	return(_alpm_db_register_sync(treename));
}

/** Register the local package database.
 * @return a pmdb_t* representing the local database, or NULL on error
 */
pmdb_t SYMEXPORT *alpm_db_register_local(void)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, NULL));
	/* Do not register a database if a transaction is on-going */
	ASSERT(handle->trans == NULL, RET_ERR(PM_ERR_TRANS_NOT_NULL, NULL));

	return(_alpm_db_register_local());
}

/* Helper function for alpm_db_unregister{_all} */
void _alpm_db_unregister(pmdb_t *db)
{
	if(db == NULL) {
		return;
	}

	_alpm_log(PM_LOG_DEBUG, "unregistering database '%s'\n", db->treename);
	_alpm_db_free(db);
}

/** Unregister all package databases
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_db_unregister_all(void)
{
	alpm_list_t *i;
	pmdb_t *db;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	/* Do not unregister a database if a transaction is on-going */
	ASSERT(handle->trans == NULL, RET_ERR(PM_ERR_TRANS_NOT_NULL, -1));

	/* close local database */
	db = handle->db_local;
	if(db) {
		db->ops->unregister(db);
		handle->db_local = NULL;
	}

	/* and also sync ones */
	for(i = handle->dbs_sync; i; i = i->next) {
		db = i->data;
		db->ops->unregister(db);
		i->data = NULL;
	}
	FREELIST(handle->dbs_sync);
	return(0);
}

/** Unregister a package database
 * @param db pointer to the package database to unregister
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_db_unregister(pmdb_t *db)
{
	int found = 0;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	ASSERT(db != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));
	/* Do not unregister a database if a transaction is on-going */
	ASSERT(handle->trans == NULL, RET_ERR(PM_ERR_TRANS_NOT_NULL, -1));

	if(db == handle->db_local) {
		handle->db_local = NULL;
		found = 1;
	} else {
		/* Warning : this function shouldn't be used to unregister all sync
		 * databases by walking through the list returned by
		 * alpm_option_get_syncdbs, because the db is removed from that list here.
		 */
		void *data;
		handle->dbs_sync = alpm_list_remove(handle->dbs_sync,
				db, _alpm_db_cmp, &data);
		if(data) {
			found = 1;
		}
	}

	if(!found) {
		RET_ERR(PM_ERR_DB_NOT_FOUND, -1);
	}

	db->ops->unregister(db);
	return(0);
}

/** Set the serverlist of a database.
 * @param db database pointer
 * @param url url of the server
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_db_setserver(pmdb_t *db, const char *url)
{
	alpm_list_t *i;
	int found = 0;
	char *newurl;
	int len = 0;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));

	for(i = handle->dbs_sync; i && !found; i = i->next) {
		pmdb_t *sdb = i->data;
		if(strcmp(db->treename, sdb->treename) == 0) {
			found = 1;
		}
	}
	if(!found) {
		RET_ERR(PM_ERR_DB_NOT_FOUND, -1);
	}

	if(url) {
		len = strlen(url);
	}
	if(len) {
		newurl = strdup(url);
		/* strip the trailing slash if one exists */
		if(newurl[len - 1] == '/') {
			newurl[len - 1] = '\0';
		}
		db->servers = alpm_list_add(db->servers, newurl);
		_alpm_log(PM_LOG_DEBUG, "adding new server URL to database '%s': %s\n",
				db->treename, newurl);
	} else {
		FREELIST(db->servers);
		_alpm_log(PM_LOG_DEBUG, "serverlist flushed for '%s'\n", db->treename);
	}

	return(0);
}

/** Get the name of a package database
 * @param db pointer to the package database
 * @return the name of the package database, NULL on error
 */
const char SYMEXPORT *alpm_db_get_name(const pmdb_t *db)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));

	return db->treename;
}

/** Get a download URL for the package database
 * @param db pointer to the package database
 * @return a fully-specified download URL, NULL on error
 */
const char SYMEXPORT *alpm_db_get_url(const pmdb_t *db)
{
	char *url;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));
	ASSERT(db->servers != NULL, return(NULL));

	url = (char*)db->servers->data;

	return(url);
}


/** Get a package entry from a package database
 * @param db pointer to the package database to get the package from
 * @param name of the package
 * @return the package entry on success, NULL on error
 */
pmpkg_t SYMEXPORT *alpm_db_get_pkg(pmdb_t *db, const char *name)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));
	ASSERT(name != NULL && strlen(name) != 0, return(NULL));

	return(_alpm_db_get_pkgfromcache(db, name));
}

/** Get the package cache of a package database
 * @param db pointer to the package database to get the package from
 * @return the list of packages on success, NULL on error
 */
alpm_list_t SYMEXPORT *alpm_db_get_pkgcache(pmdb_t *db)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));

	return(_alpm_db_get_pkgcache(db));
}

/** Get a group entry from a package database
 * @param db pointer to the package database to get the group from
 * @param name of the group
 * @return the groups entry on success, NULL on error
 */
pmgrp_t SYMEXPORT *alpm_db_readgrp(pmdb_t *db, const char *name)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));
	ASSERT(name != NULL && strlen(name) != 0, return(NULL));

	return(_alpm_db_get_grpfromcache(db, name));
}

/** Get the group cache of a package database
 * @param db pointer to the package database to get the group from
 * @return the list of groups on success, NULL on error
 */
alpm_list_t SYMEXPORT *alpm_db_get_grpcache(pmdb_t *db)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));

	return(_alpm_db_get_grpcache(db));
}

/** Searches a database
 * @param db pointer to the package database to search in
 * @param needles the list of strings to search for
 * @return the list of packages on success, NULL on error
 */
alpm_list_t SYMEXPORT *alpm_db_search(pmdb_t *db, const alpm_list_t* needles)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));

	return(_alpm_db_search(db, needles));
}

/** Set install reason for a package in db
 * @param db pointer to the package database
 * @param name the name of the package
 * @param reason the new install reason
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_db_set_pkgreason(pmdb_t *db, const char *name, pmpkgreason_t reason)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	ASSERT(db != NULL && name != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	pmpkg_t *pkg = _alpm_db_get_pkgfromcache(db, name);
	if(pkg == NULL) {
		RET_ERR(PM_ERR_PKG_NOT_FOUND, -1);
	}

	_alpm_log(PM_LOG_DEBUG, "setting install reason %u for %s/%s\n", reason, db->treename, name);
	/* read DESC */
	if(_alpm_local_db_read(db, pkg, INFRQ_DESC)) {
		return(-1);
	}
	if(pkg->reason == reason) {
		/* we are done */
		return(0);
	}
	/* set reason (in pkgcache) */
	pkg->reason = reason;
	/* write DESC */
	if(_alpm_local_db_write(db, pkg, INFRQ_DESC)) {
		return(-1);
	}

	return(0);
}

/** @} */

pmdb_t *_alpm_db_new(const char *treename, int is_local)
{
	pmdb_t *db;

	ALPM_LOG_FUNC;

	CALLOC(db, 1, sizeof(pmdb_t), RET_ERR(PM_ERR_MEMORY, NULL));
	STRDUP(db->treename, treename, RET_ERR(PM_ERR_MEMORY, NULL));
	db->is_local = is_local;

	return(db);
}

void _alpm_db_free(pmdb_t *db)
{
	ALPM_LOG_FUNC;

	/* cleanup pkgcache */
	_alpm_db_free_pkgcache(db);
	/* cleanup server list */
	FREELIST(db->servers);
	FREE(db->_path);
	FREE(db->treename);
	FREE(db);

	return;
}

const char *_alpm_db_path(pmdb_t *db)
{
	if(!db) {
		return(NULL);
	}
	if(!db->_path) {
		const char *dbpath;
		size_t pathsize;

		dbpath = alpm_option_get_dbpath();
		if(!dbpath) {
			_alpm_log(PM_LOG_ERROR, _("database path is undefined\n"));
			RET_ERR(PM_ERR_DB_OPEN, NULL);
		}

		if(db->is_local) {
			pathsize = strlen(dbpath) + strlen(db->treename) + 2;
			CALLOC(db->_path, 1, pathsize, RET_ERR(PM_ERR_MEMORY, NULL));
			sprintf(db->_path, "%s%s/", dbpath, db->treename);
		} else {
			pathsize = strlen(dbpath) + 5 + strlen(db->treename) + 4;
			CALLOC(db->_path, 1, pathsize, RET_ERR(PM_ERR_MEMORY, NULL));
			/* all sync DBs now reside in the sync/ subdir of the dbpath */
			sprintf(db->_path, "%ssync/%s.db", dbpath, db->treename);
		}
		_alpm_log(PM_LOG_DEBUG, "database path for tree %s set to %s\n",
				db->treename, db->_path);
	}
	return(db->_path);
}

int _alpm_db_cmp(const void *d1, const void *d2)
{
	pmdb_t *db1 = (pmdb_t *)d1;
	pmdb_t *db2 = (pmdb_t *)d2;
	return(strcmp(db1->treename, db2->treename));
}

alpm_list_t *_alpm_db_search(pmdb_t *db, const alpm_list_t *needles)
{
	const alpm_list_t *i, *j, *k;
	alpm_list_t *ret = NULL;
	/* copy the pkgcache- we will free the list var after each needle */
	alpm_list_t *list = alpm_list_copy(_alpm_db_get_pkgcache(db));

	ALPM_LOG_FUNC;

	for(i = needles; i; i = i->next) {
		char *targ;
		regex_t reg;

		if(i->data == NULL) {
			continue;
		}
		ret = NULL;
		targ = i->data;
		_alpm_log(PM_LOG_DEBUG, "searching for target '%s'\n", targ);

		if(regcomp(&reg, targ, REG_EXTENDED | REG_NOSUB | REG_ICASE | REG_NEWLINE) != 0) {
			RET_ERR(PM_ERR_INVALID_REGEX, NULL);
		}

		for(j = list; j; j = j->next) {
			pmpkg_t *pkg = j->data;
			const char *matched = NULL;
			const char *name = alpm_pkg_get_name(pkg);
			const char *desc = alpm_pkg_get_desc(pkg);

			/* check name as regex AND as plain text */
			if(name && (regexec(&reg, name, 0, 0, 0) == 0 || strstr(name, targ))) {
				matched = name;
			}
			/* check desc */
			else if (desc && regexec(&reg, desc, 0, 0, 0) == 0) {
				matched = desc;
			}
			/* TODO: should we be doing this, and should we print something
			 * differently when we do match it since it isn't currently printed? */
			if(!matched) {
				/* check provides */
				for(k = alpm_pkg_get_provides(pkg); k; k = k->next) {
					if (regexec(&reg, k->data, 0, 0, 0) == 0) {
						matched = k->data;
						break;
					}
				}
			}
			if(!matched) {
				/* check groups */
				for(k = alpm_pkg_get_groups(pkg); k; k = k->next) {
					if (regexec(&reg, k->data, 0, 0, 0) == 0) {
						matched = k->data;
						break;
					}
				}
			}

			if(matched != NULL) {
				_alpm_log(PM_LOG_DEBUG, "    search target '%s' matched '%s'\n",
				          targ, matched);
				ret = alpm_list_add(ret, pkg);
			}
		}

		/* Free the existing search list, and use the returned list for the
		 * next needle. This allows for AND-based package searching. */
		alpm_list_free(list);
		list = ret;
		regfree(&reg);
	}

	return(ret);
}

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
	if(db->ops->populate(db) == -1) {
		_alpm_log(PM_LOG_DEBUG,
				"failed to load package cache for repository '%s'\n", db->treename);
		return(-1);
	}

	db->pkgcache_loaded = 1;
	return(0);
}

void _alpm_db_free_pkgcache(pmdb_t *db)
{
	ALPM_LOG_FUNC;

	if(db == NULL || !db->pkgcache_loaded) {
		return;
	}

	_alpm_log(PM_LOG_DEBUG, "freeing package cache for repository '%s'\n",
	                        db->treename);

	alpm_list_free_inner(db->pkgcache, (alpm_list_fn_free)_alpm_pkg_free);
	alpm_list_free(db->pkgcache);
	db->pkgcache_loaded = 0;

	_alpm_db_free_grpcache(db);
}

alpm_list_t *_alpm_db_get_pkgcache(pmdb_t *db)
{
	ALPM_LOG_FUNC;

	if(db == NULL) {
		return(NULL);
	}

	if(!db->pkgcache_loaded) {
		_alpm_db_load_pkgcache(db);
	}

	/* hmmm, still NULL ?*/
	if(!db->pkgcache) {
		_alpm_log(PM_LOG_DEBUG, "warning: pkgcache is NULL for db '%s'\n", db->treename);
	}

	return(db->pkgcache);
}

/* "duplicate" pkg then add it to pkgcache */
int _alpm_db_add_pkgincache(pmdb_t *db, pmpkg_t *pkg)
{
	pmpkg_t *newpkg;

	ALPM_LOG_FUNC;

	if(db == NULL || !db->pkgcache_loaded || pkg == NULL) {
		return(-1);
	}

	newpkg = _alpm_pkg_dup(pkg);
	if(newpkg == NULL) {
		return(-1);
	}

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

	if(db == NULL || !db->pkgcache_loaded || pkg == NULL) {
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
		_alpm_log(PM_LOG_DEBUG, "warning: failed to get '%s' from NULL pkgcache\n",
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

	db->grpcache_loaded = 1;
	return(0);
}

void _alpm_db_free_grpcache(pmdb_t *db)
{
	alpm_list_t *lg;

	ALPM_LOG_FUNC;

	if(db == NULL || !db->grpcache_loaded) {
		return;
	}

	_alpm_log(PM_LOG_DEBUG, "freeing group cache for repository '%s'\n",
	                        db->treename);

	for(lg = db->grpcache; lg; lg = lg->next) {
		_alpm_grp_free(lg->data);
		lg->data = NULL;
	}
	FREELIST(db->grpcache);
	db->grpcache_loaded = 0;
}

alpm_list_t *_alpm_db_get_grpcache(pmdb_t *db)
{
	ALPM_LOG_FUNC;

	if(db == NULL) {
		return(NULL);
	}

	if(!db->grpcache_loaded) {
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
