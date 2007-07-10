/*
 *  db.c
 * 
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
#include <dirent.h>
#include <regex.h>

/* libalpm */
#include "db.h"
#include "alpm_list.h"
#include "log.h"
#include "util.h"
#include "error.h"
#include "server.h"
#include "provide.h"
#include "handle.h"
#include "cache.h"
#include "alpm.h"

#include "sync.h" /* alpm_db_get_upgrades() */

/** \addtogroup alpm_databases Database Functions
 * @brief Functions to query and manipulate the database of libalpm
 * @{
 */

/** Register a package database
 * @param treename the name of the repository
 * @return a pmdb_t* on success (the value), NULL on error
 */
pmdb_t SYMEXPORT *alpm_db_register(const char *treename)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, NULL));
	ASSERT(treename != NULL && strlen(treename) != 0, RET_ERR(PM_ERR_WRONG_ARGS, NULL));
	/* Do not register a database if a transaction is on-going */
	ASSERT(handle->trans == NULL, RET_ERR(PM_ERR_TRANS_NOT_NULL, NULL));

	return(_alpm_db_register(treename));
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
		void *data;
		handle->dbs_sync = alpm_list_remove(handle->dbs_sync, db, _alpm_db_cmp, &data);
		if(data) {
			found = 1;
		}
	}

	if(!found) {
		RET_ERR(PM_ERR_DB_NOT_FOUND, -1);
	}

	_alpm_log(PM_LOG_DEBUG, "unregistering database '%s'", db->treename);

	/* Cleanup */
	_alpm_db_free_pkgcache(db);

	_alpm_log(PM_LOG_DEBUG, "closing database '%s'", db->treename);
	_alpm_db_close(db);

	_alpm_db_free(db);

	return(0);
}

/** Set the serverlist of a database.
 * @param db database pointer
 * @param url url of the server
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_db_setserver(pmdb_t *db, const char *url)
{
	int found = 0;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));

	if(strcmp(db->treename, "local") == 0) {
		if(handle->db_local != NULL) {
			found = 1;
		}
	} else {
		alpm_list_t *i;
		for(i = handle->dbs_sync; i && !found; i = i->next) {
			pmdb_t *sdb = i->data;
			if(strcmp(db->treename, sdb->treename) == 0) {
				found = 1;
			}
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
		_alpm_log(PM_LOG_DEBUG, "adding new server to database '%s': protocol '%s', server '%s', path '%s'",
							db->treename, server->s_url->scheme, server->s_url->host, server->s_url->doc);
	} else {
		FREELIST(db->servers);
		_alpm_log(PM_LOG_DEBUG, "serverlist flushed for '%s'", db->treename);
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
	char newmtime[16] = "";
	char lastupdate[16] = "";
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

	if(!alpm_list_find(handle->dbs_sync, db)) {
		RET_ERR(PM_ERR_DB_NOT_FOUND, -1);
	}

	if(!force) {
		/* get the lastupdate time */
		_alpm_db_getlastupdate(db, lastupdate);
		if(strlen(lastupdate) == 0) {
			_alpm_log(PM_LOG_DEBUG, "failed to get lastupdate time for %s (no big deal)", db->treename);
		}
	}

	/* build a one-element list */
	snprintf(path, PATH_MAX, "%s" DBEXT, db->treename);
	files = alpm_list_add(files, strdup(path));

	dbpath = alpm_option_get_dbpath();

	ret = _alpm_downloadfiles_forreal(db->servers, dbpath, files, lastupdate, newmtime);
	FREELIST(files);
	if(ret == 1) {
		/* mtimes match, do nothing */
		pm_errno = 0;
		return(1);
	} else if(ret == -1) {
		/* we use downloadLastErrString and downloadLastErrCode here, error returns from
		 * libdownload */
		_alpm_log(PM_LOG_DEBUG, "failed to sync db: %s [%d]",
				downloadLastErrString, downloadLastErrCode);
		RET_ERR(PM_ERR_DB_SYNC, -1);
	} else {
		if(strlen(newmtime)) {
			_alpm_log(PM_LOG_DEBUG, "sync: new mtime for %s: %s",
					db->treename, newmtime);
			_alpm_db_setlastupdate(db, newmtime);
		}
		snprintf(path, PATH_MAX, "%s%s" DBEXT, dbpath, db->treename);

		/* remove the old dir */
		_alpm_log(PM_LOG_DEBUG, "flushing database %s%s", db->path);
		for(lp = _alpm_db_get_pkgcache(db); lp; lp = lp->next) {
			pmpkg_t *pkg = lp->data;
			if(pkg && _alpm_db_remove(db, pkg) == -1) {
				_alpm_log(PM_LOG_ERROR, _("could not remove database entry %s%s"), db->treename,
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

/** Tests a database
 * @param db pointer to the package database to search in
 * @return the list of problems found on success, NULL on error
 */
alpm_list_t SYMEXPORT *alpm_db_test(pmdb_t *db)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));

	return(_alpm_db_test(db));
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

/* This function is mostly the same as sync.c find_replacements and sysupgrade
 * functions, and we should be able to combine them - this is an interim
 * solution made for -Qu operation */
/** Get a list of upgradable packages on the current system
 * @return a pmsyncpkg_t list of packages that are out of date
 */
alpm_list_t SYMEXPORT *alpm_db_get_upgrades()
{
	alpm_list_t *syncpkgs = NULL;
	const alpm_list_t *i, *j, *k, *m;

	ALPM_LOG_FUNC;

	/* TODO holy nested loops, Batman! */
	/* check for "recommended" package replacements */
	_alpm_log(PM_LOG_DEBUG, "checking for package replacements");
	for(i = handle->dbs_sync; i; i = i->next) {
		for(j = _alpm_db_get_pkgcache(i->data); j; j = j->next) {
			pmpkg_t *spkg = j->data;

			for(k = alpm_pkg_get_replaces(spkg); k; k = k->next) {

				for(m = _alpm_db_get_pkgcache(handle->db_local); m; m = m->next) {
					pmpkg_t *lpkg = m->data;

					if(strcmp(k->data, alpm_pkg_get_name(lpkg)) == 0) {
						_alpm_log(PM_LOG_DEBUG, "checking replacement '%s' for package '%s'",
								k->data, alpm_pkg_get_name(spkg));
						if(alpm_list_find_str(handle->ignorepkg, alpm_pkg_get_name(lpkg))) {
							_alpm_log(PM_LOG_WARNING, _("%s-%s: ignoring package upgrade (to be replaced by %s-%s)"),
												alpm_pkg_get_name(lpkg), alpm_pkg_get_version(lpkg),
												alpm_pkg_get_name(spkg), alpm_pkg_get_version(spkg));
						} else {
							/* assume all replaces=() packages are accepted */
							pmsyncpkg_t *sync = NULL;
							pmpkg_t *dummy = _alpm_pkg_new(alpm_pkg_get_name(lpkg), NULL);
							if(dummy == NULL) {
								pm_errno = PM_ERR_MEMORY;
								goto error;
							}
							dummy->requiredby = alpm_list_strdup(alpm_pkg_get_requiredby(lpkg));

							pmsyncpkg_t *syncpkg;
							syncpkg = _alpm_sync_find(syncpkgs, alpm_pkg_get_name(spkg));

							if(syncpkg) {
								/* found it -- just append to the replaces list */
								sync->data = alpm_list_add(sync->data, dummy);
							} else {
								/* none found -- enter pkg into the final sync list */
								sync = _alpm_sync_new(PM_SYNC_TYPE_REPLACE, spkg, NULL);
								if(sync == NULL) {
									_alpm_pkg_free(dummy);
									pm_errno = PM_ERR_MEMORY;
									goto error;
								}
								sync->data = alpm_list_add(NULL, dummy);
								syncpkgs = alpm_list_add(syncpkgs, sync);
							}
							_alpm_log(PM_LOG_DEBUG, "%s-%s elected for upgrade (to be replaced by %s-%s)",
												alpm_pkg_get_name(lpkg), alpm_pkg_get_version(lpkg),
												alpm_pkg_get_name(spkg), alpm_pkg_get_version(spkg));
						}
						break;
					}
				}
			}
		}
	}

	/* now do normal upgrades */
	for(i = _alpm_db_get_pkgcache(handle->db_local); i; i = i->next) {
		int replace=0;
		pmpkg_t *local = i->data;
		pmpkg_t *spkg = NULL;
		pmsyncpkg_t *sync;

		for(j = handle->dbs_sync; !spkg && j; j = j->next) {
			spkg = _alpm_db_get_pkgfromcache(j->data, alpm_pkg_get_name(local));
		}
		if(spkg == NULL) {
			_alpm_log(PM_LOG_DEBUG, "'%s' not found in sync db -- skipping",
					alpm_pkg_get_name(local));
			continue;
		}

		/* we don't care about a to-be-replaced package's newer version */
		for(j = syncpkgs; j && !replace; j=j->next) {
			sync = j->data;
			if(sync->type == PM_SYNC_TYPE_REPLACE) {
				if(_alpm_pkg_find(alpm_pkg_get_name(spkg), sync->data)) {
					replace=1;
				}
			}
		}
		if(replace) {
			_alpm_log(PM_LOG_DEBUG, "'%s' is already elected for removal -- skipping",
								alpm_pkg_get_name(local));
			continue;
		}

		if(alpm_pkg_compare_versions(local, spkg)) {
			_alpm_log(PM_LOG_DEBUG, "%s elected for upgrade (%s => %s)",
								alpm_pkg_get_name(local), alpm_pkg_get_version(local),
								alpm_pkg_get_version(spkg));

			pmsyncpkg_t *syncpkg;
			syncpkg	= _alpm_sync_find(syncpkgs, alpm_pkg_get_name(local));

			if(!syncpkg) {
				pmpkg_t *dummy = _alpm_pkg_new(alpm_pkg_get_name(local),
																			 alpm_pkg_get_version(local));
				if(dummy == NULL) {
					goto error;
				}
				sync = _alpm_sync_new(PM_SYNC_TYPE_UPGRADE, spkg, dummy);
				if(sync == NULL) {
					_alpm_pkg_free(dummy);
					goto error;
				}
				syncpkgs = alpm_list_add(syncpkgs, sync);
			}
		}
	}

	return(syncpkgs);
error:
	if(syncpkgs) {
		alpm_list_t *tmp;
		for(tmp = syncpkgs; tmp; tmp = alpm_list_next(tmp)) {
			if(tmp->data) {
				_alpm_sync_free(tmp->data);
			}
		}
		alpm_list_free(syncpkgs);
	}
	return(NULL);
}

/** @} */

pmdb_t *_alpm_db_new(const char *dbpath, const char *treename)
{
	pmdb_t *db;
	const size_t pathsize = strlen(dbpath) + strlen(treename) + 2;

	ALPM_LOG_FUNC;

	db = calloc(1, sizeof(pmdb_t));
	if(db == NULL) {
		_alpm_log(PM_LOG_ERROR, _("malloc failed: could not allocate %d bytes"),
				  sizeof(pmdb_t));
		RET_ERR(PM_ERR_MEMORY, NULL);
	}

	db->path = calloc(1, pathsize);
	if(db->path == NULL) {
		_alpm_log(PM_LOG_ERROR, _("malloc failed: could not allocate %d bytes"),
				  pathsize);
		FREE(db);
		RET_ERR(PM_ERR_MEMORY, NULL);
	}
	sprintf(db->path, "%s%s/", dbpath, treename);

	strncpy(db->treename, treename, PATH_MAX);

	return(db);
}

void _alpm_db_free(pmdb_t *db)
{
	ALPM_LOG_FUNC;

	alpm_list_t *tmp;
	for(tmp = db->servers; tmp; tmp = alpm_list_next(tmp)) {
		_alpm_server_free(tmp->data);
	}
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
		_alpm_log(PM_LOG_DEBUG, "searching for target '%s'", targ);
		
		if(regcomp(&reg, targ, REG_EXTENDED | REG_NOSUB | REG_ICASE | REG_NEWLINE) != 0) {
			RET_ERR(PM_ERR_INVALID_REGEX, NULL);
		}

		for(j = _alpm_db_get_pkgcache(db); j; j = j->next) {
			pmpkg_t *pkg = j->data;
			const char *matched = NULL;

			/* check name */
			if (regexec(&reg, alpm_pkg_get_name(pkg), 0, 0, 0) == 0) {
				matched = alpm_pkg_get_name(pkg);
			}
			/* check desc */
			else if (regexec(&reg, alpm_pkg_get_desc(pkg), 0, 0, 0) == 0) {
				matched = alpm_pkg_get_desc(pkg);
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
				_alpm_log(PM_LOG_DEBUG, "    search target '%s' matched '%s'",
				          targ, matched);
				ret = alpm_list_add(ret, pkg);
			}
		}

		regfree(&reg);
	}

	return(ret);
}

pmdb_t *_alpm_db_register(const char *treename)
{
	struct stat buf;
	pmdb_t *db;
	const char *dbpath;
	char path[PATH_MAX];

	ALPM_LOG_FUNC;

	if(strcmp(treename, "local") == 0) {
		if(handle->db_local != NULL) {
			_alpm_log(PM_LOG_WARNING, _("attempt to re-register the 'local' DB"));
			RET_ERR(PM_ERR_DB_NOT_NULL, NULL);
		}
	} else {
		alpm_list_t *i;
		for(i = handle->dbs_sync; i; i = i->next) {
			pmdb_t *sdb = i->data;
			if(strcmp(treename, sdb->treename) == 0) {
				_alpm_log(PM_LOG_DEBUG, "attempt to re-register the '%s' database, using existing", sdb->treename);
				return sdb;
			}
		}
	}
	
	_alpm_log(PM_LOG_DEBUG, "registering database '%s'", treename);

	/* make sure the database directory exists */
	dbpath = alpm_option_get_dbpath();
	if(!dbpath) {
		_alpm_log(PM_LOG_WARNING, _("database path is undefined"));
			RET_ERR(PM_ERR_DB_OPEN, NULL);
	}
	snprintf(path, PATH_MAX, "%s%s", dbpath, treename);
	if(stat(path, &buf) != 0 || !S_ISDIR(buf.st_mode)) {
		_alpm_log(PM_LOG_DEBUG, "database dir '%s' does not exist, creating it",
				path);
		if(_alpm_makepath(path) != 0) {
			RET_ERR(PM_ERR_SYSTEM, NULL);
		}
	}

	db = _alpm_db_new(handle->dbpath, treename);
	if(db == NULL) {
		RET_ERR(PM_ERR_DB_CREATE, NULL);
	}

	_alpm_log(PM_LOG_DEBUG, "opening database '%s'", db->treename);
	if(_alpm_db_open(db) == -1) {
		_alpm_db_free(db);
		RET_ERR(PM_ERR_DB_OPEN, NULL);
	}

	if(strcmp(treename, "local") == 0) {
		handle->db_local = db;
	} else {
		handle->dbs_sync = alpm_list_add(handle->dbs_sync, db);
	}

	return(db);
}

/* vim: set ts=2 sw=2 noet: */
