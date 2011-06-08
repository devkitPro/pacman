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
 * Enumerations
 * These ones are used in multiple contexts, so are forward-declared.
 */

/**
 * Install reasons.
 * Why the package was installed.
 */
typedef enum _pmpkgreason_t {
	/** Explicitly requested by the user. */
	PM_PKG_REASON_EXPLICIT = 0,
	/** Installed as a dependency for another package. */
	PM_PKG_REASON_DEPEND = 1
} pmpkgreason_t;

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

/**
 * File conflict type.
 * Whether the conflict results from a file existing on the filesystem, or with
 * another target in the transaction.
 */
typedef enum _pmfileconflicttype_t {
	PM_FILECONFLICT_TARGET = 1,
	PM_FILECONFLICT_FILESYSTEM
} pmfileconflicttype_t;

/**
 * GPG signature verification options
 */
typedef enum _pgp_verify_t {
	PM_PGP_VERIFY_UNKNOWN,
	PM_PGP_VERIFY_NEVER,
	PM_PGP_VERIFY_OPTIONAL,
	PM_PGP_VERIFY_ALWAYS
} pgp_verify_t;

/*
 * Structures
 */

typedef struct __pmhandle_t pmhandle_t;
typedef struct __pmdb_t pmdb_t;
typedef struct __pmpkg_t pmpkg_t;
typedef struct __pmtrans_t pmtrans_t;

/** Dependency */
typedef struct _pmdepend_t {
	char *name;
	char *version;
	unsigned long name_hash;
	pmdepmod_t mod;
} pmdepend_t;

/** Missing dependency */
typedef struct _pmdepmissing_t {
	char *target;
	pmdepend_t *depend;
	/* this is used in case of remove dependency error only */
	char *causingpkg;
} pmdepmissing_t;

/** Conflict */
typedef struct _pmconflict_t {
	char *package1;
	char *package2;
	char *reason;
} pmconflict_t;

/** File conflict */
typedef struct _pmfileconflict_t {
	char *target;
	pmfileconflicttype_t type;
	char *file;
	char *ctarget;
} pmfileconflict_t;

/** Package group */
typedef struct _pmgrp_t {
	/** group name */
	char *name;
	/** list of pmpkg_t packages */
	alpm_list_t *packages;
} pmgrp_t;

/** Package upgrade delta */
typedef struct _pmdelta_t {
	/** filename of the delta patch */
	char *delta;
	/** md5sum of the delta file */
	char *delta_md5;
	/** filename of the 'before' file */
	char *from;
	/** filename of the 'after' file */
	char *to;
	/** filesize of the delta file */
	off_t delta_size;
	/** download filesize of the delta file */
	off_t download_size;
} pmdelta_t;

/** Local package or package file backup entry */
typedef struct _pmbackup_t {
	char *name;
	char *hash;
} pmbackup_t;

/*
 * Logging facilities
 */

/**
 * Logging Levels
 */
typedef enum _pmloglevel_t {
	PM_LOG_ERROR    = 1,
	PM_LOG_WARNING  = (1 << 1),
	PM_LOG_DEBUG    = (1 << 2),
	PM_LOG_FUNCTION = (1 << 3)
} pmloglevel_t;

typedef void (*alpm_cb_log)(pmloglevel_t, const char *, va_list);
int alpm_logaction(pmhandle_t *handle, const char *fmt, ...);

/*
 * Downloading
 */

/** Type of download progress callbacks.
 * @param filename the name of the file being downloaded
 * @param xfered the number of transferred bytes
 * @param total the total number of bytes to transfer
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
 * @param handle the context handle
 * @param url URL of the package to download
 * @return the downloaded filepath on success, NULL on error
 */
char *alpm_fetch_pkgurl(pmhandle_t *handle, const char *url);

/** @addtogroup alpm_api_options Options
 * Libalpm option getters and setters
 * @{
 */

/** Returns the callback used for logging. */
alpm_cb_log alpm_option_get_logcb(pmhandle_t *handle);
/** Sets the callback used for logging. */
int alpm_option_set_logcb(pmhandle_t *handle, alpm_cb_log cb);

/** Returns the callback used to report download progress. */
alpm_cb_download alpm_option_get_dlcb(pmhandle_t *handle);
/** Sets the callback used to report download progress. */
int alpm_option_set_dlcb(pmhandle_t *handle, alpm_cb_download cb);

/** Returns the downloading callback. */
alpm_cb_fetch alpm_option_get_fetchcb(pmhandle_t *handle);
/** Sets the downloading callback. */
int alpm_option_set_fetchcb(pmhandle_t *handle, alpm_cb_fetch cb);

/** Returns the callback used to report total download size. */
alpm_cb_totaldl alpm_option_get_totaldlcb(pmhandle_t *handle);
/** Sets the callback used to report total download size. */
int alpm_option_set_totaldlcb(pmhandle_t *handle, alpm_cb_totaldl cb);

/** Returns the root of the destination filesystem. Read-only. */
const char *alpm_option_get_root(pmhandle_t *handle);

/** Returns the path to the database directory. Read-only. */
const char *alpm_option_get_dbpath(pmhandle_t *handle);

/** Get the name of the database lock file. Read-only. */
const char *alpm_option_get_lockfile(pmhandle_t *handle);

/** @name Accessors to the list of package cache directories.
 * @{
 */
alpm_list_t *alpm_option_get_cachedirs(pmhandle_t *handle);
int alpm_option_set_cachedirs(pmhandle_t *handle, alpm_list_t *cachedirs);
int alpm_option_add_cachedir(pmhandle_t *handle, const char *cachedir);
int alpm_option_remove_cachedir(pmhandle_t *handle, const char *cachedir);
/** @} */

/** Returns the logfile name. */
const char *alpm_option_get_logfile(pmhandle_t *handle);
/** Sets the logfile name. */
int alpm_option_set_logfile(pmhandle_t *handle, const char *logfile);

/** Returns the path to libalpm's GnuPG home directory. */
const char *alpm_option_get_gpgdir(pmhandle_t *handle);
/** Sets the path to libalpm's GnuPG home directory. */
int alpm_option_set_gpgdir(pmhandle_t *handle, const char *gpgdir);

/** Returns whether to use syslog (0 is FALSE, TRUE otherwise). */
int alpm_option_get_usesyslog(pmhandle_t *handle);
/** Sets whether to use syslog (0 is FALSE, TRUE otherwise). */
int alpm_option_set_usesyslog(pmhandle_t *handle, int usesyslog);

/** @name Accessors to the list of no-upgrade files.
 * These functions modify the list of files which should
 * not be updated by package installation.
 * @{
 */
alpm_list_t *alpm_option_get_noupgrades(pmhandle_t *handle);
int alpm_option_add_noupgrade(pmhandle_t *handle, const char *pkg);
int alpm_option_set_noupgrades(pmhandle_t *handle, alpm_list_t *noupgrade);
int alpm_option_remove_noupgrade(pmhandle_t *handle, const char *pkg);
/** @} */

/** @name Accessors to the list of no-extract files.
 * These functions modify the list of filenames which should
 * be skipped packages which should
 * not be upgraded by a sysupgrade operation.
 * @{
 */
alpm_list_t *alpm_option_get_noextracts(pmhandle_t *handle);
int alpm_option_add_noextract(pmhandle_t *handle, const char *pkg);
int alpm_option_set_noextracts(pmhandle_t *handle, alpm_list_t *noextract);
int alpm_option_remove_noextract(pmhandle_t *handle, const char *pkg);
/** @} */

/** @name Accessors to the list of ignored packages.
 * These functions modify the list of packages that
 * should be ignored by a sysupgrade.
 * @{
 */
alpm_list_t *alpm_option_get_ignorepkgs(pmhandle_t *handle);
int alpm_option_add_ignorepkg(pmhandle_t *handle, const char *pkg);
int alpm_option_set_ignorepkgs(pmhandle_t *handle, alpm_list_t *ignorepkgs);
int alpm_option_remove_ignorepkg(pmhandle_t *handle, const char *pkg);
/** @} */

/** @name Accessors to the list of ignored groups.
 * These functions modify the list of groups whose packages
 * should be ignored by a sysupgrade.
 * @{
 */
alpm_list_t *alpm_option_get_ignoregrps(pmhandle_t *handle);
int alpm_option_add_ignoregrp(pmhandle_t *handle, const char *grp);
int alpm_option_set_ignoregrps(pmhandle_t *handle, alpm_list_t *ignoregrps);
int alpm_option_remove_ignoregrp(pmhandle_t *handle, const char *grp);
/** @} */

/** Returns the targeted architecture. */
const char *alpm_option_get_arch(pmhandle_t *handle);
/** Sets the targeted architecture. */
int alpm_option_set_arch(pmhandle_t *handle, const char *arch);

int alpm_option_get_usedelta(pmhandle_t *handle);
int alpm_option_set_usedelta(pmhandle_t *handle, int usedelta);

int alpm_option_get_checkspace(pmhandle_t *handle);
int alpm_option_set_checkspace(pmhandle_t *handle, int checkspace);

pgp_verify_t alpm_option_get_default_sigverify(pmhandle_t *handle);
int alpm_option_set_default_sigverify(pmhandle_t *handle, pgp_verify_t level);

/** @} */

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
pmdb_t *alpm_option_get_localdb(pmhandle_t *handle);

/** Get the list of sync databases.
 * Returns a list of pmdb_t structures, one for each registered
 * sync database.
 * @param handle the context handle
 * @return a reference to an internal list of pmdb_t structures
 */
alpm_list_t *alpm_option_get_syncdbs(pmhandle_t *handle);

/** Register a sync database of packages.
 * @param handle the context handle
 * @param treename the name of the sync repository
 * @param check_sig what level of signature checking to perform on the
 * database; note that this must be a '.sig' file type verification
 * @return a pmdb_t* on success (the value), NULL on error
 */
pmdb_t *alpm_db_register_sync(pmhandle_t *handle, const char *treename,
		pgp_verify_t check_sig);

/** Unregister a package database.
 * @param db pointer to the package database to unregister
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_db_unregister(pmdb_t *db);

/** Unregister all package databases.
 * @param handle the context handle
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_db_unregister_all(pmhandle_t *handle);

/** Get the name of a package database.
 * @param db pointer to the package database
 * @return the name of the package database, NULL on error
 */
const char *alpm_db_get_name(const pmdb_t *db);

/** @name Accessors to the list of servers for a database.
 * @{
 */
alpm_list_t *alpm_db_get_servers(const pmdb_t *db);
int alpm_db_set_servers(pmdb_t *db, alpm_list_t *servers);
int alpm_db_add_server(pmdb_t *db, const char *url);
int alpm_db_remove_server(pmdb_t *db, const char *url);
/** @} */

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

/** Searches a database with regular expressions.
 * @param db pointer to the package database to search in
 * @param needles a list of regular expressions to search for
 * @return the list of packages matching all regular expressions on success, NULL on error
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
 * serves as a verification of integrity and the filelist can be created.
 * The allocated structure should be freed using alpm_pkg_free().
 * @param handle the context handle
 * @param filename location of the package tarball
 * @param full whether to stop the load after metadata is read or continue
 * through the full archive
 * @param check_sig what level of package signature checking to perform on the
 * package; note that this must be a '.sig' file type verification
 * @param pkg address of the package pointer
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_pkg_load(pmhandle_t *handle, const char *filename, int full,
		pgp_verify_t check_sig, pmpkg_t **pkg);

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

/** Returns the database containing pkg.
 * Returns a pointer to the pmdb_t structure the package is
 * originating from, or NULL if the package was loaded from a file.
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

/** Returns whether the package has an install scriptlet.
 * @return 0 if FALSE, TRUE otherwise
 */
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
 * Signatures
 */

int alpm_pkg_check_pgp_signature(pmpkg_t *pkg);

int alpm_db_check_pgp_signature(pmdb_t *db);
int alpm_db_set_pgp_verify(pmdb_t *db, pgp_verify_t verify);

/*
 * Groups
 */

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

/** Returns the bitfield of flags for the current transaction.
 * @param handle the context handle
 * @return the bitfield of transaction flags
 */
pmtransflag_t alpm_trans_get_flags(pmhandle_t *handle);

/** Returns a list of packages added by the transaction.
 * @param handle the context handle
 * @return a list of pmpkg_t structures
 */
alpm_list_t * alpm_trans_get_add(pmhandle_t *handle);

/** Returns the list of packages removed by the transaction.
 * @param handle the context handle
 * @return a list of pmpkg_t structures
 */
alpm_list_t * alpm_trans_get_remove(pmhandle_t *handle);

/** Initialize the transaction.
 * @param handle the context handle
 * @param flags flags of the transaction (like nodeps, etc)
 * @param event event callback function pointer
 * @param conv question callback function pointer
 * @param progress progress callback function pointer
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_trans_init(pmhandle_t *handle, pmtransflag_t flags,
                    alpm_trans_cb_event cb_event, alpm_trans_cb_conv conv,
                    alpm_trans_cb_progress cb_progress);

/** Prepare a transaction.
 * @param handle the context handle
 * @param data the address of an alpm_list where a list
 * of pmdepmissing_t objects is dumped (conflicting packages)
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_trans_prepare(pmhandle_t *handle, alpm_list_t **data);

/** Commit a transaction.
 * @param handle the context handle
 * @param data the address of an alpm_list where detailed description
 * of an error can be dumped (ie. list of conflicting files)
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_trans_commit(pmhandle_t *handle, alpm_list_t **data);

/** Interrupt a transaction.
 * @param handle the context handle
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_trans_interrupt(pmhandle_t *handle);

/** Release a transaction.
 * @param handle the context handle
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_trans_release(pmhandle_t *handle);
/** @} */

/** @name Common Transactions */
/** @{ */

/** Search for packages to upgrade and add them to the transaction.
 * @param handle the context handle
 * @param enable_downgrade allow downgrading of packages if the remote version is lower
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_sync_sysupgrade(pmhandle_t *handle, int enable_downgrade);

/** Add a package to the transaction.
 * If the package was loaded by alpm_pkg_load(), it will be freed upon
 * alpm_trans_release() invocation.
 * @param handle the context handle
 * @param pkg the package to add
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_add_pkg(pmhandle_t *handle, pmpkg_t *pkg);

/** Add a package removal action to the transaction.
 * @param handle the context handle
 * @param pkg the package to uninstall
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int alpm_remove_pkg(pmhandle_t *handle, pmpkg_t *pkg);

/** @} */

/** @addtogroup alpm_api_depends Dependency Functions
 * Functions dealing with libalpm representation of dependency
 * information.
 * @{
 */

alpm_list_t *alpm_checkdeps(pmhandle_t *handle, alpm_list_t *pkglist,
		alpm_list_t *remove, alpm_list_t *upgrade, int reversedeps);
pmpkg_t *alpm_find_satisfier(alpm_list_t *pkgs, const char *depstring);
pmpkg_t *alpm_find_dbs_satisfier(pmhandle_t *handle,
		alpm_list_t *dbs, const char *depstring);

alpm_list_t *alpm_checkconflicts(pmhandle_t *handle, alpm_list_t *pkglist);

/** Returns a newly allocated string representing the dependency information.
 * @param dep a dependency info structure
 * @return a formatted string, e.g. "glibc>=2.12"
 */
char *alpm_dep_compute_string(const pmdepend_t *dep);

/** @} */

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
	PM_ERR_DB_INVALID,
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
	/* Signatures */
	PM_ERR_SIG_MISSINGDIR,
	PM_ERR_SIG_INVALID,
	PM_ERR_SIG_UNKNOWN,
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
	PM_ERR_LIBCURL,
	PM_ERR_EXTERNAL_DOWNLOAD,
	PM_ERR_GPGME
};

/** Returns the current error code from the handle. */
enum _pmerrno_t alpm_errno(pmhandle_t *handle);

/** Returns the string corresponding to an error number. */
const char *alpm_strerror(enum _pmerrno_t err);

/* End of alpm_api_errors */
/** @} */

pmhandle_t *alpm_initialize(const char *root, const char *dbpath,
		enum _pmerrno_t *err);
int alpm_release(pmhandle_t *handle);
const char *alpm_version(void);

/* End of alpm_api */
/** @} */

#ifdef __cplusplus
}
#endif
#endif /* _ALPM_H */

/* vim: set ts=2 sw=2 noet: */
