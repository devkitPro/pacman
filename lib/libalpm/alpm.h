/*
 * alpm.h
 *
 *  Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
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

/** @addtogroup alpm_api Public API
 * The libalpm Public API
 * @{
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

typedef void (*alpm_cb_log)(pmloglevel_t, const char *, va_list);
int alpm_logaction(const char *fmt, ...);

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

/** Fetch a remote pkg.
 * @param url URL of the package to download
 * @return the downloaded filepath on success, NULL on error
 */
char *alpm_fetch_pkgurl(const char *url);

/** @addtogroup alpm_api_options Options
 * Libalpm option getters and setters
 * @{
 */

/** @name The logging callback. */
/* @{ */
alpm_cb_log alpm_option_get_logcb(void);
void alpm_option_set_logcb(alpm_cb_log cb);
/* @} */

/** Get/set the download progress callback. */
alpm_cb_download alpm_option_get_dlcb(void);
void alpm_option_set_dlcb(alpm_cb_download cb);

/** Get/set the downloader callback. */
alpm_cb_fetch alpm_option_get_fetchcb(void);
void alpm_option_set_fetchcb(alpm_cb_fetch cb);

/** Get/set the callback used when download size is known. */
alpm_cb_totaldl alpm_option_get_totaldlcb(void);
void alpm_option_set_totaldlcb(alpm_cb_totaldl cb);

/** Get/set the root of the destination filesystem. */
const char *alpm_option_get_root(void);
int alpm_option_set_root(const char *root);

/** Get/set the path to the database directory. */
const char *alpm_option_get_dbpath(void);
int alpm_option_set_dbpath(const char *dbpath);

/** Get/set the list of package cache directories. */
alpm_list_t *alpm_option_get_cachedirs(void);
void alpm_option_set_cachedirs(alpm_list_t *cachedirs);

/** Add a single directory to the package cache paths. */
int alpm_option_add_cachedir(const char *cachedir);

/** Remove a single directory from the package cache paths. */
int alpm_option_remove_cachedir(const char *cachedir);

/** Get/set the logfile name. */
const char *alpm_option_get_logfile(void);
int alpm_option_set_logfile(const char *logfile);

/** Get the name of the database lock file.
 *
 * This properly is read-only, and determined from
 * the database path.
 *
 * @sa alpm_option_set_dbpath(const char*)
 */
const char *alpm_option_get_lockfile(void);

/** Get/set whether to use syslog (0 is FALSE, TRUE otherwise). */
int alpm_option_get_usesyslog(void);
void alpm_option_set_usesyslog(int usesyslog);

alpm_list_t *alpm_option_get_noupgrades(void);
void alpm_option_add_noupgrade(const char *pkg);
void alpm_option_set_noupgrades(alpm_list_t *noupgrade);
int alpm_option_remove_noupgrade(const char *pkg);

alpm_list_t *alpm_option_get_noextracts(void);
void alpm_option_add_noextract(const char *pkg);
void alpm_option_set_noextracts(alpm_list_t *noextract);
int alpm_option_remove_noextract(const char *pkg);

alpm_list_t *alpm_option_get_ignorepkgs(void);
void alpm_option_add_ignorepkg(const char *pkg);
void alpm_option_set_ignorepkgs(alpm_list_t *ignorepkgs);
int alpm_option_remove_ignorepkg(const char *pkg);

alpm_list_t *alpm_option_get_ignoregrps(void);
void alpm_option_add_ignoregrp(const char *grp);
void alpm_option_set_ignoregrps(alpm_list_t *ignoregrps);
int alpm_option_remove_ignoregrp(const char *grp);

/** Get/set the targeted architecture. */
const char *alpm_option_get_arch(void);
void alpm_option_set_arch(const char *arch);

int alpm_option_get_usedelta(void);
void alpm_option_set_usedelta(int usedelta);

int alpm_option_get_checkspace(void);
void alpm_option_set_checkspace(int checkspace);

/** @} */

/** Install reasons
 * Why the package was installed.
 */
typedef enum _pmpkgreason_t {
	/** Explicitly requested by the user. */
	PM_PKG_REASON_EXPLICIT = 0,
	/** Installed as a dependency for another package. */
	PM_PKG_REASON_DEPEND = 1
} pmpkgreason_t;

/** @addtogroup alpm_api_databases Database Functions
 * Functions to query and manipulate the database of libalpm.
 * @{
 */

/** Get the database of locally installed packages.
 * The returned pointer points to an internal structure
 * of libalpm which should only be manipulated through
 * libalpm functions.
 * @return a reference to the local database
 */
pmdb_t *alpm_option_get_localdb(void);

/** Get the list of sync databases.
 * Returns a list of pmdb_t structures, one for each registered
 * sync database.
 * @return a reference to an internal list of pmdb_t structures
 */
alpm_list_t *alpm_option_get_syncdbs(void);

/** Register a sync database of packages.
 * @param treename the name of the sync repository
 * @return a pmdb_t* on success (the value), NULL on error
 */
pmdb_t *alpm_db_register_sync(const char *treename);

/** Unregister a package database.
 * @param db pointer to the package database to unregister
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_db_unregister(pmdb_t *db);

/** Unregister all package databases.
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_db_unregister_all(void);

/** Get the name of a package database.
 * @param db pointer to the package database
 * @return the name of the package database, NULL on error
 */
const char *alpm_db_get_name(const pmdb_t *db);

/** Get a download URL for the package database.
 * @param db pointer to the package database
 * @return a fully-specified download URL, NULL on error
 */
const char *alpm_db_get_url(const pmdb_t *db);

/** Set the serverlist of a database.
 * @param db database pointer
 * @param url url of the server
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_db_setserver(pmdb_t *db, const char *url);

int alpm_db_update(int level, pmdb_t *db);

/** Get a package entry from a package database.
 * @param db pointer to the package database to get the package from
 * @param name of the package
 * @return the package entry on success, NULL on error
 */
pmpkg_t *alpm_db_get_pkg(pmdb_t *db, const char *name);

/** Get the package cache of a package database.
 * @param db pointer to the package database to get the package from
 * @return the list of packages on success, NULL on error
 */
alpm_list_t *alpm_db_get_pkgcache(pmdb_t *db);

/** Get a group entry from a package database.
 * @param db pointer to the package database to get the group from
 * @param name of the group
 * @return the groups entry on success, NULL on error
 */
pmgrp_t *alpm_db_readgrp(pmdb_t *db, const char *name);

/** Get the group cache of a package database.
 * @param db pointer to the package database to get the group from
 * @return the list of groups on success, NULL on error
 */
alpm_list_t *alpm_db_get_grpcache(pmdb_t *db);

/** Searches a database.
 * @param db pointer to the package database to search in
 * @param needles the list of strings to search for
 * @return the list of packages on success, NULL on error
 */
alpm_list_t *alpm_db_search(pmdb_t *db, const alpm_list_t* needles);

/** Set install reason for a package in db.
 * @param db pointer to the package database
 * @param name the name of the package
 * @param reason the new install reason
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_db_set_pkgreason(pmdb_t *db, const char *name, pmpkgreason_t reason);

/** @} */

/** @addtogroup alpm_api_packages Package Functions
 * Functions to manipulate libalpm packages
 * @{
 */

/** Create a package from a file.
 * If full is false, the archive is read only until all necessary
 * metadata is found. If it is true, the entire archive is read, which
 * serves as a verfication of integrity and the filelist can be created.
 * @param filename location of the package tarball
 * @param full whether to stop the load after metadata is read or continue
 *             through the full archive
 * @param pkg address of the package pointer
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_pkg_load(const char *filename, int full, pmpkg_t **pkg);

/** Free a package.
 * @param pkg package pointer to free
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_pkg_free(pmpkg_t *pkg);

/** Check the integrity (with md5) of a package from the sync cache.
 * @param pkg package pointer
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_pkg_checkmd5sum(pmpkg_t *pkg);

/** Compare two version strings and determine which one is 'newer'. */
int alpm_pkg_vercmp(const char *a, const char *b);

/** Computes the list of packages requiring a given package.
 * The return value of this function is a newly allocated
 * list of package names (char*), it should be freed by the caller.
 * @param pkg a package
 * @return the list of packages requiring pkg
 */
alpm_list_t *alpm_pkg_compute_requiredby(pmpkg_t *pkg);

/** @name Package Property Accessors
 * Any pointer returned by these functions points to internal structures
 * allocated by libalpm. They should not be freed nor modified in any
 * way.
 * @{
 */

/** Gets the name of the file from which the package was loaded.
 * @param pkg a pointer to package
 * @return a reference to an internal string
 */
const char *alpm_pkg_get_filename(pmpkg_t *pkg);

/** Returns the package name.
 * @param pkg a pointer to package
 * @return a reference to an internal string
 */
const char *alpm_pkg_get_name(pmpkg_t *pkg);

/** Returns the package version as a string.
 * This includes all available epoch, version, and pkgrel components. Use
 * alpm_pkg_vercmp() to compare version strings if necessary.
 * @param pkg a pointer to package
 * @return a reference to an internal string
 */
const char *alpm_pkg_get_version(pmpkg_t *pkg);

/** Returns the package description.
 * @param pkg a pointer to package
 * @return a reference to an internal string
 */
const char *alpm_pkg_get_desc(pmpkg_t *pkg);

/** Returns the package URL.
 * @param pkg a pointer to package
 * @return a reference to an internal string
 */
const char *alpm_pkg_get_url(pmpkg_t *pkg);

/** Returns the build timestamp of the package.
 * @param pkg a pointer to package
 * @return the timestamp of the build time
 */
time_t alpm_pkg_get_builddate(pmpkg_t *pkg);

/** Returns the install timestamp of the package.
 * @param pkg a pointer to package
 * @return the timestamp of the install time
 */
time_t alpm_pkg_get_installdate(pmpkg_t *pkg);

/** Returns the packager's name.
 * @param pkg a pointer to package
 * @return a reference to an internal string
 */
const char *alpm_pkg_get_packager(pmpkg_t *pkg);

/** Returns the package's MD5 checksum as a string.
 * The returned string is a sequence of lowercase hexadecimal digits.
 * @param pkg a pointer to package
 * @return a reference to an internal string
 */
const char *alpm_pkg_get_md5sum(pmpkg_t *pkg);

/** Returns the architecture for which the package was built.
 * @param pkg a pointer to package
 * @return a reference to an internal string
 */
const char *alpm_pkg_get_arch(pmpkg_t *pkg);

/** Returns the size of the package.
 * @param pkg a pointer to package
 * @return the size of the package in bytes.
 */
off_t alpm_pkg_get_size(pmpkg_t *pkg);

/** Returns the installed size of the package.
 * @param pkg a pointer to package
 * @return the total size of files installed by the package.
 */
off_t alpm_pkg_get_isize(pmpkg_t *pkg);

/** Returns the package installation reason.
 * @param pkg a pointer to package
 * @return an enum member giving the install reason.
 */
pmpkgreason_t alpm_pkg_get_reason(pmpkg_t *pkg);

/** Returns the list of package licenses.
 * @param pkg a pointer to package
 * @return a pointer to an internal list of strings.
 */
alpm_list_t *alpm_pkg_get_licenses(pmpkg_t *pkg);

/** Returns the list of package groups.
 * @param pkg a pointer to package
 * @return a pointer to an internal list of strings.
 */
alpm_list_t *alpm_pkg_get_groups(pmpkg_t *pkg);

/** Returns the list of package dependencies as pmdepend_t.
 * @param pkg a pointer to package
 * @return a reference to an internal list of pmdepend_t structures.
 */
alpm_list_t *alpm_pkg_get_depends(pmpkg_t *pkg);

/** Returns the list of package optional dependencies.
 * @param pkg a pointer to package
 * @return a reference to an internal list of strings.
 */
alpm_list_t *alpm_pkg_get_optdepends(pmpkg_t *pkg);

/** Returns the list of package names conflicting with pkg.
 * @param pkg a pointer to package
 * @return a reference to an internal list of strings.
 */
alpm_list_t *alpm_pkg_get_conflicts(pmpkg_t *pkg);

/** Returns the list of package names provided by pkg.
 * @param pkg a pointer to package
 * @return a reference to an internal list of strings.
 */
alpm_list_t *alpm_pkg_get_provides(pmpkg_t *pkg);

/** Returns the list of available deltas for pkg.
 * @param pkg a pointer to package
 * @return a reference to an internal list of strings.
 */
alpm_list_t *alpm_pkg_get_deltas(pmpkg_t *pkg);

/** Returns the list of packages to be replaced by pkg.
 * @param pkg a pointer to package
 * @return a reference to an internal list of strings.
 */
alpm_list_t *alpm_pkg_get_replaces(pmpkg_t *pkg);

/** Returns the list of files installed by pkg.
 * The filenames are relative to the install root,
 * and do not include leading slashes.
 * @param pkg a pointer to package
 * @return a reference to an internal list of strings.
 */
alpm_list_t *alpm_pkg_get_files(pmpkg_t *pkg);

/** Returns the list of files backed up when installing pkg.
 * The elements of the returned list have the form
 * "<filename>\t<md5sum>", where the given md5sum is that of
 * the file as provided by the package.
 * @param pkg a pointer to package
 * @return a reference to an internal list of strings.
 */
alpm_list_t *alpm_pkg_get_backup(pmpkg_t *pkg);

/** Returns the database containing pkg
 * Returns a pointer to the pmdb_t structure the package is
 * originating from, or NULL is the package was loaded from a file.
 * @param pkg a pointer to package
 * @return a pointer to the DB containing pkg, or NULL.
 */
pmdb_t *alpm_pkg_get_db(pmpkg_t *pkg);

/* End of pmpkg_t accessors */
/* @} */

/** Open a package changelog for reading.
 * Similar to fopen in functionality, except that the returned 'file
 * stream' could really be from an archive as well as from the database.
 * @param pkg the package to read the changelog of (either file or db)
 * @return a 'file stream' to the package changelog
 */
void *alpm_pkg_changelog_open(pmpkg_t *pkg);

/** Read data from an open changelog 'file stream'.
 * Similar to fread in functionality, this function takes a buffer and
 * amount of data to read. If an error occurs pm_errno will be set.
 * @param ptr a buffer to fill with raw changelog data
 * @param size the size of the buffer
 * @param pkg the package that the changelog is being read from
 * @param fp a 'file stream' to the package changelog
 * @return the number of characters read, or 0 if there is no more data or an
 * error occurred.
 */
size_t alpm_pkg_changelog_read(void *ptr, size_t size,
		const pmpkg_t *pkg, const void *fp);

/*int alpm_pkg_changelog_feof(const pmpkg_t *pkg, void *fp);*/

int alpm_pkg_changelog_close(const pmpkg_t *pkg, void *fp);

int alpm_pkg_has_scriptlet(pmpkg_t *pkg);

/** Returns the size of download.
 * Returns the size of the files that will be downloaded to install a
 * package.
 * @param newpkg the new package to upgrade to
 * @return the size of the download
 */
off_t alpm_pkg_download_size(pmpkg_t *newpkg);

alpm_list_t *alpm_pkg_unused_deltas(pmpkg_t *pkg);

/* End of alpm_pkg */
/** @} */

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
alpm_list_t *alpm_find_grp_pkgs(alpm_list_t *dbs, const char *name);

/*
 * Sync
 */

pmpkg_t *alpm_sync_newversion(pmpkg_t *pkg, alpm_list_t *dbs_sync);

/** @addtogroup alpm_api_trans Transaction Functions
 * Functions to manipulate libalpm transactions
 * @{
 */

/** Transaction flags */
typedef enum _pmtransflag_t {
	/** Ignore dependency checks. */
	PM_TRANS_FLAG_NODEPS = 1,
	/** Ignore file conflicts and overwrite files. */
	PM_TRANS_FLAG_FORCE = (1 << 1),
	/** Delete files even if they are tagged as backup. */
	PM_TRANS_FLAG_NOSAVE = (1 << 2),
	/** Ignore version numbers when checking dependencies. */
	PM_TRANS_FLAG_NODEPVERSION = (1 << 3),
	/** Remove also any packages depending on a package being removed. */
	PM_TRANS_FLAG_CASCADE = (1 << 4),
	/** Remove packages and their unneeded deps (not explicitly installed). */
	PM_TRANS_FLAG_RECURSE = (1 << 5),
	/** Modify database but do not commit changes to the filesystem. */
	PM_TRANS_FLAG_DBONLY = (1 << 6),
	/* (1 << 7) flag can go here */
	/** Use PM_PKG_REASON_DEPEND when installing packages. */
	PM_TRANS_FLAG_ALLDEPS = (1 << 8),
	/** Only download packages and do not actually install. */
	PM_TRANS_FLAG_DOWNLOADONLY = (1 << 9),
	/** Do not execute install scriptlets after installing. */
	PM_TRANS_FLAG_NOSCRIPTLET = (1 << 10),
	/** Ignore dependency conflicts. */
	PM_TRANS_FLAG_NOCONFLICTS = (1 << 11),
	/* (1 << 12) flag can go here */
	/** Do not install a package if it is already installed and up to date. */
	PM_TRANS_FLAG_NEEDED = (1 << 13),
	/** Use PM_PKG_REASON_EXPLICIT when installing packages. */
	PM_TRANS_FLAG_ALLEXPLICIT = (1 << 14),
	/** Do not remove a package if it is needed by another one. */
	PM_TRANS_FLAG_UNNEEDED = (1 << 15),
	/** Remove also explicitly installed unneeded deps (use with PM_TRANS_FLAG_RECURSE). */
	PM_TRANS_FLAG_RECURSEALL = (1 << 16),
	/** Do not lock the database during the operation. */
	PM_TRANS_FLAG_NOLOCK = (1 << 17)
} pmtransflag_t;

/** Transaction events.
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
	/** Disk space usage will be computed for a package */
	PM_TRANS_EVT_DISKSPACE_START,
	/** Disk space usage was computed for a package */
	PM_TRANS_EVT_DISKSPACE_DONE,
} pmtransevt_t;

/** Transaction Conversations (ie, questions) */
typedef enum _pmtransconv_t {
	PM_TRANS_CONV_INSTALL_IGNOREPKG = 1,
	PM_TRANS_CONV_REPLACE_PKG = (1 << 1),
	PM_TRANS_CONV_CONFLICT_PKG = (1 << 2),
	PM_TRANS_CONV_CORRUPTED_PKG = (1 << 3),
	PM_TRANS_CONV_LOCAL_NEWER = (1 << 4),
	PM_TRANS_CONV_REMOVE_PKGS = (1 << 5),
	PM_TRANS_CONV_SELECT_PROVIDER = (1 << 6),
} pmtransconv_t;

/** Transaction Progress */
typedef enum _pmtransprog_t {
	PM_TRANS_PROGRESS_ADD_START,
	PM_TRANS_PROGRESS_UPGRADE_START,
	PM_TRANS_PROGRESS_REMOVE_START,
	PM_TRANS_PROGRESS_CONFLICTS_START,
	PM_TRANS_PROGRESS_DISKSPACE_START,
	PM_TRANS_PROGRESS_INTEGRITY_START,
} pmtransprog_t;

/** Transaction Event callback */
typedef void (*alpm_trans_cb_event)(pmtransevt_t, void *, void *);

/** Transaction Conversation callback */
typedef void (*alpm_trans_cb_conv)(pmtransconv_t, void *, void *,
                                   void *, int *);

/** Transaction Progress callback */
typedef void (*alpm_trans_cb_progress)(pmtransprog_t, const char *, int, size_t, size_t);

int alpm_trans_get_flags(void);

/** Returns a list of packages added by the transaction.
 * @return a list of pmpkg_t structures
 */
alpm_list_t * alpm_trans_get_add(void);

/** Returns the list of packages removed by the transaction.
 * @return a list of pmpkg_t structures
 */
alpm_list_t * alpm_trans_get_remove(void);

/** Initialize the transaction.
 * @param flags flags of the transaction (like nodeps, etc)
 * @param event event callback function pointer
 * @param conv question callback function pointer
 * @param progress progress callback function pointer
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_trans_init(pmtransflag_t flags,
                    alpm_trans_cb_event cb_event, alpm_trans_cb_conv conv,
                    alpm_trans_cb_progress cb_progress);

/** Prepare a transaction.
 * @param data the address of an alpm_list where a list
 * of pmdepmissing_t objects is dumped (conflicting packages)
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_trans_prepare(alpm_list_t **data);

/** Commit a transaction.
 * @param data the address of an alpm_list where detailed description
 * of an error can be dumped (ie. list of conflicting files)
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_trans_commit(alpm_list_t **data);

/** Interrupt a transaction.
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_trans_interrupt(void);

/** Release a transaction.
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_trans_release(void);
/** @} */

/** @name Common Transactions */
/** @{ */
int alpm_sync_sysupgrade(int enable_downgrade);
int alpm_add_pkg(pmpkg_t *pkg);
int alpm_remove_pkg(pmpkg_t *pkg);
/** @} */

/** @addtogroup alpm_api_depends Dependency Functions
 * Functions dealing with libalpm representation of dependency
 * information.
 * @{
 */

/** Types of version constraints in dependency specs. */
typedef enum _pmdepmod_t {
  /** No version constraint */
	PM_DEP_MOD_ANY = 1,
  /** Test version equality (package=x.y.z) */
	PM_DEP_MOD_EQ,
  /** Test for at least a version (package>=x.y.z) */
	PM_DEP_MOD_GE,
  /** Test for at most a version (package<=x.y.z) */
	PM_DEP_MOD_LE,
  /** Test for greater than some version (package>x.y.z) */
	PM_DEP_MOD_GT,
  /** Test for less than some version (package<x.y.z) */
	PM_DEP_MOD_LT
} pmdepmod_t;

alpm_list_t *alpm_checkdeps(alpm_list_t *pkglist, int reversedeps,
		alpm_list_t *remove, alpm_list_t *upgrade);
pmpkg_t *alpm_find_satisfier(alpm_list_t *pkgs, const char *depstring);
pmpkg_t *alpm_find_dbs_satisfier(alpm_list_t *dbs, const char *depstring);

const char *alpm_miss_get_target(const pmdepmissing_t *miss);
pmdepend_t *alpm_miss_get_dep(pmdepmissing_t *miss);
const char *alpm_miss_get_causingpkg(const pmdepmissing_t *miss);

alpm_list_t *alpm_checkconflicts(alpm_list_t *pkglist);

const char *alpm_conflict_get_package1(pmconflict_t *conflict);
const char *alpm_conflict_get_package2(pmconflict_t *conflict);
const char *alpm_conflict_get_reason(pmconflict_t *conflict);

/** Returns the type of version constraint.
 * @param dep a dependency info structure
 * @return the type of version constraint (PM_DEP_MOD_ANY if no version
 * is specified).
 */
pmdepmod_t alpm_dep_get_mod(const pmdepend_t *dep);

/** Returns the package name of a dependency constraint.
 * @param dep a dependency info structure
 * @return a pointer to an internal string.
 */
const char *alpm_dep_get_name(const pmdepend_t *dep);

/** Returns the version specified by a dependency constraint.
 * The version information is returned as a string in the same format
 * as given by alpm_pkg_get_version().
 * @param dep a dependency info structure
 * @return a pointer to an internal string.
 */
const char *alpm_dep_get_version(const pmdepend_t *dep);

/** Returns a newly allocated string representing the dependency information.
 * @param dep a dependency info structure
 * @return a formatted string, e.g. "glibc>=2.12"
 */
char *alpm_dep_compute_string(const pmdepend_t *dep);

/** @} */

/** @addtogroup alpm_api_fileconflicts File Conflicts Functions
 * Functions to manipulate file conflict information.
 * @{
 */

typedef enum _pmfileconflicttype_t {
	PM_FILECONFLICT_TARGET = 1,
	PM_FILECONFLICT_FILESYSTEM
} pmfileconflicttype_t;

const char *alpm_fileconflict_get_target(pmfileconflict_t *conflict);
pmfileconflicttype_t alpm_fileconflict_get_type(pmfileconflict_t *conflict);
const char *alpm_fileconflict_get_file(pmfileconflict_t *conflict);
const char *alpm_fileconflict_get_ctarget(pmfileconflict_t *conflict);

/** @} */

/*
 * Helpers
 */

/* checksums */
char *alpm_compute_md5sum(const char *name);

/** @addtogroup alpm_api_errors Error Codes
 * @{
 */
enum _pmerrno_t {
	PM_ERR_MEMORY = 1,
	PM_ERR_SYSTEM,
	PM_ERR_BADPERMS,
	PM_ERR_NOT_A_FILE,
	PM_ERR_NOT_A_DIR,
	PM_ERR_WRONG_ARGS,
	PM_ERR_DISK_SPACE,
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
	PM_ERR_DB_VERSION,
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
	PM_ERR_WRITE,
	PM_ERR_INVALID_REGEX,
	/* External library errors */
	PM_ERR_LIBARCHIVE,
	PM_ERR_LIBFETCH,
	PM_ERR_EXTERNAL_DOWNLOAD
};

/** The number of the last error that occurred. */
extern enum _pmerrno_t pm_errno;

/** Returns the string corresponding to an error number. */
const char *alpm_strerror(int err);

/** Returns the string corresponding to pm_errno. */
const char *alpm_strerrorlast(void);

/* End of alpm_api_errors */
/** @} */

/* End of alpm_api */
/** @} */

#ifdef __cplusplus
}
#endif
#endif /* _ALPM_H */

/* vim: set ts=2 sw=2 noet: */
