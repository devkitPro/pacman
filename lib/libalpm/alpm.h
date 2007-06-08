/*
 * alpm.h
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
#ifndef _ALPM_H
#define _ALPM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h> /* for time_t */
#include <stdarg.h> /* for va_list */

/*
 * Arch Linux Package Management library
 */

/* 
 * Structures
 */

typedef struct __alpm_list_t alpm_list_t;

typedef struct __pmdb_t pmdb_t;
typedef struct __pmpkg_t pmpkg_t;
typedef struct __pmgrp_t pmgrp_t;
typedef struct __pmserver_t pmserver_t;
typedef struct __pmtrans_t pmtrans_t;
typedef struct __pmsyncpkg_t pmsyncpkg_t;
typedef struct __pmdepend_t pmdepend_t;
typedef struct __pmdepmissing_t pmdepmissing_t;
typedef struct __pmconflict_t pmconflict_t;

/*
 * Library
 */

int alpm_initialize();
int alpm_release(void);

/*
 * Logging facilities
 */

/* Levels */
typedef enum _pmloglevel_t {
	PM_LOG_ERROR    = 0x01,
	PM_LOG_WARNING  = 0x02,
	PM_LOG_DEBUG    = 0x04,
	PM_LOG_FUNCTION = 0x08
} pmloglevel_t;

typedef void (*alpm_cb_log)(pmloglevel_t, char *, va_list);
int alpm_logaction(char *fmt, ...);

/*
 * Downloading
 */

typedef void (*alpm_cb_download)(const char *filename, int xfered, int total);

/*
 * Options
 */

#define PM_DLFNM_LEN 22

alpm_cb_log alpm_option_get_logcb();
void alpm_option_set_logcb(alpm_cb_log cb);

alpm_cb_download alpm_option_get_dlcb();
void alpm_option_set_dlcb(alpm_cb_download cb);

const char *alpm_option_get_root();
void alpm_option_set_root(const char *root);

const char *alpm_option_get_dbpath();
void alpm_option_set_dbpath(const char *dbpath);

const char *alpm_option_get_cachedir();
void alpm_option_set_cachedir(const char *cachedir);

const char *alpm_option_get_logfile();
void alpm_option_set_logfile(const char *logfile);

const char *alpm_option_get_lockfile();
void alpm_option_set_lockfile(const char *lockfile);

unsigned short alpm_option_get_usesyslog();
void alpm_option_set_usesyslog(unsigned short usesyslog);

alpm_list_t *alpm_option_get_noupgrades();
void alpm_option_add_noupgrade(const char *pkg);
void alpm_option_set_noupgrades(alpm_list_t *noupgrade);

alpm_list_t *alpm_option_get_noextracts();
void alpm_option_add_noextract(const char *pkg);
void alpm_option_set_noextracts(alpm_list_t *noextract);

alpm_list_t *alpm_option_get_ignorepkgs();
void alpm_option_add_ignorepkg(const char *pkg);
void alpm_option_set_ignorepkgs(alpm_list_t *ignorepkgs);

alpm_list_t *alpm_option_get_holdpkgs();
void alpm_option_add_holdpkg(const char *pkg);
void alpm_option_set_holdpkgs(alpm_list_t *holdpkgs);

time_t alpm_option_get_upgradedelay();
void alpm_option_set_upgradedelay(time_t delay);

const char *alpm_option_get_xfercommand();
void alpm_option_set_xfercommand(const char *cmd);

unsigned short alpm_option_get_nopassiveftp();
void alpm_option_set_nopassiveftp(unsigned short nopasv);

pmdb_t *alpm_option_get_localdb();
alpm_list_t *alpm_option_get_syncdbs();

/*
 * Databases
 */

pmdb_t *alpm_db_register(const char *treename);
int alpm_db_unregister(pmdb_t *db);

const char *alpm_db_get_name(const pmdb_t *db);
const char *alpm_db_get_url(const pmdb_t *db);

int alpm_db_setserver(pmdb_t *db, const char *url);

int alpm_db_update(int level, pmdb_t *db);

pmpkg_t *alpm_db_get_pkg(pmdb_t *db, const char *name);
alpm_list_t *alpm_db_getpkgcache(pmdb_t *db);
alpm_list_t *alpm_db_whatprovides(pmdb_t *db, const char *name);

pmgrp_t *alpm_db_readgrp(pmdb_t *db, const char *name);
alpm_list_t *alpm_db_getgrpcache(pmdb_t *db);
alpm_list_t *alpm_db_search(pmdb_t *db, const alpm_list_t* needles);

alpm_list_t *alpm_db_get_upgrades();

/*
 * Packages
 */

/* Info parameters */

/* reasons -- ie, why the package was installed */
typedef enum _pmpkgreason_t {
	PM_PKG_REASON_EXPLICIT = 0,  /* explicitly requested by the user */
	PM_PKG_REASON_DEPEND = 1  /* installed as a dependency for another package */
} pmpkgreason_t;

/* package name formats */
/*
typedef enum _pmpkghasarch_t {
  PM_PKG_WITHOUT_ARCH = 0,  / pkgname-pkgver-pkgrel, used under PM_DBPATH /
  PM_PKG_WITH_ARCH = 1  / pkgname-pkgver-pkgrel-arch, used under PM_CACHEDIR /
} pmpkghasarch_t;
*/

int alpm_pkg_load(const char *filename, pmpkg_t **pkg);
int alpm_pkg_free(pmpkg_t *pkg);
int alpm_pkg_checkmd5sum(pmpkg_t *pkg);
int alpm_pkg_checksha1sum(pmpkg_t *pkg);
char *alpm_fetch_pkgurl(const char *url);
int alpm_pkg_vercmp(const char *ver1, const char *ver2);
char *alpm_pkg_name_hasarch(const char *pkgname);

const char *alpm_pkg_get_filename(pmpkg_t *pkg);
const char *alpm_pkg_get_name(pmpkg_t *pkg);
const char *alpm_pkg_get_version(pmpkg_t *pkg);
const char *alpm_pkg_get_desc(pmpkg_t *pkg);
const char *alpm_pkg_get_url(pmpkg_t *pkg);
const char *alpm_pkg_get_builddate(pmpkg_t *pkg);
const char *alpm_pkg_get_buildtype(pmpkg_t *pkg);
const char *alpm_pkg_get_installdate(pmpkg_t *pkg);
const char *alpm_pkg_get_packager(pmpkg_t *pkg);
const char *alpm_pkg_get_md5sum(pmpkg_t *pkg);
const char *alpm_pkg_get_sha1sum(pmpkg_t *pkg);
const char *alpm_pkg_get_arch(pmpkg_t *pkg);
unsigned long alpm_pkg_get_size(pmpkg_t *pkg);
unsigned long alpm_pkg_get_isize(pmpkg_t *pkg);
pmpkgreason_t alpm_pkg_get_reason(pmpkg_t *pkg);
alpm_list_t *alpm_pkg_get_licenses(pmpkg_t *pkg);
alpm_list_t *alpm_pkg_get_groups(pmpkg_t *pkg);
alpm_list_t *alpm_pkg_get_depends(pmpkg_t *pkg);
alpm_list_t *alpm_pkg_get_requiredby(pmpkg_t *pkg);
alpm_list_t *alpm_pkg_get_conflicts(pmpkg_t *pkg);
alpm_list_t *alpm_pkg_get_provides(pmpkg_t *pkg);
alpm_list_t *alpm_pkg_get_replaces(pmpkg_t *pkg);
alpm_list_t *alpm_pkg_get_files(pmpkg_t *pkg);
alpm_list_t *alpm_pkg_get_backup(pmpkg_t *pkg);
unsigned short alpm_pkg_has_scriptlet(pmpkg_t *pkg);

/*
 * Groups
 */
const char *alpm_grp_get_name(const pmgrp_t *grp);
const alpm_list_t *alpm_grp_get_pkgs(const pmgrp_t *grp);

/*
 * Sync
 */

/* Types */
typedef enum _pmsynctype_t {
	PM_SYNC_TYPE_REPLACE = 1,
	PM_SYNC_TYPE_UPGRADE,
	PM_SYNC_TYPE_DEPEND
} pmsynctype_t;

pmsynctype_t alpm_sync_get_type(const pmsyncpkg_t *sync);
pmpkg_t *alpm_sync_get_pkg(const pmsyncpkg_t *sync);
void *alpm_sync_get_data(const pmsyncpkg_t *sync);

/*
 * Transactions
 */

/* Types */
typedef enum _pmtranstype_t {
	PM_TRANS_TYPE_ADD = 1,
	PM_TRANS_TYPE_REMOVE,
	PM_TRANS_TYPE_UPGRADE,
	PM_TRANS_TYPE_SYNC
} pmtranstype_t;

/* Flags */
typedef enum _pmtransflag_t {
	PM_TRANS_FLAG_NODEPS = 0x01,
	PM_TRANS_FLAG_FORCE = 0x02,
	PM_TRANS_FLAG_NOSAVE = 0x04,
	PM_TRANS_FLAG_FRESHEN = 0x08,
	PM_TRANS_FLAG_CASCADE = 0x10,
	PM_TRANS_FLAG_RECURSE = 0x20,
	PM_TRANS_FLAG_DBONLY = 0x40,
	PM_TRANS_FLAG_DEPENDSONLY = 0x80,
	PM_TRANS_FLAG_ALLDEPS = 0x100,
	PM_TRANS_FLAG_DOWNLOADONLY = 0x200,
	PM_TRANS_FLAG_NOSCRIPTLET = 0x400,
	PM_TRANS_FLAG_NOCONFLICTS = 0x800,
	PM_TRANS_FLAG_PRINTURIS = 0x1000
} pmtransflag_t;

/* Transaction Events */
typedef enum _pmtransevt_t {
	PM_TRANS_EVT_CHECKDEPS_START = 1,
	PM_TRANS_EVT_CHECKDEPS_DONE,
	PM_TRANS_EVT_FILECONFLICTS_START,
	PM_TRANS_EVT_FILECONFLICTS_DONE,
	PM_TRANS_EVT_RESOLVEDEPS_START,
	PM_TRANS_EVT_RESOLVEDEPS_DONE,
	PM_TRANS_EVT_INTERCONFLICTS_START,
	PM_TRANS_EVT_INTERCONFLICTS_DONE,
	PM_TRANS_EVT_ADD_START,
	PM_TRANS_EVT_ADD_DONE,
	PM_TRANS_EVT_REMOVE_START,
	PM_TRANS_EVT_REMOVE_DONE,
	PM_TRANS_EVT_UPGRADE_START,
	PM_TRANS_EVT_UPGRADE_DONE,
	PM_TRANS_EVT_EXTRACT_DONE,
	PM_TRANS_EVT_INTEGRITY_START,
	PM_TRANS_EVT_INTEGRITY_DONE,
	PM_TRANS_EVT_SCRIPTLET_INFO,
	PM_TRANS_EVT_SCRIPTLET_START,
	PM_TRANS_EVT_SCRIPTLET_DONE,
	PM_TRANS_EVT_PRINTURI,
	PM_TRANS_EVT_RETRIEVE_START,
} pmtransevt_t;

/* Transaction Conversations (ie, questions) */
typedef enum _pmtransconv_t {
	PM_TRANS_CONV_INSTALL_IGNOREPKG = 0x01,
	PM_TRANS_CONV_REPLACE_PKG = 0x02,
	PM_TRANS_CONV_CONFLICT_PKG = 0x04,
	PM_TRANS_CONV_CORRUPTED_PKG = 0x08,
	PM_TRANS_CONV_LOCAL_NEWER = 0x10,
	PM_TRANS_CONV_LOCAL_UPTODATE = 0x20,
	PM_TRANS_CONV_REMOVE_HOLDPKG = 0x40
} pmtransconv_t;

/* Transaction Progress */
typedef enum _pmtransprog_t {
	PM_TRANS_PROGRESS_ADD_START,
	PM_TRANS_PROGRESS_UPGRADE_START,
	PM_TRANS_PROGRESS_REMOVE_START,
	PM_TRANS_PROGRESS_CONFLICTS_START
} pmtransprog_t;

/* Transaction Event callback */
typedef void (*alpm_trans_cb_event)(pmtransevt_t, void *, void *);

/* Transaction Conversation callback */
typedef void (*alpm_trans_cb_conv)(pmtransconv_t, void *, void *,
                                   void *, int *);

/* Transaction Progress callback */
typedef void (*alpm_trans_cb_progress)(pmtransprog_t, const char *, int, int, int);

pmtranstype_t alpm_trans_get_type();
unsigned int alpm_trans_get_flags();
alpm_list_t * alpm_trans_get_targets();
alpm_list_t * alpm_trans_get_pkgs();
int alpm_trans_init(pmtranstype_t type, pmtransflag_t flags,
                    alpm_trans_cb_event cb_event, alpm_trans_cb_conv conv,
                    alpm_trans_cb_progress cb_progress);
int alpm_trans_sysupgrade(void);
int alpm_trans_addtarget(char *target);
int alpm_trans_prepare(alpm_list_t **data);
int alpm_trans_commit(alpm_list_t **data);
int alpm_trans_release(void);

/*
 * Dependencies and conflicts
 */

typedef enum _pmdepmod_t {
	PM_DEP_MOD_ANY = 1,
	PM_DEP_MOD_EQ,
	PM_DEP_MOD_GE,
	PM_DEP_MOD_LE
} pmdepmod_t;

typedef enum _pmdeptype_t {
	PM_DEP_TYPE_DEPEND = 1,
	PM_DEP_TYPE_CONFLICT
} pmdeptype_t;

pmdepend_t *alpm_splitdep(const char *depstring);
int alpm_depcmp(pmpkg_t *pkg, pmdepend_t *dep);

const char *alpm_dep_get_target(pmdepmissing_t *miss);
pmdeptype_t alpm_dep_get_type(pmdepmissing_t *miss);
pmdepmod_t alpm_dep_get_mod(pmdepmissing_t *miss);
const char *alpm_dep_get_name(pmdepmissing_t *miss);
const char *alpm_dep_get_version(pmdepmissing_t *miss);

/*
 * File conflicts
 */

typedef enum _pmconflicttype_t {
	PM_CONFLICT_TYPE_TARGET = 1,
	PM_CONFLICT_TYPE_FILE
} pmconflicttype_t;

const char *alpm_conflict_get_target(pmconflict_t *conflict);
pmconflicttype_t alpm_conflict_get_type(pmconflict_t *conflict);
const char *alpm_conflict_get_file(pmconflict_t *conflict);
const char *alpm_conflict_get_ctarget(pmconflict_t *conflict);

/*
 * Helpers
 */

/* checksums */
char *alpm_get_md5sum(char *name);
char *alpm_get_sha1sum(char *name);

/*
 * Errors
 */
enum _pmerrno_t {
	PM_ERR_MEMORY = 1,
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
	PM_ERR_DB_NOT_NULL,
	PM_ERR_DB_NOT_FOUND,
	PM_ERR_DB_WRITE,
	PM_ERR_DB_REMOVE,
	/* Servers */
	PM_ERR_SERVER_BAD_URL,
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
	PM_ERR_TRANS_NOT_INITIALIZED,
	PM_ERR_TRANS_NOT_PREPARED,
	PM_ERR_TRANS_ABORT,
	PM_ERR_TRANS_TYPE,
	PM_ERR_TRANS_COMMITING,
	PM_ERR_TRANS_DOWNLOADING,
	/* Packages */
	PM_ERR_PKG_NOT_FOUND,
	PM_ERR_PKG_INVALID,
	PM_ERR_PKG_OPEN,
	PM_ERR_PKG_LOAD,
	PM_ERR_PKG_INSTALLED,
	PM_ERR_PKG_CANT_FRESH,
	PM_ERR_PKG_CANT_REMOVE,
	PM_ERR_PKG_INVALID_NAME,
	PM_ERR_PKG_CORRUPTED,
	PM_ERR_PKG_REPO_NOT_FOUND,
	/* Groups */
	PM_ERR_GRP_NOT_FOUND,
	/* Dependencies */
	PM_ERR_UNSATISFIED_DEPS,
	PM_ERR_CONFLICTING_DEPS,
	PM_ERR_FILE_CONFLICTS,
	/* Misc */
	PM_ERR_USER_ABORT,
	PM_ERR_INTERNAL_ERROR,
	PM_ERR_LIBARCHIVE_ERROR,
	PM_ERR_DISK_FULL,
	PM_ERR_DB_SYNC,
	PM_ERR_RETRIEVE,
	PM_ERR_PKG_HOLD,
	PM_ERR_INVALID_REGEX,
  /* Downloading */
	PM_ERR_CONNECT_FAILED,
  PM_ERR_FORK_FAILED
};

extern enum _pmerrno_t pm_errno;

const char *alpm_strerror(int err);

#ifdef __cplusplus
}
#endif
#endif /* _ALPM_H */

/* vim: set ts=2 sw=2 noet: */
