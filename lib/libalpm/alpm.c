/*
 *  alpm.c
 * 
 *  Copyright (c) 2002 by Judd Vinet <jvinet@zeroflux.org>
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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <syslog.h>
#include <limits.h> /* PATH_MAX */
#include <stdarg.h>
/* pacman */
#include "log.h"
#include "error.h"
#include "rpmvercmp.h"
#include "md5.h"
#include "list.h"
#include "package.h"
#include "group.h"
#include "util.h"
#include "db.h"
#include "cache.h"
#include "deps.h"
#include "backup.h"
#include "add.h"
#include "remove.h"
#include "sync.h"
#include "handle.h"
#include "alpm.h"

#define PM_LOCK   "/tmp/pacman.lck"


/* Globals */
pmhandle_t *handle = NULL;
enum __pmerrno_t pm_errno;

/*
 * Library
 */

int alpm_initialize(char *root)
{
	char str[PATH_MAX];

	ASSERT(handle == NULL, RET_ERR(PM_ERR_HANDLE_NOT_NULL, -1));

	handle = handle_new();
	if(handle == NULL) {
		RET_ERR(PM_ERR_MEMORY, -1);
	}

	/* lock db */
	if(handle->access == PM_ACCESS_RW) {
		if(_alpm_lckmk(PM_LOCK) == -1) {
			FREE(handle);
			RET_ERR(PM_ERR_HANDLE_LOCK, -1);
		}
	}

	STRNCPY(str, (root) ? root : PM_ROOT, PATH_MAX);
	/* add a trailing '/' if there isn't one */
	if(str[strlen(str)-1] != '/') {
		strcat(str, "/");
	}
	handle->root = strdup(str);

	return(0);
}

int alpm_release()
{
	PMList *i;

	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	/* unlock db */
	if(handle->access == PM_ACCESS_RW) {
		if(_alpm_lckrm(PM_LOCK)) {
			_alpm_log(PM_LOG_WARNING, "could not remove lock file %s", PM_LOCK);
			alpm_logaction("warning: could not remove lock file %s", PM_LOCK);
		}
	}

	/* close local database */
	if(handle->db_local) {
		db_close(handle->db_local);
		handle->db_local = NULL;
	}
	/* and also sync ones */
	for(i = handle->dbs_sync; i; i = i->next) {
		db_close(i->data);
		i->data = NULL;
	}

	FREEHANDLE(handle);

	return(0);
}

/*
 * Options
 */

int alpm_set_option(unsigned char parm, unsigned long data)
{
	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	return(handle_set_option(handle, parm, data));
}

int alpm_get_option(unsigned char parm, long *data)
{
	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	ASSERT(data != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	return(handle_get_option(handle, parm, data));
}

/*
 * Databases
 */

pmdb_t *alpm_db_register(char *treename)
{
	pmdb_t *db;
	int found = 0;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, NULL));
	ASSERT(treename != NULL && strlen(treename) != 0, RET_ERR(PM_ERR_WRONG_ARGS, NULL));

	if(strcmp(treename, "local") == 0) {
		if(handle->db_local != NULL) {
			found = 1;
		}
	} else {
		PMList *i;
		for(i = handle->dbs_sync; i && !found; i = i->next) {
			pmdb_t *sdb = i->data;
			if(strcmp(treename, sdb->treename) == 0) {
				found = 1;
			}
		}
	}
	if(found) {
		RET_ERR(PM_ERR_DB_NOT_NULL, NULL);
	}

	db = db_open(handle->root, handle->dbpath, treename);
	if(db == NULL) {
		/* couldn't open the db directory - try creating it */
		if(db_create(handle->root, handle->dbpath, treename) == -1) {
			RET_ERR(PM_ERR_DB_CREATE, NULL);
		}
		db = db_open(handle->root, handle->dbpath, treename);
		if(db == NULL) {
			/* couldn't open the db directory */
			RET_ERR(PM_ERR_DB_OPEN, NULL);
		}
	}

	if(strcmp(treename, "local") == 0) {
		handle->db_local = db;
	} else {
		handle->dbs_sync = pm_list_add(handle->dbs_sync, db);
	}

	return(db);
}

/* Helper function for comparing databases
 */
static int db_cmp(const void *db1, const void *db2)
{
	return(strcmp(((pmdb_t *)db1)->treename, ((pmdb_t *)db2)->treename));
}

int alpm_db_unregister(pmdb_t *db)
{
	int found = 0;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	ASSERT(db != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	if(db == handle->db_local) {
		db_close(handle->db_local);
		handle->db_local = NULL;
		found = 1;
	} else {
		pmdb_t *data;
		handle->dbs_sync = _alpm_list_remove(handle->dbs_sync, db, db_cmp, (void **)&data);
		if(data) {
			db_close(data);
			found = 1;
		}
	}

	if(!found) {
		RET_ERR(PM_ERR_DB_NOT_FOUND, -1);
	}

	return(0);
}

void *alpm_db_getinfo(PM_DB *db, unsigned char parm)
{
	void *data = NULL;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));

	switch(parm) {
		case PM_DB_TREENAME:   data = db->treename; break;
		case PM_DB_LASTUPDATE: data = db->lastupdate; break;
		default:
			data = NULL;
	}

	return(data);
}

int alpm_db_update(PM_DB *db, char *archive, char *ts)
{
	struct stat buf;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	ASSERT(db != NULL && db != handle->db_local, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	if(!pm_list_is_in(db, handle->dbs_sync)) {
		RET_ERR(PM_ERR_DB_NOT_FOUND, -1);
	}

	if(ts && strlen(ts) != 0) {
		char lastupdate[15];
		if(db_getlastupdate(db, lastupdate) != -1) {
			if(strcmp(ts, lastupdate) == 0) {
				RET_ERR(PM_ERR_DB_UPTODATE, -1);
			}
		}
	}

	if(stat(archive, &buf)) {
		/* not found */
		RET_ERR(PM_ERR_NOT_A_FILE, -1);
	}

	/* Cache needs to be rebuild */
	db_free_pkgcache(db);

	/* remove the old dir */
	_alpm_log(PM_LOG_FLOW2, "removing database %s/%s", handle->dbpath, db->treename);
	/* ORE
	We should db_remove each db entry, and not rmrf the top directory */
	_alpm_rmrf(db->path);

	/* make the new dir */
	if(db_create(handle->root, handle->dbpath, db->treename) != 0) {
		RET_ERR(PM_ERR_DB_CREATE, -1);
	}

	/* uncompress the sync database */
	/* ORE
	we should not simply unpack the archive, but better parse it and 
	db_write each entry */
	_alpm_log(PM_LOG_FLOW2, "unpacking %s", archive);
	if(_alpm_unpack(archive, db->path, NULL)) {
		RET_ERR(PM_ERR_XXX, -1);
	}

	if(ts && strlen(ts) != 0) {
		if(db_setlastupdate(db, ts) == -1) {
			RET_ERR(PM_ERR_XXX, -1);
		}
	}

	return(0);
}

pmpkg_t *alpm_db_readpkg(pmdb_t *db, char *name)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));
	ASSERT(name != NULL && strlen(name) != 0, return(NULL));

	return(db_get_pkgfromcache(db, name));
}

PMList *alpm_db_getpkgcache(pmdb_t *db)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));

	return(db_get_pkgcache(db));
}

pmgrp_t *alpm_db_readgrp(pmdb_t *db, char *name)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));
	ASSERT(name != NULL && strlen(name) != 0, return(NULL));

	return(db_get_grpfromcache(db, name));
}

PMList *alpm_db_getgrpcache(pmdb_t *db)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));

	return(db_get_grpcache(db));
}

/*
 * Packages
 */

void *alpm_pkg_getinfo(pmpkg_t *pkg, unsigned char parm)
{
	void *data = NULL;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	/* Update the cache package entry if needed */
	if(pkg->origin == PKG_FROM_CACHE) {
		switch(parm) {
			/* Desc entry */
			/* ORE
			not needed: the cache is loaded with DESC and DEPENDS by default
			case PM_PKG_NAME:
			case PM_PKG_VERSION:
			case PM_PKG_DESC:
			case PM_PKG_GROUPS:
			case PM_PKG_URL:
			case PM_PKG_LICENSE:
			case PM_PKG_ARCH:
			case PM_PKG_BUILDDATE:
			case PM_PKG_INSTALLDATE:
			case PM_PKG_PACKAGER:
			case PM_PKG_SIZE:
			case PM_PKG_REASON:
			case PM_PKG_REPLACES:
			case PM_PKG_MD5SUM:
				if(!(pkg->infolevel & INFRQ_DESC)) {
					char target[(PKG_NAME_LEN-1)+1+(PKG_VERSION_LEN-1)+1];
					snprintf(target, (PKG_NAME_LEN-1)+1+(PKG_VERSION_LEN-1)+1, "%s-%s", pkg->name, pkg->version);
					db_read(pkg->data, target, INFRQ_DESC, pkg);
				}
			break;*/
			/* Depends entry */
			/* ORE
			not needed: the cache is loaded with DESC and DEPENDS by default
			case PM_PKG_DEPENDS:
			case PM_PKG_REQUIREDBY:
			case PM_PKG_CONFLICTS:
			case PM_PKG_PROVIDES:
				if(!(pkg->infolevel & INFRQ_DEPENDS)) {
					char target[(PKG_NAME_LEN-1)+1+(PKG_VERSION_LEN-1)+1];
					snprintf(target, (PKG_NAME_LEN-1)+1+(PKG_VERSION_LEN-1)+1, "%s-%s", pkg->name, pkg->version);
					db_read(pkg->data, target, INFRQ_DEPENDS, pkg);
				}
			break;*/
			/* Files entry */
			case PM_PKG_FILES:
			case PM_PKG_BACKUP:
				if(pkg->data == handle->db_local && !(pkg->infolevel & INFRQ_FILES)) {
					char target[(PKG_NAME_LEN-1)+1+(PKG_VERSION_LEN-1)+1];
					snprintf(target, (PKG_NAME_LEN-1)+1+(PKG_VERSION_LEN-1)+1, "%s-%s", pkg->name, pkg->version);
					db_read(pkg->data, target, INFRQ_FILES, pkg);
				}
			break;
			/* Scriptlet */
			case PM_PKG_SCRIPLET:
				if(pkg->data == handle->db_local && !(pkg->infolevel & INFRQ_SCRIPLET)) {
					char target[(PKG_NAME_LEN-1)+1+(PKG_VERSION_LEN-1)+1];
					snprintf(target, (PKG_NAME_LEN-1)+1+(PKG_VERSION_LEN-1)+1, "%s-%s", pkg->name, pkg->version);
					db_read(pkg->data, target, INFRQ_SCRIPLET, pkg);
				}
			break;
		}
	}

	switch(parm) {
		case PM_PKG_NAME:        data = pkg->name; break;
		case PM_PKG_VERSION:     data = pkg->version; break;
		case PM_PKG_DESC:        data = pkg->desc; break;
		case PM_PKG_GROUPS:      data = pkg->groups; break;
		case PM_PKG_URL:         data = pkg->url; break;
		case PM_PKG_ARCH:        data = pkg->arch; break;
		case PM_PKG_BUILDDATE:   data = pkg->builddate; break;
		case PM_PKG_INSTALLDATE: data = pkg->installdate; break;
		case PM_PKG_PACKAGER:    data = pkg->packager; break;
		case PM_PKG_SIZE:        data = (void *)pkg->size; break;
		case PM_PKG_REASON:      data = (void *)(int)pkg->reason; break;
		case PM_PKG_LICENSE:     data = pkg->license; break;
		case PM_PKG_REPLACES:    data = pkg->replaces; break;
		case PM_PKG_MD5SUM:      data = pkg->md5sum; break;
		case PM_PKG_DEPENDS:     data = pkg->depends; break;
		case PM_PKG_REQUIREDBY:  data = pkg->requiredby; break;
		case PM_PKG_PROVIDES:    data = pkg->provides; break;
		case PM_PKG_CONFLICTS:   data = pkg->conflicts; break;
		case PM_PKG_FILES:       data = pkg->files; break;
		case PM_PKG_BACKUP:      data = pkg->backup; break;
		case PM_PKG_SCRIPLET:    data = (void *)(int)pkg->scriptlet; break;
		case PM_PKG_DATA:        data = pkg->data; break;
		default:
			data = NULL;
		break;
	}

	return(data);
}

int alpm_pkg_load(char *filename, pmpkg_t **pkg)
{
	/* Sanity checks */
	ASSERT(filename != NULL && strlen(filename) != 0, RET_ERR(PM_ERR_WRONG_ARGS, -1));
	ASSERT(pkg != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	*pkg = pkg_load(filename);
	if(*pkg == NULL) {
		/* pm_errno is set by pkg_load */
		return(-1);
	}

	return(0);
}

void alpm_pkg_free(pmpkg_t *pkg)
{
	pkg_free(pkg);
}

int alpm_pkg_vercmp(const char *ver1, const char *ver2)
{
	return(rpmvercmp(ver1, ver2));
}

/*
 * Groups
 */

void *alpm_grp_getinfo(pmgrp_t *grp, unsigned char parm)
{
	void *data = NULL;

	/* Sanity checks */
	ASSERT(grp != NULL, return(NULL));

	switch(parm) {
		case PM_GRP_NAME:     data = grp->name; break;
		case PM_GRP_PKGNAMES: data = grp->packages; break;
		default:
			data = NULL;
		break;
	}

	return(data);
}

/*
 * Sync operations
 */

void *alpm_sync_getinfo(pmsyncpkg_t *sync, unsigned char parm)
{
	void *data;

	/* Sanity checks */
	ASSERT(sync != NULL, return(NULL));

	switch(parm) {
		case PM_SYNC_TYPE: data = (void *)(int)sync->type; break;
		case PM_SYNC_PKG:  data = sync->pkg; break;
		case PM_SYNC_DATA: data = sync->data; break;
		default:
			data = NULL;
		break;
	}

	return(data);
}

/*
 * Transactions
 */

void *alpm_trans_getinfo(unsigned char parm)
{
	pmtrans_t *trans;
	void *data;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));

	trans = handle->trans;

	switch(parm) {
		case PM_TRANS_TYPE:     data = (void *)(int)trans->type; break;
		case PM_TRANS_FLAGS:    data = (void *)(int)trans->flags; break;
		case PM_TRANS_TARGETS:  data = trans->targets; break;
		case PM_TRANS_PACKAGES: data = trans->packages; break;
		default:
			data = NULL;
		break;
	}

	return(data);
}

int alpm_trans_init(unsigned char type, unsigned char flags, alpm_trans_cb_event event)
{
	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	ASSERT(handle->trans == NULL, RET_ERR(PM_ERR_TRANS_NOT_NULL, -1));

	handle->trans = trans_new();
	if(handle->trans == NULL) {
		RET_ERR(PM_ERR_MEMORY, -1);
	}

	return(trans_init(handle->trans, type, flags, event));
}

int alpm_trans_sysupgrade()
{
	pmtrans_t *trans;

	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	trans = handle->trans;
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_INITIALIZED, RET_ERR(PM_ERR_TRANS_NOT_INITIALIZED, -1));
	ASSERT(trans->type == PM_TRANS_TYPE_SYNC, RET_ERR(PM_ERR_XXX, -1));

	return(trans_sysupgrade(trans));
}

int alpm_trans_addtarget(char *target)
{
	pmtrans_t *trans;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	ASSERT(target != NULL && strlen(target) != 0, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	trans = handle->trans;
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_INITIALIZED, RET_ERR(PM_ERR_TRANS_NOT_INITIALIZED, -1));

	return(trans_addtarget(trans, target));
}

int alpm_trans_prepare(PMList **data)
{
	pmtrans_t *trans;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	ASSERT(data != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	trans = handle->trans;
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_INITIALIZED, RET_ERR(PM_ERR_TRANS_NOT_INITIALIZED, -1));

	return(trans_prepare(handle->trans, data));
}

int alpm_trans_commit()
{
	pmtrans_t *trans;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	trans = handle->trans;
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_PREPARED, RET_ERR(PM_ERR_TRANS_NOT_PREPARED, -1));

	/* ORE
	ASSERT(handle->access != PM_ACCESS_RW, RET_ERR(PM_ERR_BAD_PERMS, -1));*/

	return(trans_commit(handle->trans));
}

int alpm_trans_release()
{
	pmtrans_t *trans;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	trans = handle->trans;
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state != STATE_IDLE, RET_ERR(PM_ERR_TRANS_NULL, -1));

	FREETRANS(handle->trans);

	return(0);
}

/*
 * Dependencies
 */

void *alpm_dep_getinfo(pmdepmissing_t *miss, unsigned char parm)
{
	void *data;

	/* Sanity checks */
	ASSERT(miss != NULL, return(NULL));

	switch(parm) {
		case PM_DEP_TARGET:  data = (void *)(int)miss->target; break;
		case PM_DEP_TYPE:    data = (void *)(int)miss->type; break;
		case PM_DEP_MOD:     data = (void *)(int)miss->depend.mod; break;
		case PM_DEP_NAME:    data = miss->depend.name; break;
		case PM_DEP_VERSION: data = miss->depend.version; break;
		default:
			data = NULL;
		break;
	}

	return(data);
}

/*
 * Log facilities
 */

int alpm_logaction(char *fmt, ...)
{
	char str[LOG_STR_LEN];
	va_list args;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	va_start(args, fmt);
	vsnprintf(str, LOG_STR_LEN, fmt, args);
	va_end(args);

	/* ORE
	We should add a prefix to log strings depending on who called us.
	If logaction was called by the frontend:
		USER: <the frontend log>
	and if called internally:
		ALPM: <the library log>
	Moreover, the frontend should be able to choose its prefix (USER by default?):
		pacman: "PACMAN"
		kpacman: "KPACMAN"
		...
	It allows to share the log file between several frontends and to actually 
	know who does what */

	return(_alpm_log_action(handle->usesyslog, handle->logfd, str));
}

/*
 * Lists wrappers
 */

PMList *alpm_list_first(PMList *list)
{
	ASSERT(list != NULL, return(NULL));

	return(list);
}

PMList *alpm_list_next(PMList *entry)
{
	ASSERT(entry != NULL, return(NULL));

	return(entry->next);
}

void *alpm_list_getdata(PMList *entry)
{
	ASSERT(entry != NULL, return(NULL));

	return(entry->data);
}

int alpm_list_free(PMList *entry)
{
	FREELIST(entry);

	return(0);
}

/*
 * Misc wrappers
 */

char *alpm_get_md5sum(char *name)
{
	ASSERT(name != NULL, return(NULL));

	return(MDFile(name));
}

/* vim: set ts=2 sw=2 noet: */
