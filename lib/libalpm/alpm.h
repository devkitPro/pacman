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

/*
 * Arch Linux Package Management library
 */

#define PM_ROOT     "/"
#define PM_DBPATH   "var/lib/pacman"
#define PM_CACHEDIR "var/cache/pacman/pkg"
#define PM_LOCK     "tmp/pacman.lck"


#define PM_EXT_PKG ".pkg.tar.gz"
#define PM_EXT_DB  ".db.tar.gz"

/* 
 * Structures
 */

typedef struct __pmlist_t pmlist_t;
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

int alpm_initialize(const char *root);
int alpm_release(void);

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
#define PM_LOG_DOWNLOAD 0x40

typedef void (*alpm_cb_log)(unsigned short, char *);
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

unsigned char alpm_option_get_logmask();
void alpm_option_set_logmask(unsigned char mask);

const char *alpm_option_get_root();
void alpm_option_set_root(const char *root);

const char *alpm_option_get_dbpath();
void alpm_option_set_dbpath(const char *dbpath);

const char *alpm_option_get_cachedir();
void alpm_option_set_cachedir(const char *cachedir);

const char *alpm_option_get_logfile();
void alpm_option_set_logfile(const char *logfile);

unsigned char alpm_option_get_usesyslog();
void alpm_option_set_usesyslog(unsigned char usesyslog);

pmlist_t *alpm_option_get_noupgrades();
void alpm_option_add_noupgrade(char *pkg);
void alpm_option_set_noupgrades(pmlist_t *noupgrade);

pmlist_t *alpm_option_get_noextracts();
void alpm_option_add_noextract(char *pkg);
void alpm_option_set_noextracts(pmlist_t *noextract);

pmlist_t *alpm_option_get_ignorepkgs();
void alpm_option_add_ignorepkg(char *pkg);
void alpm_option_set_ignorepkgs(pmlist_t *ignorepkgs);

pmlist_t *alpm_option_get_holdpkgs();
void alpm_option_add_holdpkg(char *pkg);
void alpm_option_set_holdpkgs(pmlist_t *holdpkgs);

time_t alpm_option_get_upgradedelay();
void alpm_option_set_upgradedelay(time_t delay);

const char *alpm_option_get_xfercommand();
void alpm_option_set_xfercommand(const char *cmd);

unsigned short alpm_option_get_nopassiveftp();
void alpm_option_set_nopassiveftp(unsigned short nopasv);

unsigned short alpm_option_get_chomp();
void alpm_option_set_chomp(unsigned short chomp);

pmlist_t *alpm_option_get_needles();
void alpm_option_add_needle(char *needle);
void alpm_option_set_needles(pmlist_t *needles);

unsigned short alpm_option_get_usecolor();
void alpm_option_set_usecolor(unsigned short usecolor);

/*
 * Databases
 */

/* Database registration callback */
typedef void (*alpm_cb_db_register)(char *, pmdb_t *);

pmdb_t *alpm_db_register(char *treename);
int alpm_db_unregister(pmdb_t *db);

const char *alpm_db_get_name(pmdb_t *db);
const char *alpm_db_get_url(pmdb_t *db);

int alpm_db_setserver(pmdb_t *db, const char *url);

int alpm_db_update(int level, pmdb_t *db);

pmpkg_t *alpm_db_readpkg(pmdb_t *db, char *name);
pmlist_t *alpm_db_getpkgcache(pmdb_t *db);
pmlist_t *alpm_db_whatprovides(pmdb_t *db, char *name);

pmgrp_t *alpm_db_readgrp(pmdb_t *db, char *name);
pmlist_t *alpm_db_getgrpcache(pmdb_t *db);
pmlist_t *alpm_db_search(pmdb_t *db);

/*
 * Packages
 */

/* Info parameters */

/* reasons -- ie, why the package was installed */
#define PM_PKG_REASON_EXPLICIT  0  /* explicitly requested by the user */
#define PM_PKG_REASON_DEPEND    1  /* installed as a dependency for another package */

/* package name formats */
#define PM_PKG_WITHOUT_ARCH 0 /* pkgname-pkgver-pkgrel, used under PM_DBPATH */
#define PM_PKG_WITH_ARCH    1 /* ie, pkgname-pkgver-pkgrel-arch, used under PM_CACHEDIR */

int alpm_pkg_load(char *filename, pmpkg_t **pkg);
int alpm_pkg_free(pmpkg_t *pkg);
int alpm_pkg_checkmd5sum(pmpkg_t *pkg);
int alpm_pkg_checksha1sum(pmpkg_t *pkg);
char *alpm_fetch_pkgurl(char *url);
int alpm_parse_config(char *file, alpm_cb_db_register callback, const char *this_section);
int alpm_pkg_vercmp(const char *ver1, const char *ver2);
char *alpm_pkg_name_hasarch(char *pkgname);

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
unsigned char alpm_pkg_get_reason(pmpkg_t *pkg);
pmlist_t *alpm_pkg_get_licenses(pmpkg_t *pkg);
pmlist_t *alpm_pkg_get_groups(pmpkg_t *pkg);
pmlist_t *alpm_pkg_get_depends(pmpkg_t *pkg);
pmlist_t *alpm_pkg_get_removes(pmpkg_t *pkg);
pmlist_t *alpm_pkg_get_requiredby(pmpkg_t *pkg);
pmlist_t *alpm_pkg_get_conflicts(pmpkg_t *pkg);
pmlist_t *alpm_pkg_get_provides(pmpkg_t *pkg);
pmlist_t *alpm_pkg_get_replaces(pmpkg_t *pkg);
pmlist_t *alpm_pkg_get_files(pmpkg_t *pkg);
pmlist_t *alpm_pkg_get_backup(pmpkg_t *pkg);
unsigned char alpm_pkg_has_scriptlet(pmpkg_t *pkg);

/*
 * Groups
 */
const char *alpm_grp_get_name(pmgrp_t *grp);
pmlist_t *alpm_grp_get_packages(pmgrp_t *grp);

/*
 * Sync
 */

/* Types */
enum {
	PM_SYNC_TYPE_REPLACE = 1,
	PM_SYNC_TYPE_UPGRADE,
	PM_SYNC_TYPE_DEPEND
};

unsigned char alpm_sync_get_type(pmsyncpkg_t *sync);
pmpkg_t *alpm_sync_get_package(pmsyncpkg_t *sync);
void *alpm_sync_get_data(pmsyncpkg_t *sync);

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
#define PM_TRANS_FLAG_DEPENDSONLY 0x80
#define PM_TRANS_FLAG_ALLDEPS 0x100
#define PM_TRANS_FLAG_DOWNLOADONLY 0x200
#define PM_TRANS_FLAG_NOSCRIPTLET 0x400
#define PM_TRANS_FLAG_NOCONFLICTS 0x800
#define PM_TRANS_FLAG_PRINTURIS 0x1000

/* Transaction Events */
enum {
	PM_TRANS_EVT_CHECKDEPS_START = 1,
	PM_TRANS_EVT_CHECKDEPS_DONE,
	PM_TRANS_EVT_FILECONFLICTS_START,
	PM_TRANS_EVT_FILECONFLICTS_DONE,
	PM_TRANS_EVT_CLEANUP_START,
	PM_TRANS_EVT_CLEANUP_DONE,
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
	PM_TRANS_EVT_RETRIEVE_LOCAL
};

/* Transaction Conversations (ie, questions) */
enum {
	PM_TRANS_CONV_INSTALL_IGNOREPKG = 0x01,
	PM_TRANS_CONV_REPLACE_PKG = 0x02,
	PM_TRANS_CONV_CONFLICT_PKG = 0x04,
	PM_TRANS_CONV_CORRUPTED_PKG = 0x08,
	PM_TRANS_CONV_LOCAL_NEWER = 0x10,
	PM_TRANS_CONV_LOCAL_UPTODATE = 0x20,
	PM_TRANS_CONV_REMOVE_HOLDPKG = 0x40
};

/* Transaction Progress */
enum {
	PM_TRANS_PROGRESS_ADD_START,
	PM_TRANS_PROGRESS_UPGRADE_START,
	PM_TRANS_PROGRESS_REMOVE_START,
	PM_TRANS_PROGRESS_CONFLICTS_START
};

/* Transaction Event callback */
typedef void (*alpm_trans_cb_event)(unsigned char, void *, void *);

/* Transaction Conversation callback */
typedef void (*alpm_trans_cb_conv)(unsigned char, void *, void *, void *, int *);

/* Transaction Progress callback */
typedef void (*alpm_trans_cb_progress)(unsigned char, char *, int, int, int);

unsigned char alpm_trans_get_type();
unsigned int alpm_trans_get_flags();
pmlist_t * alpm_trans_get_targets();
pmlist_t * alpm_trans_get_packages();
int alpm_trans_init(unsigned char type, unsigned int flags, alpm_trans_cb_event cb_event, alpm_trans_cb_conv conv, alpm_trans_cb_progress cb_progress);
int alpm_trans_sysupgrade(void);
int alpm_trans_addtarget(char *target);
int alpm_trans_prepare(pmlist_t **data);
int alpm_trans_commit(pmlist_t **data);
int alpm_trans_release(void);

/*
 * Dependencies and conflicts
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

const char *alpm_dep_get_target(pmdepmissing_t *miss);
unsigned char alpm_dep_get_type(pmdepmissing_t *miss);
unsigned char alpm_dep_get_mod(pmdepmissing_t *miss);
const char *alpm_dep_get_name(pmdepmissing_t *miss);
const char *alpm_dep_get_version(pmdepmissing_t *miss);

/*
 * File conflicts
 */

enum {
	PM_CONFLICT_TYPE_TARGET = 1,
	PM_CONFLICT_TYPE_FILE
};

const char *alpm_conflict_get_target(pmconflict_t *conflict);
unsigned char alpm_conflict_get_type(pmconflict_t *conflict);
const char *alpm_conflict_get_file(pmconflict_t *conflict);
const char *alpm_conflict_get_ctarget(pmconflict_t *conflict);

/*
 * Helpers
 */
 
/* pmlist_t */
pmlist_t *alpm_list_first(pmlist_t *list);
pmlist_t *alpm_list_next(pmlist_t *entry);
#define alpm_list_data(type, list) (type)alpm_list_getdata((list))
void *alpm_list_getdata(const pmlist_t *entry);
int alpm_list_free(pmlist_t *entry);
int alpm_list_free_outer(pmlist_t *entry);
int alpm_list_count(const pmlist_t *list);

/* md5sums */
char *alpm_get_md5sum(char *name);
char *alpm_get_sha1sum(char *name);

/*
 * Errors
 */
enum __pmerrno_t {
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
	PM_ERR_SERVER_BAD_LOCATION,
	PM_ERR_SERVER_PROTOCOL_UNSUPPORTED,
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
	/* Packages */
	PM_ERR_PKG_NOT_FOUND,
	PM_ERR_PKG_INVALID,
	PM_ERR_PKG_OPEN,
	PM_ERR_PKG_LOAD,
	PM_ERR_PKG_INSTALLED,
	PM_ERR_PKG_CANT_FRESH,
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
	/* Configuration file */
	PM_ERR_CONF_BAD_SECTION,
	PM_ERR_CONF_LOCAL,
	PM_ERR_CONF_BAD_SYNTAX,
	PM_ERR_CONF_DIRECTIVE_OUTSIDE_SECTION,
	PM_ERR_INVALID_REGEX,
	PM_ERR_TRANS_DOWNLOADING,
  /* Downloading */
	PM_ERR_CONNECT_FAILED,
  PM_ERR_FORK_FAILED
};

extern enum __pmerrno_t pm_errno;

char *alpm_strerror(int err);

#ifdef __cplusplus
}
#endif
#endif /* _ALPM_H */

/* vim: set ts=2 sw=2 noet: */
