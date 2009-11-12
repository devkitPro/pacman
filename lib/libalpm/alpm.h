/*
 * alpm.h
 *
 *  Copyright (c) 2006-2009 Pacman Development Team <pacman-dev@archlinux.org>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _ALPM_H
#define _ALPM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h> /* for off_t */
#include <time.h> /* for time_t */
#include <stdarg.h> /* for va_list */

#include <alpm_list.h>

#define DEPRECATED __attribute__((deprecated))

/*
 * Arch Linux Package Management library
 */

/*
 * Structures
 */

typedef struct __pmdb_t pmdb_t;
typedef struct __pmpkg_t pmpkg_t;
typedef struct __pmdelta_t pmdelta_t;
typedef struct __pmgrp_t pmgrp_t;
typedef struct __pmtrans_t pmtrans_t;
typedef struct __pmdepend_t pmdepend_t;
typedef struct __pmdepmissing_t pmdepmissing_t;
typedef struct __pmconflict_t pmconflict_t;
typedef struct __pmfileconflict_t pmfileconflict_t;

/*
 * Library
 */

int alpm_initialize(void);
int alpm_release(void);
const char *alpm_version(void);

/*
 * Logging facilities
 */

/* Levels */
typedef enum _pmloglevel_t {
	PM_LOG_ERROR    = 1,
	PM_LOG_WARNING  = (1 << 1),
	PM_LOG_DEBUG    = (1 << 2),
	PM_LOG_FUNCTION = (1 << 3)
} pmloglevel_t;

typedef void (*alpm_cb_log)(pmloglevel_t, char *, va_list);
int alpm_logaction(char *fmt, ...);

/*
 * Downloading
 */

typedef void (*alpm_cb_download)(const char *filename,
		off_t xfered, off_t total);
typedef void (*alpm_cb_totaldl)(off_t total);
/** A callback for downloading files
 * @param url the URL of the file to be downloaded
 * @param localpath the directory to which the file should be downloaded
 * @param force whether to force an update, even if the file is the same
 * @return 0 on success, 1 if the file exists and is identical, -1 on
 * error.
 */
typedef int (*alpm_cb_fetch)(const char *url, const char *localpath,
		int force);

/*
 * Options
 */

alpm_cb_log alpm_option_get_logcb();
void alpm_option_set_logcb(alpm_cb_log cb);

alpm_cb_download alpm_option_get_dlcb();
void alpm_option_set_dlcb(alpm_cb_download cb);

alpm_cb_fetch alpm_option_get_fetchcb();
void alpm_option_set_fetchcb(alpm_cb_fetch cb);

alpm_cb_totaldl alpm_option_get_totaldlcb();
void alpm_option_set_totaldlcb(alpm_cb_totaldl cb);

const char *alpm_option_get_root();
int alpm_option_set_root(const char *root);

const char *alpm_option_get_dbpath();
int alpm_option_set_dbpath(const char *dbpath);

alpm_list_t *alpm_option_get_cachedirs();
int alpm_option_add_cachedir(const char *cachedir);
void alpm_option_set_cachedirs(alpm_list_t *cachedirs);
int alpm_option_remove_cachedir(const char *cachedir);

const char *alpm_option_get_logfile();
int alpm_option_set_logfile(const char *logfile);

const char *alpm_option_get_lockfile();
/* no set_lockfile, path is determined from dbpath */

int alpm_option_get_usesyslog();
void alpm_option_set_usesyslog(int usesyslog);

alpm_list_t *alpm_option_get_noupgrades();
void alpm_option_add_noupgrade(const char *pkg);
void alpm_option_set_noupgrades(alpm_list_t *noupgrade);
int alpm_option_remove_noupgrade(const char *pkg);

alpm_list_t *alpm_option_get_noextracts();
void alpm_option_add_noextract(const char *pkg);
void alpm_option_set_noextracts(alpm_list_t *noextract);
int alpm_option_remove_noextract(const char *pkg);

alpm_list_t *alpm_option_get_ignorepkgs();
void alpm_option_add_ignorepkg(const char *pkg);
void alpm_option_set_ignorepkgs(alpm_list_t *ignorepkgs);
int alpm_option_remove_ignorepkg(const char *pkg);

alpm_list_t *alpm_option_get_ignoregrps();
void alpm_option_add_ignoregrp(const char *grp);
void alpm_option_set_ignoregrps(alpm_list_t *ignoregrps);
int alpm_option_remove_ignoregrp(const char *grp);

const char *alpm_option_get_arch();
void alpm_option_set_arch(const char *arch);

int alpm_option_get_usedelta();
void alpm_option_set_usedelta(int usedelta);

pmdb_t *alpm_option_get_localdb();
alpm_list_t *alpm_option_get_syncdbs();

/*
 * Databases
 */

/* Preferred interfaces db_register_local and db_register_sync */
pmdb_t *alpm_db_register_local(void);
pmdb_t *alpm_db_register_sync(const char *treename);
int alpm_db_unregister(pmdb_t *db);
int alpm_db_unregister_all(void);

const char *alpm_db_get_name(const pmdb_t *db);
const char *alpm_db_get_url(const pmdb_t *db);

int alpm_db_setserver(pmdb_t *db, const char *url);

int alpm_db_update(int level, pmdb_t *db);

pmpkg_t *alpm_db_get_pkg(pmdb_t *db, const char *name);
alpm_list_t *alpm_db_get_pkgcache(pmdb_t *db);

pmgrp_t *alpm_db_readgrp(pmdb_t *db, const char *name);
alpm_list_t *alpm_db_get_grpcache(pmdb_t *db);
alpm_list_t *alpm_db_search(pmdb_t *db, const alpm_list_t* needles);

/*
 * Packages
 */

/* Info parameters */

/* reasons -- ie, why the package was installed */
typedef enum _pmpkgreason_t {
	PM_PKG_REASON_EXPLICIT = 0,  /* explicitly requested by the user */
	PM_PKG_REASON_DEPEND = 1  /* installed as a dependency for another package */
} pmpkgreason_t;

int alpm_pkg_load(const char *filename, int full, pmpkg_t **pkg);
int alpm_pkg_free(pmpkg_t *pkg);
int alpm_pkg_checkmd5sum(pmpkg_t *pkg);
char *alpm_fetch_pkgurl(const char *url);
int alpm_pkg_vercmp(const char *a, const char *b);
alpm_list_t *alpm_pkg_compute_requiredby(pmpkg_t *pkg);

const char *alpm_pkg_get_filename(pmpkg_t *pkg);
const char *alpm_pkg_get_name(pmpkg_t *pkg);
const char *alpm_pkg_get_version(pmpkg_t *pkg);
const char *alpm_pkg_get_desc(pmpkg_t *pkg);
const char *alpm_pkg_get_url(pmpkg_t *pkg);
time_t alpm_pkg_get_builddate(pmpkg_t *pkg);
time_t alpm_pkg_get_installdate(pmpkg_t *pkg);
const char *alpm_pkg_get_packager(pmpkg_t *pkg);
const char *alpm_pkg_get_md5sum(pmpkg_t *pkg);
const char *alpm_pkg_get_arch(pmpkg_t *pkg);
off_t alpm_pkg_get_size(pmpkg_t *pkg);
off_t alpm_pkg_get_isize(pmpkg_t *pkg);
pmpkgreason_t alpm_pkg_get_reason(pmpkg_t *pkg);
alpm_list_t *alpm_pkg_get_licenses(pmpkg_t *pkg);
alpm_list_t *alpm_pkg_get_groups(pmpkg_t *pkg);
alpm_list_t *alpm_pkg_get_depends(pmpkg_t *pkg);
alpm_list_t *alpm_pkg_get_optdepends(pmpkg_t *pkg);
alpm_list_t *alpm_pkg_get_conflicts(pmpkg_t *pkg);
alpm_list_t *alpm_pkg_get_provides(pmpkg_t *pkg);
alpm_list_t *alpm_pkg_get_deltas(pmpkg_t *pkg);
alpm_list_t *alpm_pkg_get_replaces(pmpkg_t *pkg);
alpm_list_t *alpm_pkg_get_files(pmpkg_t *pkg);
alpm_list_t *alpm_pkg_get_backup(pmpkg_t *pkg);
pmdb_t *alpm_pkg_get_db(pmpkg_t *pkg);
void *alpm_pkg_changelog_open(pmpkg_t *pkg);
size_t alpm_pkg_changelog_read(void *ptr, size_t size,
		const pmpkg_t *pkg, const void *fp);
/*int alpm_pkg_changelog_feof(const pmpkg_t *pkg, void *fp);*/
int alpm_pkg_changelog_close(const pmpkg_t *pkg, void *fp);
int alpm_pkg_has_scriptlet(pmpkg_t *pkg);
int alpm_pkg_has_force(pmpkg_t *pkg);

off_t alpm_pkg_download_size(pmpkg_t *newpkg);

/*
 * Deltas
 */

const char *alpm_delta_get_from(pmdelta_t *delta);
const char *alpm_delta_get_to(pmdelta_t *delta);
const char *alpm_delta_get_filename(pmdelta_t *delta);
const char *alpm_delta_get_md5sum(pmdelta_t *delta);
off_t alpm_delta_get_size(pmdelta_t *delta);

/*
 * Groups
 */
const char *alpm_grp_get_name(const pmgrp_t *grp);
alpm_list_t *alpm_grp_get_pkgs(const pmgrp_t *grp);

/*
 * Sync
 */

pmpkg_t *alpm_sync_newversion(pmpkg_t *pkg, alpm_list_t *dbs_sync);

/*
 * Transactions
 */


/* Flags */
typedef enum _pmtransflag_t {
	PM_TRANS_FLAG_NODEPS = 1,
	PM_TRANS_FLAG_FORCE = (1 << 1),
	PM_TRANS_FLAG_NOSAVE = (1 << 2),
	/* (1 << 3) flag can go here */
	PM_TRANS_FLAG_CASCADE = (1 << 4),
	PM_TRANS_FLAG_RECURSE = (1 << 5),
	PM_TRANS_FLAG_DBONLY = (1 << 6),
	/* (1 << 7) flag can go here */
	PM_TRANS_FLAG_ALLDEPS = (1 << 8),
	PM_TRANS_FLAG_DOWNLOADONLY = (1 << 9),
	PM_TRANS_FLAG_NOSCRIPTLET = (1 << 10),
	PM_TRANS_FLAG_NOCONFLICTS = (1 << 11),
	/* (1 << 12) flag can go here */
	PM_TRANS_FLAG_NEEDED = (1 << 13),
	PM_TRANS_FLAG_ALLEXPLICIT = (1 << 14),
	PM_TRANS_FLAG_UNNEEDED = (1 << 15),
	PM_TRANS_FLAG_RECURSEALL = (1 << 16),
	PM_TRANS_FLAG_NOLOCK = (1 << 17)
} pmtransflag_t;

/**
 * @addtogroup alpm_trans
 * @{
 */
/**
 * @brief Transaction events.
 * NULL parameters are passed to in all events unless specified otherwise.
 */
typedef enum _pmtransevt_t {
	/** Dependencies will be computed for a package. */
	PM_TRANS_EVT_CHECKDEPS_START = 1,
	/** Dependencies were computed for a package. */
	PM_TRANS_EVT_CHECKDEPS_DONE,
	/** File conflicts will be computed for a package. */
	PM_TRANS_EVT_FILECONFLICTS_START,
	/** File conflicts were computed for a package. */
	PM_TRANS_EVT_FILECONFLICTS_DONE,
	/** Dependencies will be resolved for target package. */
	PM_TRANS_EVT_RESOLVEDEPS_START,
	/** Dependencies were resolved for target package. */
	PM_TRANS_EVT_RESOLVEDEPS_DONE,
	/** Inter-conflicts will be checked for target package. */
	PM_TRANS_EVT_INTERCONFLICTS_START,
	/** Inter-conflicts were checked for target package. */
	PM_TRANS_EVT_INTERCONFLICTS_DONE,
	/** Package will be installed.
	 * A pointer to the target package is passed to the callback.
	 */
	PM_TRANS_EVT_ADD_START,
	/** Package was installed.
	 * A pointer to the new package is passed to the callback.
	 */
	PM_TRANS_EVT_ADD_DONE,
	/** Package will be removed.
	 * A pointer to the target package is passed to the callback.
	 */
	PM_TRANS_EVT_REMOVE_START,
	/** Package was removed.
	 * A pointer to the removed package is passed to the callback.
	 */
	PM_TRANS_EVT_REMOVE_DONE,
	/** Package will be upgraded.
	 * A pointer to the upgraded package is passed to the callback.
	 */
	PM_TRANS_EVT_UPGRADE_START,
	/** Package was upgraded.
	 * A pointer to the new package, and a pointer to the old package is passed
	 * to the callback, respectively.
	 */
	PM_TRANS_EVT_UPGRADE_DONE,
	/** Target package's integrity will be checked. */
	PM_TRANS_EVT_INTEGRITY_START,
	/** Target package's integrity was checked. */
	PM_TRANS_EVT_INTEGRITY_DONE,
	/** Target deltas's integrity will be checked. */
	PM_TRANS_EVT_DELTA_INTEGRITY_START,
	/** Target delta's integrity was checked. */
	PM_TRANS_EVT_DELTA_INTEGRITY_DONE,
	/** Deltas will be applied to packages. */
	PM_TRANS_EVT_DELTA_PATCHES_START,
	/** Deltas were applied to packages. */
	PM_TRANS_EVT_DELTA_PATCHES_DONE,
	/** Delta patch will be applied to target package.
	 * The filename of the package and the filename of the patch is passed to the
	 * callback.
	 */
	PM_TRANS_EVT_DELTA_PATCH_START,
	/** Delta patch was applied to target package. */
	PM_TRANS_EVT_DELTA_PATCH_DONE,
	/** Delta patch failed to apply to target package. */
	PM_TRANS_EVT_DELTA_PATCH_FAILED,
	/** Scriptlet has printed information.
	 * A line of text is passed to the callback.
	 */
	PM_TRANS_EVT_SCRIPTLET_INFO,
	/** Files will be downloaded from a repository.
	 * The repository's tree name is passed to the callback.
	 */
	PM_TRANS_EVT_RETRIEVE_START,
} pmtransevt_t;
/*@}*/

/* Transaction Conversations (ie, questions) */
typedef enum _pmtransconv_t {
	PM_TRANS_CONV_INSTALL_IGNOREPKG = 1,
	PM_TRANS_CONV_REPLACE_PKG = (1 << 1),
	PM_TRANS_CONV_CONFLICT_PKG = (1 << 2),
	PM_TRANS_CONV_CORRUPTED_PKG = (1 << 3),
	PM_TRANS_CONV_LOCAL_NEWER = (1 << 4),
	PM_TRANS_CONV_REMOVE_PKGS = (1 << 5),
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

int alpm_trans_get_flags();
alpm_list_t * alpm_trans_get_add();
alpm_list_t * alpm_trans_get_remove();
int alpm_trans_init(pmtransflag_t flags,
                    alpm_trans_cb_event cb_event, alpm_trans_cb_conv conv,
                    alpm_trans_cb_progress cb_progress);
int alpm_trans_prepare(alpm_list_t **data);
int alpm_trans_commit(alpm_list_t **data);
int alpm_trans_interrupt(void);
int alpm_trans_release(void);

int alpm_sync_sysupgrade(int enable_downgrade);
int alpm_sync_target(char *target);
int alpm_sync_dbtarget(char *db, char *target);
int alpm_add_target(char *target);
int alpm_remove_target(char *target);

/*
 * Dependencies and conflicts
 */

typedef enum _pmdepmod_t {
	PM_DEP_MOD_ANY = 1,
	PM_DEP_MOD_EQ,
	PM_DEP_MOD_GE,
	PM_DEP_MOD_LE,
	PM_DEP_MOD_GT,
	PM_DEP_MOD_LT
} pmdepmod_t;

int alpm_depcmp(pmpkg_t *pkg, pmdepend_t *dep);
alpm_list_t *alpm_checkdeps(alpm_list_t *pkglist, int reversedeps,
		alpm_list_t *remove, alpm_list_t *upgrade);
alpm_list_t *alpm_deptest(pmdb_t *db, alpm_list_t *targets);

const char *alpm_miss_get_target(const pmdepmissing_t *miss);
pmdepend_t *alpm_miss_get_dep(pmdepmissing_t *miss);
const char *alpm_miss_get_causingpkg(const pmdepmissing_t *miss);

alpm_list_t *alpm_checkconflicts(alpm_list_t *pkglist);

const char *alpm_conflict_get_package1(pmconflict_t *conflict);
const char *alpm_conflict_get_package2(pmconflict_t *conflict);
const char *alpm_conflict_get_reason(pmconflict_t *conflict);

pmdepmod_t alpm_dep_get_mod(const pmdepend_t *dep);
const char *alpm_dep_get_name(const pmdepend_t *dep);
const char *alpm_dep_get_version(const pmdepend_t *dep);
char *alpm_dep_compute_string(const pmdepend_t *dep);

/*
 * File conflicts
 */

typedef enum _pmfileconflicttype_t {
	PM_FILECONFLICT_TARGET = 1,
	PM_FILECONFLICT_FILESYSTEM
} pmfileconflicttype_t;

const char *alpm_fileconflict_get_target(pmfileconflict_t *conflict);
pmfileconflicttype_t alpm_fileconflict_get_type(pmfileconflict_t *conflict);
const char *alpm_fileconflict_get_file(pmfileconflict_t *conflict);
const char *alpm_fileconflict_get_ctarget(pmfileconflict_t *conflict);

/*
 * Helpers
 */

/* checksums */
char *alpm_compute_md5sum(const char *name);

/*
 * Errors
 */
enum _pmerrno_t {
	PM_ERR_MEMORY = 1,
	PM_ERR_SYSTEM,
	PM_ERR_BADPERMS,
	PM_ERR_NOT_A_FILE,
	PM_ERR_NOT_A_DIR,
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
	PM_ERR_SERVER_NONE,
	/* Transactions */
	PM_ERR_TRANS_NOT_NULL,
	PM_ERR_TRANS_NULL,
	PM_ERR_TRANS_DUP_TARGET,
	PM_ERR_TRANS_NOT_INITIALIZED,
	PM_ERR_TRANS_NOT_PREPARED,
	PM_ERR_TRANS_ABORT,
	PM_ERR_TRANS_TYPE,
	PM_ERR_TRANS_NOT_LOCKED,
	/* Packages */
	PM_ERR_PKG_NOT_FOUND,
	PM_ERR_PKG_IGNORED,
	PM_ERR_PKG_INVALID,
	PM_ERR_PKG_OPEN,
	PM_ERR_PKG_CANT_REMOVE,
	PM_ERR_PKG_INVALID_NAME,
	PM_ERR_PKG_INVALID_ARCH,
	PM_ERR_PKG_REPO_NOT_FOUND,
	/* Deltas */
	PM_ERR_DLT_INVALID,
	PM_ERR_DLT_PATCHFAILED,
	/* Dependencies */
	PM_ERR_UNSATISFIED_DEPS,
	PM_ERR_CONFLICTING_DEPS,
	PM_ERR_FILE_CONFLICTS,
	/* Misc */
	PM_ERR_RETRIEVE,
	PM_ERR_INVALID_REGEX,
	/* External library errors */
	PM_ERR_LIBARCHIVE,
	PM_ERR_LIBFETCH,
	PM_ERR_EXTERNAL_DOWNLOAD
};

extern enum _pmerrno_t pm_errno;

const char *alpm_strerror(int err);
const char *alpm_strerrorlast(void);

#ifdef __cplusplus
}
#endif
#endif /* _ALPM_H */

/* vim: set ts=2 sw=2 noet: */
