/*
 *  db.c
 *
 *  Copyright (c) 2002-2007 by Judd Vinet <jvinet@zeroflux.org>
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h> /* uintmax_t */
#include <sys/stat.h>
#include <dirent.h>
#include <regex.h>
#include <time.h>
#include <download.h> /* downloadLastErrString among others. kill this */

/* libalpm */
#include "db.h"
#include "alpm_list.h"
#include "log.h"
#include "util.h"
#include "error.h"
#include "server.h"
#include "dload.h"
#include "handle.h"
#include "cache.h"
#include "alpm.h"

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
static void _alpm_db_unregister(pmdb_t *db)
{
	if(db == NULL) {
		return;
	}

	_alpm_log(PM_LOG_DEBUG, "closing database '%s'\n", db->treename);
	_alpm_db_close(db);

	_alpm_log(PM_LOG_DEBUG, "unregistering database '%s'\n", db->treename);
	_alpm_db_free(db);
}

/** Unregister all package databases
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_db_unregister_all(void)
{
	alpm_list_t *i;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	/* Do not unregister a database if a transaction is on-going */
	ASSERT(handle->trans == NULL, RET_ERR(PM_ERR_TRANS_NOT_NULL, -1));

	/* close local database */
	_alpm_db_unregister(handle->db_local);
	handle->db_local = NULL;

	/* and also sync ones */
	for(i = handle->dbs_sync; i; i = i->next) {
		pmdb_t *db = i->data;
		_alpm_db_unregister(db);
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

	_alpm_db_unregister(db);
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

	if(url && strlen(url)) {
		pmserver_t *server;
		if((server = _alpm_server_new(url)) == NULL) {
			/* pm_errno is set by _alpm_server_new */
			return(-1);
		}
		db->servers = alpm_list_add(db->servers, server);
		_alpm_log(PM_LOG_DEBUG, "adding new server to database '%s': protocol '%s', server '%s', path '%s'\n",
							db->treename, server->s_url->scheme, server->s_url->host, server->s_url->doc);
	} else {
		FREELIST(db->servers);
		_alpm_log(PM_LOG_DEBUG, "serverlist flushed for '%s'\n", db->treename);
	}

	return(0);
}

/** Update a package database
 * @param force if true, then forces the update, otherwise update only in case
 * the database isn't up to date
 * @param db pointer to the package database to update
 * @return 0 on success, > 0 on error (pm_errno is set accordingly), < 0 if up
 * to date
 */
int SYMEXPORT alpm_db_update(int force, pmdb_t *db)
{
	alpm_list_t *lp;
	char path[PATH_MAX];
	alpm_list_t *files = NULL;
	time_t newmtime = 0, lastupdate = 0;
	const char *dbpath;
	int ret;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	ASSERT(db != NULL && db != handle->db_local, RET_ERR(PM_ERR_WRONG_ARGS, -1));
	/* Verify we are in a transaction.  This is done _mainly_ because we need a DB
	 * lock - if we update without a db lock, we may kludge some other pacman
	 * process that _has_ a lock.
	 */
	ASSERT(handle->trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(handle->trans->state == STATE_INITIALIZED, RET_ERR(PM_ERR_TRANS_NOT_INITIALIZED, -1));
	ASSERT(handle->trans->type == PM_TRANS_TYPE_SYNC, RET_ERR(PM_ERR_TRANS_TYPE, -1));

	if(!alpm_list_find_ptr(handle->dbs_sync, db)) {
		RET_ERR(PM_ERR_DB_NOT_FOUND, -1);
	}

	if(!force) {
		/* get the lastupdate time */
		lastupdate = _alpm_db_getlastupdate(db);
		if(lastupdate == 0) {
			_alpm_log(PM_LOG_DEBUG, "failed to get lastupdate time for %s\n",
					db->treename);
		}
	}

	/* build a one-element list */
	snprintf(path, PATH_MAX, "%s" DBEXT, db->treename);
	files = alpm_list_add(files, strdup(path));

	dbpath = alpm_option_get_dbpath();

	ret = _alpm_downloadfiles_forreal(db->servers, dbpath, files, lastupdate,
			&newmtime, NULL, 0);
	FREELIST(files);
	if(ret == 1) {
		/* mtimes match, do nothing */
		pm_errno = 0;
		return(1);
	} else if(ret == -1) {
		/* we use downloadLastErrString and downloadLastErrCode here, error returns from
		 * libdownload */
		_alpm_log(PM_LOG_DEBUG, "failed to sync db: %s [%d]\n",
				downloadLastErrString, downloadLastErrCode);
		RET_ERR(PM_ERR_DB_SYNC, -1);
	} else {
		if(newmtime != 0) {
			_alpm_log(PM_LOG_DEBUG, "sync: new mtime for %s: %ju\n",
					db->treename, (uintmax_t)newmtime);
			_alpm_db_setlastupdate(db, newmtime);
		}
		snprintf(path, PATH_MAX, "%s%s" DBEXT, dbpath, db->treename);

		/* remove the old dir */
		_alpm_log(PM_LOG_DEBUG, "flushing database %s\n", db->path);
		for(lp = _alpm_db_get_pkgcache(db); lp; lp = lp->next) {
			pmpkg_t *pkg = lp->data;
			if(pkg && _alpm_db_remove(db, pkg) == -1) {
				_alpm_log(PM_LOG_ERROR, _("could not remove database entry %s%s\n"), db->treename,
									alpm_pkg_get_name(pkg));
				RET_ERR(PM_ERR_DB_REMOVE, -1);
			}
		}

		/* Cache needs to be rebuild */
		_alpm_db_free_pkgcache(db);

		/* uncompress the sync database */
		if(_alpm_db_install(db, path) == -1) {
			return -1;
		}
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
	char path[PATH_MAX];
	pmserver_t *s;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));

	s = (pmserver_t*)db->servers->data;

	snprintf(path, PATH_MAX, "%s://%s%s", s->s_url->scheme, s->s_url->host, s->s_url->doc);
	return strdup(path);
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
alpm_list_t SYMEXPORT *alpm_db_getpkgcache(pmdb_t *db)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));

	return(_alpm_db_get_pkgcache(db));
}

/** Get the list of packages that a package provides
 * @param db pointer to the package database to get the package from
 * @param name name of the package
 * @return the list of packages on success, NULL on error
 */
alpm_list_t SYMEXPORT *alpm_db_whatprovides(pmdb_t *db, const char *name)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));
	ASSERT(name != NULL && strlen(name) != 0, return(NULL));

	return(_alpm_db_whatprovides(db, name));
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
alpm_list_t SYMEXPORT *alpm_db_getgrpcache(pmdb_t *db)
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

/** @} */

pmdb_t *_alpm_db_new(const char *dbpath, const char *treename)
{
	pmdb_t *db;
	const size_t pathsize = strlen(dbpath) + strlen(treename) + 2;

	ALPM_LOG_FUNC;

	CALLOC(db, 1, sizeof(pmdb_t), RET_ERR(PM_ERR_MEMORY, NULL));
	CALLOC(db->path, 1, pathsize, RET_ERR(PM_ERR_MEMORY, NULL));

	sprintf(db->path, "%s%s/", dbpath, treename);

	strncpy(db->treename, treename, PATH_MAX);

	return(db);
}

void _alpm_db_free(pmdb_t *db)
{
	alpm_list_t *tmp;

	ALPM_LOG_FUNC;

	/* cleanup pkgcache */
	_alpm_db_free_pkgcache(db);
	/* cleanup server list */
	for(tmp = db->servers; tmp; tmp = alpm_list_next(tmp)) {
		_alpm_server_free(tmp->data);
	}
	alpm_list_free(db->servers);
	FREE(db->path);
	FREE(db);

	return;
}

int _alpm_db_cmp(const void *db1, const void *db2)
{
	ALPM_LOG_FUNC;
	return(strcmp(((pmdb_t *)db1)->treename, ((pmdb_t *)db2)->treename));
}

alpm_list_t *_alpm_db_search(pmdb_t *db, const alpm_list_t *needles)
{
	const alpm_list_t *i, *j, *k;
	alpm_list_t *ret = NULL;

	ALPM_LOG_FUNC;

	for(i = needles; i; i = i->next) {
		char *targ;
		regex_t reg;

		if(i->data == NULL) {
			continue;
		}
		targ = i->data;
		_alpm_log(PM_LOG_DEBUG, "searching for target '%s'\n", targ);

		if(regcomp(&reg, targ, REG_EXTENDED | REG_NOSUB | REG_ICASE | REG_NEWLINE) != 0) {
			RET_ERR(PM_ERR_INVALID_REGEX, NULL);
		}

		for(j = _alpm_db_get_pkgcache(db); j; j = j->next) {
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
			/* check provides */
			/* TODO: should we be doing this, and should we print something
			 * differently when we do match it since it isn't currently printed? */
			else {
				for(k = alpm_pkg_get_provides(pkg); k; k = k->next) {
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

		regfree(&reg);
	}

	return(ret);
}

pmdb_t *_alpm_db_register_local(void)
{
	struct stat buf;
	pmdb_t *db;
	const char *dbpath;
	char path[PATH_MAX];

	ALPM_LOG_FUNC;

	if(handle->db_local != NULL) {
		_alpm_log(PM_LOG_WARNING, _("attempt to re-register the 'local' DB\n"));
		RET_ERR(PM_ERR_DB_NOT_NULL, NULL);
	}

	_alpm_log(PM_LOG_DEBUG, "registering local database\n");

	/* make sure the database directory exists */
	dbpath = alpm_option_get_dbpath();
	if(!dbpath) {
		_alpm_log(PM_LOG_ERROR, _("database path is undefined\n"));
			RET_ERR(PM_ERR_DB_OPEN, NULL);
	}
	snprintf(path, PATH_MAX, "%slocal", dbpath);
	/* TODO this is rediculous, we try to do this even if we can't */
	if(stat(path, &buf) != 0 || !S_ISDIR(buf.st_mode)) {
		_alpm_log(PM_LOG_DEBUG, "database dir '%s' does not exist, creating it\n",
				path);
		if(_alpm_makepath(path) != 0) {
			RET_ERR(PM_ERR_SYSTEM, NULL);
		}
	}

	db = _alpm_db_new(dbpath, "local");
	if(db == NULL) {
		RET_ERR(PM_ERR_DB_CREATE, NULL);
	}

	_alpm_log(PM_LOG_DEBUG, "opening database '%s'\n", db->treename);
	if(_alpm_db_open(db) == -1) {
		_alpm_db_free(db);
		RET_ERR(PM_ERR_DB_OPEN, NULL);
	}

	handle->db_local = db;
	return(db);
}

pmdb_t *_alpm_db_register_sync(const char *treename)
{
	struct stat buf;
	pmdb_t *db;
	const char *dbpath;
	char path[PATH_MAX];
	alpm_list_t *i;

	ALPM_LOG_FUNC;

	for(i = handle->dbs_sync; i; i = i->next) {
		pmdb_t *sdb = i->data;
		if(strcmp(treename, sdb->treename) == 0) {
			_alpm_log(PM_LOG_DEBUG, "attempt to re-register the '%s' database, using existing\n", sdb->treename);
			return sdb;
		}
	}

	_alpm_log(PM_LOG_DEBUG, "registering sync database '%s'\n", treename);

	/* make sure the database directory exists */
	dbpath = alpm_option_get_dbpath();
	if(!dbpath) {
		_alpm_log(PM_LOG_ERROR, _("database path is undefined\n"));
			RET_ERR(PM_ERR_DB_OPEN, NULL);
	}
	/* all sync DBs now reside in the sync/ subdir of the dbpath */
	snprintf(path, PATH_MAX, "%ssync/%s", dbpath, treename);
	/* TODO this is rediculous, we try to do this even if we can't */
	if(stat(path, &buf) != 0 || !S_ISDIR(buf.st_mode)) {
		_alpm_log(PM_LOG_DEBUG, "database dir '%s' does not exist, creating it\n",
				path);
		if(_alpm_makepath(path) != 0) {
			RET_ERR(PM_ERR_SYSTEM, NULL);
		}
	}

	/* Ensure the db gets the real path. */
	path[0] = '\0';
	snprintf(path, PATH_MAX, "%ssync/", dbpath);

	db = _alpm_db_new(path, treename);
	if(db == NULL) {
		RET_ERR(PM_ERR_DB_CREATE, NULL);
	}

	_alpm_log(PM_LOG_DEBUG, "opening database '%s'\n", db->treename);
	if(_alpm_db_open(db) == -1) {
		_alpm_db_free(db);
		RET_ERR(PM_ERR_DB_OPEN, NULL);
	}

	handle->dbs_sync = alpm_list_add(handle->dbs_sync, db);
	return(db);
}

/* helper function for alpm_list_find and _alpm_db_whatprovides
 *
 * @return "provision.name" == needle (as string)
 */
int _alpm_prov_cmp(const void *provision, const void *needle)
{
	char *tmpptr;
	char *provname = strdup(provision);
	int retval = 0;
	tmpptr = strchr(provname, '=');

	if(tmpptr != NULL) { /* provision-version */
		*tmpptr='\0';
	}
	retval = strcmp(provname, needle);
	free(provname);
	return(retval);
}

/* return a alpm_list_t of packages in "db" that provide "package"
 */
alpm_list_t *_alpm_db_whatprovides(pmdb_t *db, const char *package)
{
	alpm_list_t *pkgs = NULL;
	alpm_list_t *lp;

	ALPM_LOG_FUNC;

	if(db == NULL || package == NULL || strlen(package) == 0) {
		return(NULL);
	}

	for(lp = _alpm_db_get_pkgcache(db); lp; lp = lp->next) {
		pmpkg_t *info = lp->data;

		if(alpm_list_find(alpm_pkg_get_provides(info), (const void *)package, _alpm_prov_cmp)) {
			pkgs = alpm_list_add(pkgs, info);
		}
	}

	return(pkgs);
}

/* vim: set ts=2 sw=2 noet: */
