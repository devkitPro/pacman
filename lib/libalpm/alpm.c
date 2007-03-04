/*
 *  alpm.c
 * 
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2005 by Christian Hamar <krics@linuxforum.hu>
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <syslog.h>
#include <limits.h> /* PATH_MAX */
#include <stdarg.h>
#include <libintl.h>
/* pacman */
#include "log.h"
#include "error.h"
#include "versioncmp.h"
#include "md5.h"
#include "sha1.h"
#include "alpm_list.h"
#include "package.h"
#include "group.h"
#include "util.h"
#include "db.h"
#include "cache.h"
#include "deps.h"
#include "conflict.h"
#include "backup.h"
#include "add.h"
#include "remove.h"
#include "sync.h"
#include "handle.h"
#include "provide.h"
#include "server.h"
#include "alpm.h"

#define min(X, Y)  ((X) < (Y) ? (X) : (Y))

/* Globals */
pmhandle_t *handle = NULL;
enum _pmerrno_t pm_errno SYMEXPORT;

/** \addtogroup alpm_interface Interface Functions
 * @brief Functions to initialize and release libalpm
 * @{
 */

/** Initializes the library.  This must be called before any other
 * functions are called.
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_initialize()
{
	ASSERT(handle == NULL, RET_ERR(PM_ERR_HANDLE_NOT_NULL, -1));

	handle = _alpm_handle_new();
	if(handle == NULL) {
		RET_ERR(PM_ERR_MEMORY, -1);
	}

	return(0);
}

/** Release the library.  This should be the last alpm call you make.
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_release()
{
	int dbs_left = 0;

	ALPM_LOG_FUNC;

	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	/* free the transaction if there is any */
	if(handle->trans) {
		alpm_trans_release();
	}

	/* close local database */
	if(handle->db_local) {
		alpm_db_unregister(handle->db_local);
		handle->db_local = NULL;
	}
	/* and also sync ones */
	while((dbs_left = alpm_list_count(handle->dbs_sync)) > 0) {
		pmdb_t *db = (pmdb_t *)handle->dbs_sync->data;
		_alpm_log(PM_LOG_DEBUG, _("removing DB %s, %d remaining..."), db->treename, dbs_left);
		alpm_db_unregister(db);
		db = NULL;
	}

	FREEHANDLE(handle);

	return(0);
}

/** @} */

/** \addtogroup alpm_databases Database Functions
 * @brief Functions to query and manipulate the database of libalpm
 * @{
 */

/** Register a package database
 * @param treename the name of the repository
 * @return a pmdb_t* on success (the value), NULL on error
 */
pmdb_t SYMEXPORT *alpm_db_register(char *treename)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, NULL));
	ASSERT(treename != NULL && strlen(treename) != 0, RET_ERR(PM_ERR_WRONG_ARGS, NULL));
	/* Do not register a database if a transaction is on-going */
	ASSERT(handle->trans == NULL, RET_ERR(PM_ERR_TRANS_NOT_NULL, NULL));

	return(_alpm_db_register(treename, NULL));
}

/** Unregister a package database
 * @param db pointer to the package database to unregister
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_db_unregister(pmdb_t *db)
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

	_alpm_log(PM_LOG_DEBUG, _("unregistering database '%s'"), db->treename);

	/* Cleanup */
	_alpm_db_free_pkgcache(db);

	_alpm_log(PM_LOG_DEBUG, _("closing database '%s'"), db->treename);
	_alpm_db_close(db);

	_alpm_db_free(db);

	return(0);
}

/** Set the serverlist of a database.
 * @param db database pointer
 * @param url url of the server
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_db_setserver(pmdb_t *db, const char *url)
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
		_alpm_log(PM_LOG_DEBUG, _("adding new server to database '%s': protocol '%s', server '%s', path '%s'"),
				db->treename, server->s_url->scheme, server->s_url->host, server->s_url->doc);
	} else {
		FREELIST(db->servers);
		_alpm_log(PM_LOG_DEBUG, _("serverlist flushed for '%s'"), db->treename);
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
			_alpm_log(PM_LOG_DEBUG, _("failed to get lastupdate time for %s (no big deal)"), db->treename);
		}
	}

	/* build a one-element list */
	snprintf(path, PATH_MAX, "%s" PM_EXT_DB, db->treename);
	files = alpm_list_add(files, strdup(path));

	snprintf(path, PATH_MAX, "%s%s", handle->root, handle->dbpath);

	ret = _alpm_downloadfiles_forreal(db->servers, path, files, lastupdate, newmtime);
	FREELIST(files);
	if(ret == 1) {
		/* mtimes match, do nothing */
		pm_errno = 0;
		return(1);
	} else if(ret == -1) {
		/* we use downloadLastErrString and downloadLastErrCode here, error returns from
		 * libdownload */
		_alpm_log(PM_LOG_DEBUG, _("failed to sync db: %s [%d]"), downloadLastErrString, downloadLastErrCode);
		RET_ERR(PM_ERR_DB_SYNC, -1);
	} else {
		if(strlen(newmtime)) {
			_alpm_log(PM_LOG_DEBUG, _("sync: new mtime for %s: %s"), db->treename, newmtime);
			_alpm_db_setlastupdate(db, newmtime);
		}
		snprintf(path, PATH_MAX, "%s%s%s" PM_EXT_DB, handle->root, handle->dbpath, db->treename);

		/* remove the old dir */
		_alpm_log(PM_LOG_DEBUG, _("flushing database %s%s"), db->path);
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

/** @} */

/** \addtogroup alpm_packages Package Functions
 * @brief Functions to manipulate libalpm packages
 * @{
 */

/** Create a package from a file.
 * @param filename location of the package tarball
 * @param pkg address of the package pointer
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_pkg_load(char *filename, pmpkg_t **pkg)
{
	_alpm_log(PM_LOG_FUNCTION, "enter alpm_pkg_load");

	/* Sanity checks */
	ASSERT(filename != NULL && strlen(filename) != 0, RET_ERR(PM_ERR_WRONG_ARGS, -1));
	ASSERT(pkg != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	*pkg = _alpm_pkg_load(filename);
	if(*pkg == NULL) {
		/* pm_errno is set by pkg_load */
		return(-1);
	}

	return(0);
}

/** Free a package.
 * @param pkg package pointer to free
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_pkg_free(pmpkg_t *pkg)
{
	_alpm_log(PM_LOG_FUNCTION, "enter alpm_pkg_free");

	ASSERT(pkg != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	/* Only free packages loaded in user space */
	if(pkg->origin != PKG_FROM_CACHE) {
		_alpm_pkg_free(pkg);
	}

	return(0);
}

/** Check the integrity (with sha1) of a package from the sync cache.
 * @param pkg package pointer
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_pkg_checksha1sum(pmpkg_t *pkg)
{
	char path[PATH_MAX];
	char *sha1sum = NULL;
	int retval = 0;

	ALPM_LOG_FUNC;

	ASSERT(pkg != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));
	/* We only inspect packages from sync repositories */
	ASSERT(pkg->origin == PKG_FROM_CACHE, RET_ERR(PM_ERR_PKG_INVALID, -1));
	ASSERT(pkg->data != handle->db_local, RET_ERR(PM_ERR_PKG_INVALID, -1));

	snprintf(path, PATH_MAX, "%s%s/%s-%s" PM_EXT_PKG,
	                handle->root, handle->cachedir,
	                alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));

	sha1sum = _alpm_SHAFile(path);
	if(sha1sum == NULL) {
		_alpm_log(PM_LOG_ERROR, _("could not get sha1sum for package %s-%s"),
							alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
		pm_errno = PM_ERR_NOT_A_FILE;
		retval = -1;
	} else {
		if(strcmp(sha1sum, alpm_pkg_get_sha1sum(pkg)) == 0) {
			_alpm_log(PM_LOG_DEBUG, _("sha1sums for package %s-%s match"),
								alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
		} else {
			_alpm_log(PM_LOG_ERROR, _("sha1sums do not match for package %s-%s"),
								alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
			pm_errno = PM_ERR_PKG_INVALID;
			retval = -1;
		}
	}

	FREE(sha1sum);

	return(retval);
}

/** Check the integrity (with md5) of a package from the sync cache.
 * @param pkg package pointer
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_pkg_checkmd5sum(pmpkg_t *pkg)
{
	char path[PATH_MAX];
	char *md5sum = NULL;
	int retval = 0;

	ALPM_LOG_FUNC;

	ASSERT(pkg != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));
	/* We only inspect packages from sync repositories */
	ASSERT(pkg->origin == PKG_FROM_CACHE, RET_ERR(PM_ERR_PKG_INVALID, -1));
	ASSERT(pkg->data != handle->db_local, RET_ERR(PM_ERR_PKG_INVALID, -1));

	snprintf(path, PATH_MAX, "%s%s/%s-%s" PM_EXT_PKG,
	                handle->root, handle->cachedir,
									alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));

	md5sum = _alpm_MDFile(path);
	if(md5sum == NULL) {
		_alpm_log(PM_LOG_ERROR, _("could not get md5sum for package %s-%s"),
							alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
		pm_errno = PM_ERR_NOT_A_FILE;
		retval = -1;
	} else {
		if(strcmp(md5sum, alpm_pkg_get_md5sum(pkg)) == 0) {
			_alpm_log(PM_LOG_DEBUG, _("md5sums for package %s-%s match"),
								alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
		} else {
			_alpm_log(PM_LOG_ERROR, _("md5sums do not match for package %s-%s"),
								alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
			pm_errno = PM_ERR_PKG_INVALID;
			retval = -1;
		}
	}

	FREE(md5sum);

	return(retval);
}

/** Compare versions.
 * @param ver1 first version
 * @param ver2 secont version
 * @return postive, 0 or negative if ver1 is less, equal or more
 * than ver2, respectively.
 */
int SYMEXPORT alpm_pkg_vercmp(const char *ver1, const char *ver2)
{
 	ALPM_LOG_FUNC;

	return(_alpm_versioncmp(ver1, ver2));
}

/* internal */
static char *_supported_archs[] = {
	"i586",
	"i686",
	"ppc",
	"x86_64",
};

char SYMEXPORT *alpm_pkg_name_hasarch(char *pkgname)
{
	/* TODO remove this when we transfer everything over to -ARCH
	 *
	 * this parsing sucks... it's done to support
	 * two package formats for the time being:
	 *    package-name-foo-1.0.0-1-i686
	 * and
	 *    package-name-bar-1.2.3-1
	 */
	size_t i = 0;
	char *arch, *cmp, *p;

	ALPM_LOG_FUNC;

	if((p = strrchr(pkgname, '-'))) {
		for(i=0; i < sizeof(_supported_archs)/sizeof(char*); ++i) {
			cmp = p+1;
			arch = _supported_archs[i];

			/* whee, case insensitive compare */
			while(*arch && *cmp && tolower(*arch++) == tolower(*cmp++)) ;
			if(*arch || *cmp) {
				continue;
			}

			return(p);
		}
	}
	return(NULL);
}

/** @} */

/** \addtogroup alpm_sync Sync Functions
 * @brief Functions to get informations about libalpm syncs
 * @{
 */

/** Searches a database
 * @param db pointer to the package database to search in
 * @param needles the list of strings to search for
 * @return the list of packages on success, NULL on error
 */
alpm_list_t SYMEXPORT *alpm_db_search(pmdb_t *db, alpm_list_t* needles)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));

	return(_alpm_db_search(db, needles));
}

/** @} */

/** \addtogroup alpm_trans Transaction Functions
 * @brief Functions to manipulate libalpm transactions
 * @{
 */

/** Initialize the transaction.
 * @param type type of the transaction
 * @param flags flags of the transaction (like nodeps, etc)
 * @param event event callback function pointer
 * @param conv question callback function pointer
 * @param progress progress callback function pointer
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_trans_init(pmtranstype_t type, unsigned int flags,
                    alpm_trans_cb_event event, alpm_trans_cb_conv conv,
                    alpm_trans_cb_progress progress)
{
	char path[PATH_MAX];

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	ASSERT(handle->trans == NULL, RET_ERR(PM_ERR_TRANS_NOT_NULL, -1));

	/* lock db */
	snprintf(path, PATH_MAX, "%s%s", handle->root, PM_LOCK);
	handle->lckfd = _alpm_lckmk(path);
	if(handle->lckfd == -1) {
		RET_ERR(PM_ERR_HANDLE_LOCK, -1);
	}

	handle->trans = _alpm_trans_new();
	if(handle->trans == NULL) {
		RET_ERR(PM_ERR_MEMORY, -1);
	}

	return(_alpm_trans_init(handle->trans, type, flags, event, conv, progress));
}

/** Search for packages to upgrade and add them to the transaction.
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_trans_sysupgrade()
{
	pmtrans_t *trans;

	ALPM_LOG_FUNC;

	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	trans = handle->trans;
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_INITIALIZED, RET_ERR(PM_ERR_TRANS_NOT_INITIALIZED, -1));
	ASSERT(trans->type == PM_TRANS_TYPE_SYNC, RET_ERR(PM_ERR_TRANS_TYPE, -1));

	return(_alpm_trans_sysupgrade(trans));
}

/** Add a target to the transaction.
 * @param target the name of the target to add
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_trans_addtarget(char *target)
{
	pmtrans_t *trans;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	ASSERT(target != NULL && strlen(target) != 0, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	trans = handle->trans;
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_INITIALIZED, RET_ERR(PM_ERR_TRANS_NOT_INITIALIZED, -1));

	return(_alpm_trans_addtarget(trans, target));
}

/** Prepare a transaction.
 * @param data the address of a PM_LIST where detailed description
 * of an error can be dumped (ie. list of conflicting files)
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_trans_prepare(alpm_list_t **data)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	ASSERT(data != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	ASSERT(handle->trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(handle->trans->state == STATE_INITIALIZED, RET_ERR(PM_ERR_TRANS_NOT_INITIALIZED, -1));

	return(_alpm_trans_prepare(handle->trans, data));
}

/** Commit a transaction.
 * @param data the address of a PM_LIST where detailed description
 * of an error can be dumped (ie. list of conflicting files)
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_trans_commit(alpm_list_t **data)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	ASSERT(handle->trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(handle->trans->state == STATE_PREPARED, RET_ERR(PM_ERR_TRANS_NOT_PREPARED, -1));

	/* Check for database R/W permission */
	if(!(handle->trans->flags & PM_TRANS_FLAG_PRINTURIS)) {
		/* The print-uris operation is a bit odd. So we explicitly check for it */
		ASSERT(handle->access == PM_ACCESS_RW, RET_ERR(PM_ERR_BADPERMS, -1));
	}

	return(_alpm_trans_commit(handle->trans, data));
}

/** Release a transaction.
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_trans_release()
{
	pmtrans_t *trans;
	char path[PATH_MAX];

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	trans = handle->trans;
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state != STATE_IDLE, RET_ERR(PM_ERR_TRANS_NULL, -1));

	/* during a commit do not interrupt inmediatelly, just after a target */
	if(trans->state == STATE_COMMITING || trans->state == STATE_INTERRUPTED) {
		if(trans->state == STATE_COMMITING) {
			trans->state = STATE_INTERRUPTED;
		}
		pm_errno = PM_ERR_TRANS_COMMITING;
		return(-1);
	}

	FREETRANS(handle->trans);

	/* unlock db */
	if(handle->lckfd != -1) {
		close(handle->lckfd);
		handle->lckfd = -1;
	}
	snprintf(path, PATH_MAX, "%s%s", handle->root, PM_LOCK);
	if(_alpm_lckrm(path)) {
		_alpm_log(PM_LOG_WARNING, _("could not remove lock file %s"), path);
		alpm_logaction(_("warning: could not remove lock file %s"), path);
	}

	return(0);
}

/** @} */

/** \addtogroup alpm_log Logging Functions
 * @brief Functions to log using libalpm
 * @{
 */

/** A printf-like function for logging.
 * @param fmt output format
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_logaction(char *fmt, ...)
{
	char str[LOG_STR_LEN];
	va_list args;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	va_start(args, fmt);
	vsnprintf(str, LOG_STR_LEN, fmt, args);
	va_end(args);

	/* TODO	We should add a prefix to log strings depending on who called us.
	 * If logaction was called by the frontend:
	 *   USER: <the frontend log>
	 * and if called internally:
	 *   ALPM: <the library log>
	 * Moreover, the frontend should be able to choose its prefix
	 * (USER by default?):
	 *   pacman: "PACMAN"
	 *   kpacman: "KPACMAN"
	 * This would allow us to share the log file between several frontends
	 * and know who does what */ 
	return(_alpm_logaction(handle->usesyslog, handle->logfd, str));
}
/** @} */

/** \addtogroup alpm_misc Miscellaneous Functions
 * @brief Various libalpm functions
 * @{
 */

/** Get the md5 sum of file.
 * @param name name of the file
 * @return the checksum on success, NULL on error
 */
char SYMEXPORT *alpm_get_md5sum(char *name)
{
	ALPM_LOG_FUNC;

	ASSERT(name != NULL, return(NULL));

	return(_alpm_MDFile(name));
}

/** Get the sha1 sum of file.
 * @param name name of the file
 * @return the checksum on success, NULL on error
 */
char SYMEXPORT *alpm_get_sha1sum(char *name)
{
	ALPM_LOG_FUNC;

	ASSERT(name != NULL, return(NULL));

	return(_alpm_SHAFile(name));
}

/** Fetch a remote pkg.
 * @param url
 * @return the downloaded filename on success, NULL on error
 */
char SYMEXPORT *alpm_fetch_pkgurl(char *url)
{
	ALPM_LOG_FUNC;

	ASSERT(strstr(url, "://"), return(NULL));

	return(_alpm_fetch_pkgurl(url));
}

/** Parses a configuration file.
 * @param file path to the config file.
 * @param callback a function to be called upon new database creation
 * @param this_section the config current section being parsed
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_parse_config(char *file, alpm_cb_db_register callback, const char *this_section)
{
	FILE *fp = NULL;
	char line[PATH_MAX+1];
	char *ptr = NULL;
	char *key = NULL;
	int linenum = 0;
	char section[256] = "";
	pmdb_t *db = NULL;

	ALPM_LOG_FUNC;

	fp = fopen(file, "r");
	if(fp == NULL) {
		return(0);
	}

	if(this_section != NULL && strlen(this_section) > 0) {
		strncpy(section, this_section, min(255, strlen(this_section)));
		if(!strcmp(section, "local")) {
			RET_ERR(PM_ERR_CONF_LOCAL, -1);
		}
		if(strcmp(section, "options")) {
			db = _alpm_db_register(section, callback);
		}
	}

	while(fgets(line, PATH_MAX, fp)) {
		linenum++;
		_alpm_strtrim(line);
		if(strlen(line) == 0 || line[0] == '#') {
			continue;
		}
		if(line[0] == '[' && line[strlen(line)-1] == ']') {
			/* new config section */
			ptr = line;
			ptr++;
			strncpy(section, ptr, min(255, strlen(ptr)-1));
			section[min(255, strlen(ptr)-1)] = '\0';
			_alpm_log(PM_LOG_DEBUG, _("config: new section '%s'"), section);
			if(!strlen(section)) {
				RET_ERR(PM_ERR_CONF_BAD_SECTION, -1);
			}
			if(!strcmp(section, "local")) {
				RET_ERR(PM_ERR_CONF_LOCAL, -1);
			}
			if(strcmp(section, "options")) {
				db = _alpm_db_register(section, callback);
				if(db == NULL) {
					/* pm_errno is set by alpm_db_register */
					return(-1);
				}
			}
		} else {
			/* directive */
			ptr = line;
			key = strsep(&ptr, "=");
			if(key == NULL) {
				RET_ERR(PM_ERR_CONF_BAD_SYNTAX, -1);
			}
			_alpm_strtrim(key);
			key = _alpm_strtoupper(key);
			if(!strlen(section) && strcmp(key, "INCLUDE")) {
				RET_ERR(PM_ERR_CONF_DIRECTIVE_OUTSIDE_SECTION, -1);
			}
			if(ptr == NULL) {
				if(!strcmp(key, "NOPASSIVEFTP")) {
					alpm_option_set_nopassiveftp(1);
					_alpm_log(PM_LOG_DEBUG, _("config: nopassiveftp"));
				} else if(!strcmp(key, "USESYSLOG")) {
					alpm_option_set_usesyslog(1);
					_alpm_log(PM_LOG_DEBUG, _("config: usesyslog"));
				} else if(!strcmp(key, "ILOVECANDY")) {
					alpm_option_set_chomp(1);
					_alpm_log(PM_LOG_DEBUG, _("config: chomp"));
				} else if(!strcmp(key, "USECOLOR")) {
					alpm_option_set_usecolor(1);
					_alpm_log(PM_LOG_DEBUG, _("config: usecolor"));
				} else {
					RET_ERR(PM_ERR_CONF_BAD_SYNTAX, -1);
				}
			} else {
				_alpm_strtrim(ptr);
				if(!strcmp(key, "INCLUDE")) {
					char conf[PATH_MAX];
					strncpy(conf, ptr, PATH_MAX);
					_alpm_log(PM_LOG_DEBUG, _("config: including %s"), conf);
					alpm_parse_config(conf, callback, section);
				} else if(!strcmp(section, "options")) {
					if(!strcmp(key, "NOUPGRADE")) {
						char *p = ptr;
						char *q;

						while((q = strchr(p, ' '))) {
							*q = '\0';
							alpm_option_add_noupgrade(p);
							_alpm_log(PM_LOG_DEBUG, _("config: noupgrade: %s"), p);
							p = q;
							p++;
						}
						alpm_option_add_noupgrade(p);
						_alpm_log(PM_LOG_DEBUG, _("config: noupgrade: %s"), p);
					} else if(!strcmp(key, "NOEXTRACT")) {
						char *p = ptr;
						char *q;

						while((q = strchr(p, ' '))) {
							*q = '\0';
							alpm_option_add_noextract(p);
							_alpm_log(PM_LOG_DEBUG, _("config: noextract: %s"), p);
							p = q;
							p++;
						}
						alpm_option_add_noextract(p);
						_alpm_log(PM_LOG_DEBUG, _("config: noextract: %s"), p);
					} else if(!strcmp(key, "IGNOREPKG")) {
						char *p = ptr;
						char *q;

						while((q = strchr(p, ' '))) {
							*q = '\0';
							alpm_option_add_ignorepkg(p);
							_alpm_log(PM_LOG_DEBUG, _("config: ignorepkg: %s"), p);
							p = q;
							p++;
						}
						alpm_option_add_ignorepkg(p);
						_alpm_log(PM_LOG_DEBUG, _("config: ignorepkg: %s"), p);
					} else if(!strcmp(key, "HOLDPKG")) {
						char *p = ptr;
						char *q;

						while((q = strchr(p, ' '))) {
							*q = '\0';
							alpm_option_add_holdpkg(p);
							_alpm_log(PM_LOG_DEBUG, _("config: holdpkg: %s"), p);
							p = q;
							p++;
						}
						alpm_option_add_holdpkg(p);
						_alpm_log(PM_LOG_DEBUG, _("config: holdpkg: %s"), p);
					} else if(!strcmp(key, "DBPATH")) {
						/* shave off the leading slash, if there is one */
						if(*ptr == '/') {
							ptr++;
						}
						alpm_option_set_dbpath(ptr);
						_alpm_log(PM_LOG_DEBUG, _("config: dbpath: %s"), ptr);
					} else if(!strcmp(key, "CACHEDIR")) {
						/* shave off the leading slash, if there is one */
						if(*ptr == '/') {
							ptr++;
						}
						alpm_option_set_cachedir(ptr);
						_alpm_log(PM_LOG_DEBUG, _("config: cachedir: %s"), ptr);
					} else if (!strcmp(key, "LOGFILE")) {
						alpm_option_set_logfile(ptr);
						_alpm_log(PM_LOG_DEBUG, _("config: logfile: %s"), ptr);
					} else if (!strcmp(key, "XFERCOMMAND")) {
						alpm_option_set_xfercommand(ptr);
						_alpm_log(PM_LOG_DEBUG, _("config: xfercommand: %s"), ptr);
					} else if (!strcmp(key, "UPGRADEDELAY")) {
						/* The config value is in days, we use seconds */
						time_t ud = atol(ptr) * 60 * 60 *24;
						alpm_option_set_upgradedelay(ud);
						_alpm_log(PM_LOG_DEBUG, _("config: upgradedelay: %d"), ud);
					} else {
						RET_ERR(PM_ERR_CONF_BAD_SYNTAX, -1);
					}
				} else {
					if(!strcmp(key, "SERVER")) {
						/* add to the list */
						if(alpm_db_setserver(db, ptr) != 0) {
							/* pm_errno is set by alpm_db_setserver */
							return(-1);
						}
					} else {
						RET_ERR(PM_ERR_CONF_BAD_SYNTAX, -1);
					}
				}
				line[0] = '\0';
			}
		}
	}
	fclose(fp);

	return(0);
}

/** @} */

/* This function is mostly the same as sync.c find_replacements and sysupgrade
 * functions, and we should be able to combine them - this is an interim
 * solution made for -Qu operation */
alpm_list_t *alpm_get_upgrades()
{
	alpm_list_t *syncpkgs = NULL;
	alpm_list_t *i, *j, *k, *m;

	ALPM_LOG_FUNC;

	/* TODO holy nested loops, Batman! */
	/* check for "recommended" package replacements */
	_alpm_log(PM_LOG_DEBUG, _("checking for package replacements"));
	for(i = handle->dbs_sync; i; i = i->next) {
		for(j = _alpm_db_get_pkgcache(i->data); j; j = j->next) {
			pmpkg_t *spkg = j->data;

			for(k = alpm_pkg_get_replaces(spkg); k; k = k->next) {

				for(m = _alpm_db_get_pkgcache(handle->db_local); m; m = m->next) {
					pmpkg_t *lpkg = m->data;
					
					if(strcmp(k->data, alpm_pkg_get_name(lpkg)) == 0) {
						_alpm_log(PM_LOG_DEBUG, _("checking replacement '%s' for package '%s'"), k->data,
											alpm_pkg_get_name(spkg));
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
									FREEPKG(dummy);
									pm_errno = PM_ERR_MEMORY;
									goto error;
								}
								sync->data = alpm_list_add(NULL, dummy);
								syncpkgs = alpm_list_add(syncpkgs, sync);
							}
							_alpm_log(PM_LOG_DEBUG, _("%s-%s elected for upgrade (to be replaced by %s-%s)"),
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
			_alpm_log(PM_LOG_DEBUG, _("'%s' not found in sync db -- skipping"), alpm_pkg_get_name(local));
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
			_alpm_log(PM_LOG_DEBUG, _("'%s' is already elected for removal -- skipping"),
								alpm_pkg_get_name(local));
			continue;
		}

		if(alpm_pkg_compare_versions(local, spkg)) {
			_alpm_log(PM_LOG_DEBUG, _("%s elected for upgrade (%s => %s)"),
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
					FREEPKG(dummy);
					goto error;
				}
				syncpkgs = alpm_list_add(syncpkgs, sync);
			}
		}
	}

	return(syncpkgs);
error:
	if(syncpkgs) {
		alpm_list_free_inner(syncpkgs, _alpm_sync_free);
		alpm_list_free(syncpkgs);
	}
	return(NULL);
}

/* vim: set ts=2 sw=2 noet: */
