/*
 * alpm.h
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
#ifndef _ALPM_H
#define _ALPM_H

/*
 * Arch Linux Package Management library
 */

#define PM_EXT_PKG ".pkg.tar.gz"
#define PM_EXT_DB  ".db.tar.gz"

/* 
 * Structures (opaque)
 */

typedef struct __pmlist_t PM_LIST;
typedef struct __pmdb_t PM_DB;
typedef struct __pmpkg_t PM_PKG;
typedef struct __pmgrp_t PM_GRP;
typedef struct __pmsyncpkg_t PM_SYNCPKG;
typedef struct __pmtrans_t PM_TRANS;
typedef struct __pmdepmissing_t PM_DEPMISS;

/*
 * Library
 */

/* Version */
#define PM_VERSION "0.1.0"

int alpm_initialize(char *root);
int alpm_release();

/*
 * Logging facilities
 */

/* Levels */
#define PM_LOG_DEBUG    0x01
#define PM_LOG_ERROR    0x02
#define PM_LOG_WARNING  0x04
#define PM_LOG_FLOW1    0x08
#define PM_LOG_FLOW2    0x10
#define PM_LOG_FUNCTION 0x20

/* Log callback */
typedef void (*alpm_cb_log)(unsigned short, char *);

int alpm_logaction(char *fmt, ...);

/*
 * Options
 */

/* Parameters */
enum {
	PM_OPT_LOGCB = 1,
	PM_OPT_LOGMASK,
	PM_OPT_USESYSLOG,
	PM_OPT_ROOT,
	PM_OPT_DBPATH,
	PM_OPT_LOGFILE,
	PM_OPT_LOCALDB,
	PM_OPT_SYNCDB,
	PM_OPT_NOUPGRADE,
	PM_OPT_IGNOREPKG,
	PM_OPT_HOLDPKG
};

int alpm_set_option(unsigned char parm, unsigned long data);
int alpm_get_option(unsigned char parm, long *data);

/*
 * Databases
 */

/* Info parameters */
enum {
	PM_DB_TREENAME = 1,
	PM_DB_LASTUPDATE
};

PM_DB *alpm_db_register(char *treename);
int alpm_db_unregister(PM_DB *db);

void *alpm_db_getinfo(PM_DB *db, unsigned char parm);

int alpm_db_update(PM_DB *db, char *archive, char *ts);

PM_PKG *alpm_db_readpkg(PM_DB *db, char *name);
PM_LIST *alpm_db_getpkgcache(PM_DB *db);

PM_GRP *alpm_db_readgrp(PM_DB *db, char *name);
PM_LIST *alpm_db_getgrpcache(PM_DB *db);

/*
 * Packages
 */

/* Info parameters */
enum {
	/* Desc entry */
	PM_PKG_NAME = 1,
	PM_PKG_VERSION,
	PM_PKG_DESC,
	PM_PKG_GROUPS,
	PM_PKG_URL,
	PM_PKG_LICENSE,
	PM_PKG_ARCH,
	PM_PKG_BUILDDATE,
	PM_PKG_INSTALLDATE,
	PM_PKG_PACKAGER,
	PM_PKG_SIZE,
	PM_PKG_REASON,
	PM_PKG_REPLACES, /* Sync DB only */
	PM_PKG_MD5SUM, /* Sync DB only */
	/* Depends entry */
	PM_PKG_DEPENDS,
	PM_PKG_REQUIREDBY,
	PM_PKG_CONFLICTS,
	PM_PKG_PROVIDES,
	/* Files entry */
	PM_PKG_FILES,
	PM_PKG_BACKUP,
	/* Sciplet */
	PM_PKG_SCRIPLET,
	/* Misc */
	PM_PKG_DB
};

/* reasons -- ie, why the package was installed */
#define PM_PKG_REASON_EXPLICIT  1  /* explicitly requested by the user              */
#define PM_PKG_REASON_DEPEND    2  /* installed as a dependency for another package */

void *alpm_pkg_getinfo(PM_PKG *pkg, unsigned char parm);
int alpm_pkg_load(char *filename, PM_PKG **pkg);
int alpm_pkg_free(PM_PKG *pkg);
int alpm_pkg_vercmp(const char *ver1, const char *ver2);

/*
 * Groups
 */

/* Info parameters */
enum {
	PM_GRP_NAME = 1,
	PM_GRP_PKGNAMES
};

void *alpm_grp_getinfo(PM_GRP *grp, unsigned char parm);

/*
 * Sync
 */

/* Types */
enum {
	PM_SYNC_TYPE_REPLACE = 1,
	PM_SYNC_TYPE_UPGRADE,
	PM_SYNC_TYPE_DEPEND
};
/* Info parameters */
enum {
	PM_SYNC_TYPE = 1,
	PM_SYNC_LOCALPKG,
	PM_SYNC_SYNCPKG,
	PM_SYNC_REPLACES
};

void *alpm_sync_getinfo(PM_SYNCPKG *sync, unsigned char parm);

/*
 * Transactions
 */

/* Types */
enum {
	PM_TRANS_TYPE_ADD = 1,
	PM_TRANS_TYPE_REMOVE,
	PM_TRANS_TYPE_UPGRADE,
	PM_TRANS_TYPE_SYNC
};

/* Flags */
#define PM_TRANS_FLAG_NODEPS  0x01
#define PM_TRANS_FLAG_FORCE   0x02
#define PM_TRANS_FLAG_NOSAVE  0x04
#define PM_TRANS_FLAG_FRESHEN 0x08
#define PM_TRANS_FLAG_CASCADE 0x10
#define PM_TRANS_FLAG_RECURSE 0x20
#define PM_TRANS_FLAG_DBONLY  0x40
#define PM_TRANS_FLAG_SYSUPG  0x80

/* Events */
enum {
	PM_TRANS_EVT_CHECKDEPS_START = 1,
	PM_TRANS_EVT_CHECKDEPS_DONE,
	PM_TRANS_EVT_FILECONFLICTS_START,
	PM_TRANS_EVT_FILECONFLICTS_DONE,
	PM_TRANS_EVT_RESOLVEDEPS_START,
	PM_TRANS_EVT_RESOLVEDEPS_DONE,
	PM_TRANS_EVT_ADD_START,
	PM_TRANS_EVT_ADD_DONE,
	PM_TRANS_EVT_REMOVE_START,
	PM_TRANS_EVT_REMOVE_DONE,
	PM_TRANS_EVT_UPGRADE_START,
	PM_TRANS_EVT_UPGRADE_DONE
};

/* Event callback */
typedef void (*alpm_trans_cb)(unsigned short, void *, void *);

/* Info parameters */
enum {
	PM_TRANS_TYPE = 1,
	PM_TRANS_FLAGS,
	PM_TRANS_TARGETS,
	PM_TRANS_INSTALLQ,
	PM_TRANS_REMOVEQ
};

void *alpm_trans_getinfo(unsigned char parm);
int alpm_trans_init(unsigned char type, unsigned char flags, alpm_trans_cb cb);
int alpm_trans_addtarget(char *target);
int alpm_trans_prepare(PM_LIST **data);
int alpm_trans_commit();
int alpm_trans_release();

/*
 * Dependencies
 */

enum {
	PM_DEP_MOD_ANY = 1,
	PM_DEP_MOD_EQ,
	PM_DEP_MOD_GE,
	PM_DEP_MOD_LE
};
enum {
	PM_DEP_TYPE_DEPEND = 1,
	PM_DEP_TYPE_REQUIRED,
	PM_DEP_TYPE_CONFLICT
};
/* Dependencies parameters */
enum {
	PM_DEP_TARGET = 1,
	PM_DEP_TYPE,
	PM_DEP_MOD,
	PM_DEP_NAME,
	PM_DEP_VERSION
};

void *alpm_dep_getinfo(PM_DEPMISS *miss, unsigned char parm);

/*
 * PM_LIST helpers
 */
PM_LIST *alpm_list_first(PM_LIST *list);
PM_LIST *alpm_list_next(PM_LIST *entry);
void *alpm_list_getdata(PM_LIST *entry);
int alpm_list_free(PM_LIST *entry);

/*
 * Helpers
 */
 
char *alpm_get_md5sum(char *name);

/*
 * Errors
 */

extern enum __pmerrno_t {
	PM_ERR_NOERROR = 1,
	PM_ERR_MEMORY,
	PM_ERR_SYSTEM,
	PM_ERR_BADPERMS,
	PM_ERR_NOT_A_FILE,
	PM_ERR_WRONG_ARGS,
	/* Interface */
	PM_ERR_HANDLE_NULL,
	PM_ERR_HANDLE_NOT_NULL,
	PM_ERR_HANDLE_LOCK,
	/* Databases */
	PM_ERR_DB_OPEN,
	PM_ERR_DB_CREATE,
	PM_ERR_DB_NULL,
	PM_ERR_DB_NOT_FOUND,
	PM_ERR_DB_NOT_NULL,
	PM_ERR_DB_WRITE,
	PM_ERR_DB_UPTODATE,
	/* Cache */
	PM_ERR_CACHE_NULL,
	/* Configuration */
	PM_ERR_OPT_LOGFILE,
	PM_ERR_OPT_DBPATH,
	PM_ERR_OPT_LOCALDB,
	PM_ERR_OPT_SYNCDB,
	PM_ERR_OPT_USESYSLOG,
	/* Transactions */
	PM_ERR_TRANS_NOT_NULL,
	PM_ERR_TRANS_NULL,
	PM_ERR_TRANS_DUP_TARGET,
	PM_ERR_TRANS_INITIALIZED,
	PM_ERR_TRANS_NOT_INITIALIZED,
	PM_ERR_TRANS_NOT_PREPARED,
	PM_ERR_TRANS_ABORT,
	/* Packages */
	PM_ERR_PKG_NOT_FOUND,
	PM_ERR_PKG_INVALID,
	PM_ERR_PKG_OPEN,
	PM_ERR_PKG_LOAD,
	PM_ERR_PKG_INSTALLED,
	PM_ERR_PKG_CANT_FRESH,
	PM_ERR_INVALID_NAME,
	/* Groups */
	PM_ERR_GRP_NOT_FOUND,
	/* Dependencies */
	PM_ERR_UNSATISFIED_DEPS,
	PM_ERR_CONFLICTING_DEPS,
	PM_ERR_UNRESOLVABLE_DEPS,
	PM_ERR_FILE_CONFLICTS,
	/* Misc */
	PM_ERR_USER_ABORT,
	PM_ERR_INTERNAL_ERROR,
	PM_ERR_XXX
} pm_errno;

char *alpm_strerror(int err);

#endif /* _ALPM_H */

/* vim: set ts=2 sw=2 noet: */
