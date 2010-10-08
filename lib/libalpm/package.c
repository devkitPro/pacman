/*
 *  package.c
 *
 *  Copyright (c) 2006-2010 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2005, 2006 by Christian Hamar <krics@linuxforum.hu>
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

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

/* libalpm */
#include "package.h"
#include "alpm_list.h"
#include "log.h"
#include "util.h"
#include "db.h"
#include "delta.h"
#include "handle.h"
#include "deps.h"

/** \addtogroup alpm_packages Package Functions
 * @brief Functions to manipulate libalpm packages
 * @{
 */

/** Free a package.
 * @param pkg package pointer to free
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_pkg_free(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	ASSERT(pkg != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	/* Only free packages loaded in user space */
	if(pkg->origin == PKG_FROM_FILE) {
		_alpm_pkg_free(pkg);
	}

	return(0);
}

/** Check the integrity (with md5) of a package from the sync cache.
 * @param pkg package pointer
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_pkg_checkmd5sum(pmpkg_t *pkg)
{
	char *fpath;
	int retval;

	ALPM_LOG_FUNC;

	ASSERT(pkg != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));
	/* We only inspect packages from sync repositories */
	ASSERT(pkg->origin == PKG_FROM_SYNCDB, RET_ERR(PM_ERR_PKG_INVALID, -1));

	fpath = _alpm_filecache_find(alpm_pkg_get_filename(pkg));

	retval = _alpm_test_md5sum(fpath, alpm_pkg_get_md5sum(pkg));

	if(retval == 0) {
		return(0);
	} else if (retval == 1) {
		pm_errno = PM_ERR_PKG_INVALID;
		retval = -1;
	}

	return(retval);
}

/* Default package accessor functions. These will get overridden by any
 * backend logic that needs lazy access, such as the local database through
 * a lazy-load cache. However, the defaults will work just fine for fully-
 * populated package structures. */
const char *_pkg_get_filename(pmpkg_t *pkg)    { return pkg->filename; }
const char *_pkg_get_name(pmpkg_t *pkg)        { return pkg->name; }
const char *_pkg_get_version(pmpkg_t *pkg)     { return pkg->version; }
const char *_pkg_get_desc(pmpkg_t *pkg)        { return pkg->desc; }
const char *_pkg_get_url(pmpkg_t *pkg)         { return pkg->url; }
time_t _pkg_get_builddate(pmpkg_t *pkg)        { return pkg->builddate; }
time_t _pkg_get_installdate(pmpkg_t *pkg)      { return pkg->installdate; }
const char *_pkg_get_packager(pmpkg_t *pkg)    { return pkg->packager; }
const char *_pkg_get_md5sum(pmpkg_t *pkg)      { return pkg->md5sum; }
const char *_pkg_get_arch(pmpkg_t *pkg)        { return pkg->arch; }
off_t _pkg_get_size(pmpkg_t *pkg)              { return pkg->size; }
off_t _pkg_get_isize(pmpkg_t *pkg)             { return pkg->isize; }
pmpkgreason_t _pkg_get_reason(pmpkg_t *pkg)    { return pkg->reason; }
int _pkg_get_epoch(pmpkg_t *pkg)               { return pkg->epoch; }

alpm_list_t *_pkg_get_licenses(pmpkg_t *pkg)   { return pkg->licenses; }
alpm_list_t *_pkg_get_groups(pmpkg_t *pkg)     { return pkg->groups; }
alpm_list_t *_pkg_get_depends(pmpkg_t *pkg)    { return pkg->depends; }
alpm_list_t *_pkg_get_optdepends(pmpkg_t *pkg) { return pkg->optdepends; }
alpm_list_t *_pkg_get_conflicts(pmpkg_t *pkg)  { return pkg->conflicts; }
alpm_list_t *_pkg_get_provides(pmpkg_t *pkg)   { return pkg->provides; }
alpm_list_t *_pkg_get_replaces(pmpkg_t *pkg)   { return pkg->replaces; }
alpm_list_t *_pkg_get_deltas(pmpkg_t *pkg)     { return pkg->deltas; }
alpm_list_t *_pkg_get_files(pmpkg_t *pkg)      { return pkg->files; }
alpm_list_t *_pkg_get_backup(pmpkg_t *pkg)     { return pkg->backup; }

/** The standard package operations struct. Get fields directly from the
 * struct itself with no abstraction layer or any type of lazy loading.
 */
struct pkg_operations default_pkg_ops = {
	.get_filename    = _pkg_get_filename,
	.get_name        = _pkg_get_name,
	.get_version     = _pkg_get_version,
	.get_desc        = _pkg_get_desc,
	.get_url         = _pkg_get_url,
	.get_builddate   = _pkg_get_builddate,
	.get_installdate = _pkg_get_installdate,
	.get_packager    = _pkg_get_packager,
	.get_md5sum      = _pkg_get_md5sum,
	.get_arch        = _pkg_get_arch,
	.get_size        = _pkg_get_size,
	.get_isize       = _pkg_get_isize,
	.get_reason      = _pkg_get_reason,
	.get_epoch       = _pkg_get_epoch,
	.get_licenses    = _pkg_get_licenses,
	.get_groups      = _pkg_get_groups,
	.get_depends     = _pkg_get_depends,
	.get_optdepends  = _pkg_get_optdepends,
	.get_conflicts   = _pkg_get_conflicts,
	.get_provides    = _pkg_get_provides,
	.get_replaces    = _pkg_get_replaces,
	.get_deltas      = _pkg_get_deltas,
	.get_files       = _pkg_get_files,
	.get_backup      = _pkg_get_backup,
};

/* Public functions for getting package information. These functions
 * delegate the hard work to the function callbacks attached to each
 * package, which depend on where the package was loaded from. */
const char SYMEXPORT *alpm_pkg_get_filename(pmpkg_t *pkg)
{
	return pkg->ops->get_filename(pkg);
}

const char SYMEXPORT *alpm_pkg_get_name(pmpkg_t *pkg)
{
	return pkg->ops->get_name(pkg);
}

const char SYMEXPORT *alpm_pkg_get_version(pmpkg_t *pkg)
{
	return pkg->ops->get_version(pkg);
}

const char SYMEXPORT *alpm_pkg_get_desc(pmpkg_t *pkg)
{
	return pkg->ops->get_desc(pkg);
}

const char SYMEXPORT *alpm_pkg_get_url(pmpkg_t *pkg)
{
	return pkg->ops->get_url(pkg);
}

time_t SYMEXPORT alpm_pkg_get_builddate(pmpkg_t *pkg)
{
	return pkg->ops->get_builddate(pkg);
}

time_t SYMEXPORT alpm_pkg_get_installdate(pmpkg_t *pkg)
{
	return pkg->ops->get_installdate(pkg);
}

const char SYMEXPORT *alpm_pkg_get_packager(pmpkg_t *pkg)
{
	return pkg->ops->get_packager(pkg);
}

const char SYMEXPORT *alpm_pkg_get_md5sum(pmpkg_t *pkg)
{
	return pkg->ops->get_md5sum(pkg);
}

const char SYMEXPORT *alpm_pkg_get_arch(pmpkg_t *pkg)
{
	return pkg->ops->get_arch(pkg);
}

off_t SYMEXPORT alpm_pkg_get_size(pmpkg_t *pkg)
{
	return pkg->ops->get_size(pkg);
}

off_t SYMEXPORT alpm_pkg_get_isize(pmpkg_t *pkg)
{
	return pkg->ops->get_isize(pkg);
}

pmpkgreason_t SYMEXPORT alpm_pkg_get_reason(pmpkg_t *pkg)
{
	return pkg->ops->get_reason(pkg);
}

int SYMEXPORT alpm_pkg_get_epoch(pmpkg_t *pkg)
{
	return pkg->ops->get_epoch(pkg);
}

alpm_list_t SYMEXPORT *alpm_pkg_get_licenses(pmpkg_t *pkg)
{
	return pkg->ops->get_licenses(pkg);
}

alpm_list_t SYMEXPORT *alpm_pkg_get_groups(pmpkg_t *pkg)
{
	return pkg->ops->get_groups(pkg);
}

alpm_list_t SYMEXPORT *alpm_pkg_get_depends(pmpkg_t *pkg)
{
	return pkg->ops->get_depends(pkg);
}

alpm_list_t SYMEXPORT *alpm_pkg_get_optdepends(pmpkg_t *pkg)
{
	return pkg->ops->get_optdepends(pkg);
}

alpm_list_t SYMEXPORT *alpm_pkg_get_conflicts(pmpkg_t *pkg)
{
	return pkg->ops->get_conflicts(pkg);
}

alpm_list_t SYMEXPORT *alpm_pkg_get_provides(pmpkg_t *pkg)
{
	return pkg->ops->get_provides(pkg);
}

alpm_list_t SYMEXPORT *alpm_pkg_get_replaces(pmpkg_t *pkg)
{
	return pkg->ops->get_replaces(pkg);
}

alpm_list_t SYMEXPORT *alpm_pkg_get_deltas(pmpkg_t *pkg)
{
	return pkg->ops->get_deltas(pkg);
}

alpm_list_t SYMEXPORT *alpm_pkg_get_files(pmpkg_t *pkg)
{
	return pkg->ops->get_files(pkg);
}

alpm_list_t SYMEXPORT *alpm_pkg_get_backup(pmpkg_t *pkg)
{
	return pkg->ops->get_backup(pkg);
}

pmdb_t SYMEXPORT *alpm_pkg_get_db(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(pkg != NULL, return(NULL));
	ASSERT(pkg->origin != PKG_FROM_FILE, return(NULL));

	return(pkg->origin_data.db);
}

/**
 * Open a package changelog for reading. Similar to fopen in functionality,
 * except that the returned 'file stream' could really be from an archive
 * as well as from the database.
 * @param pkg the package to read the changelog of (either file or db)
 * @return a 'file stream' to the package changelog
 */
void SYMEXPORT *alpm_pkg_changelog_open(pmpkg_t *pkg)
{
	return pkg->ops->changelog_open(pkg);
}

/**
 * Read data from an open changelog 'file stream'. Similar to fread in
 * functionality, this function takes a buffer and amount of data to read. If an
 * error occurs pm_errno will be set.
 *
 * @param ptr a buffer to fill with raw changelog data
 * @param size the size of the buffer
 * @param pkg the package that the changelog is being read from
 * @param fp a 'file stream' to the package changelog
 * @return the number of characters read, or 0 if there is no more data or an
 * error occurred.
 */
size_t SYMEXPORT alpm_pkg_changelog_read(void *ptr, size_t size,
		const pmpkg_t *pkg, const void *fp)
{
	return pkg->ops->changelog_read(ptr, size, pkg, fp);
}

/*
int SYMEXPORT alpm_pkg_changelog_feof(const pmpkg_t *pkg, void *fp)
{
	return pkg->ops->changelog_feof(pkg, fp);
}
*/

/**
 * Close a package changelog for reading. Similar to fclose in functionality,
 * except that the 'file stream' could really be from an archive as well as
 * from the database.
 * @param pkg the package that the changelog was read from
 * @param fp a 'file stream' to the package changelog
 * @return whether closing the package changelog stream was successful
 */
int SYMEXPORT alpm_pkg_changelog_close(const pmpkg_t *pkg, void *fp)
{
	return pkg->ops->changelog_close(pkg, fp);
}

int SYMEXPORT alpm_pkg_has_scriptlet(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(-1));
	ASSERT(pkg != NULL, return(-1));

	if(pkg->origin == PKG_FROM_LOCALDB
		 && !(pkg->infolevel & INFRQ_SCRIPTLET)) {
		_alpm_local_db_read(pkg->origin_data.db, pkg, INFRQ_SCRIPTLET);
	}
	return pkg->scriptlet;
}

static void find_requiredby(pmpkg_t *pkg, pmdb_t *db, alpm_list_t **reqs)
{
	const alpm_list_t *i;
	for(i = _alpm_db_get_pkgcache(db); i; i = i->next) {
		if(!i->data) {
			continue;
		}
		pmpkg_t *cachepkg = i->data;
		if(_alpm_dep_edge(cachepkg, pkg)) {
			const char *cachepkgname = cachepkg->name;
			if(alpm_list_find_str(*reqs, cachepkgname) == 0) {
				*reqs = alpm_list_add(*reqs, strdup(cachepkgname));
			}
		}
	}
}

/**
 * @brief Compute the packages requiring a given package.
 * @param pkg a package
 * @return the list of packages requiring pkg
 */
alpm_list_t SYMEXPORT *alpm_pkg_compute_requiredby(pmpkg_t *pkg)
{
	const alpm_list_t *i;
	alpm_list_t *reqs = NULL;
	pmdb_t *db;

	if(pkg->origin == PKG_FROM_FILE) {
		/* The sane option; search locally for things that require this. */
		db = alpm_option_get_localdb();
		find_requiredby(pkg, db, &reqs);
	} else {
		/* We have a DB package. if it is a local package, then we should
		 * only search the local DB; else search all known sync databases. */
		db = pkg->origin_data.db;
		if(db->is_local) {
			find_requiredby(pkg, db, &reqs);
		} else {
			for(i = handle->dbs_sync; i; i = i->next) {
				db = i->data;
				find_requiredby(pkg, db, &reqs);
				reqs = alpm_list_msort(reqs, alpm_list_count(reqs), _alpm_str_cmp);
			}
		}
	}
	return(reqs);
}

/** @} */

pmpkg_t *_alpm_pkg_new(void)
{
	pmpkg_t* pkg;

	ALPM_LOG_FUNC;

	CALLOC(pkg, 1, sizeof(pmpkg_t), RET_ERR(PM_ERR_MEMORY, NULL));

	return(pkg);
}

pmpkg_t *_alpm_pkg_dup(pmpkg_t *pkg)
{
	pmpkg_t *newpkg;
	alpm_list_t *i;

	ALPM_LOG_FUNC;

	CALLOC(newpkg, 1, sizeof(pmpkg_t), RET_ERR(PM_ERR_MEMORY, NULL));

	STRDUP(newpkg->filename, pkg->filename, RET_ERR(PM_ERR_MEMORY, newpkg));
	STRDUP(newpkg->name, pkg->name, RET_ERR(PM_ERR_MEMORY, newpkg));
	STRDUP(newpkg->version, pkg->version, RET_ERR(PM_ERR_MEMORY, newpkg));
	STRDUP(newpkg->desc, pkg->desc, RET_ERR(PM_ERR_MEMORY, newpkg));
	STRDUP(newpkg->url, pkg->url, RET_ERR(PM_ERR_MEMORY, newpkg));
	newpkg->builddate = pkg->builddate;
	newpkg->installdate = pkg->installdate;
	STRDUP(newpkg->packager, pkg->packager, RET_ERR(PM_ERR_MEMORY, newpkg));
	STRDUP(newpkg->md5sum, pkg->md5sum, RET_ERR(PM_ERR_MEMORY, newpkg));
	STRDUP(newpkg->arch, pkg->arch, RET_ERR(PM_ERR_MEMORY, newpkg));
	newpkg->size = pkg->size;
	newpkg->isize = pkg->isize;
	newpkg->scriptlet = pkg->scriptlet;
	newpkg->epoch = pkg->epoch;
	newpkg->reason = pkg->reason;

	newpkg->licenses   = alpm_list_strdup(pkg->licenses);
	newpkg->replaces   = alpm_list_strdup(pkg->replaces);
	newpkg->groups     = alpm_list_strdup(pkg->groups);
	newpkg->files      = alpm_list_strdup(pkg->files);
	newpkg->backup     = alpm_list_strdup(pkg->backup);
	for(i = pkg->depends; i; i = alpm_list_next(i)) {
		newpkg->depends = alpm_list_add(newpkg->depends, _alpm_dep_dup(i->data));
	}
	newpkg->optdepends = alpm_list_strdup(pkg->optdepends);
	newpkg->conflicts  = alpm_list_strdup(pkg->conflicts);
	newpkg->provides   = alpm_list_strdup(pkg->provides);
	newpkg->deltas     = alpm_list_copy_data(pkg->deltas, sizeof(pmdelta_t));

	/* internal */
	newpkg->origin = pkg->origin;
	newpkg->ops = pkg->ops;
	if(newpkg->origin == PKG_FROM_FILE) {
		newpkg->origin_data.file = strdup(pkg->origin_data.file);
	} else {
		newpkg->origin_data.db = pkg->origin_data.db;
	}
	newpkg->infolevel = pkg->infolevel;

	return(newpkg);
}

void _alpm_pkg_free(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	if(pkg == NULL) {
		return;
	}

	FREE(pkg->filename);
	FREE(pkg->name);
	FREE(pkg->version);
	FREE(pkg->desc);
	FREE(pkg->url);
	FREE(pkg->packager);
	FREE(pkg->md5sum);
	FREE(pkg->arch);
	FREELIST(pkg->licenses);
	FREELIST(pkg->replaces);
	FREELIST(pkg->groups);
	FREELIST(pkg->files);
	FREELIST(pkg->backup);
	alpm_list_free_inner(pkg->depends, (alpm_list_fn_free)_alpm_dep_free);
	alpm_list_free(pkg->depends);
	FREELIST(pkg->optdepends);
	FREELIST(pkg->conflicts);
	FREELIST(pkg->provides);
	alpm_list_free_inner(pkg->deltas, (alpm_list_fn_free)_alpm_delta_free);
	alpm_list_free(pkg->deltas);
	alpm_list_free(pkg->delta_path);
	alpm_list_free(pkg->removes);

	if(pkg->origin == PKG_FROM_FILE) {
		FREE(pkg->origin_data.file);
	}
	FREE(pkg);
}

/* This function should be used when removing a target from upgrade/sync target list
 * Case 1: If pkg is a loaded package file (PKG_FROM_FILE), it will be freed.
 * Case 2: If pkg is a pkgcache entry (PKG_FROM_CACHE), it won't be freed,
 *         only the transaction specific fields of pkg will be freed.
 */
void _alpm_pkg_free_trans(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	if(pkg == NULL) {
		return;
	}

	if(pkg->origin == PKG_FROM_FILE) {
		_alpm_pkg_free(pkg);
		return;
	}

	alpm_list_free(pkg->removes);
	pkg->removes = NULL;
}

/* Is spkg an upgrade for localpkg? */
int _alpm_pkg_compare_versions(pmpkg_t *spkg, pmpkg_t *localpkg)
{
	int spkg_epoch, localpkg_epoch;

	ALPM_LOG_FUNC;

	spkg_epoch = alpm_pkg_get_epoch(spkg);
	localpkg_epoch = alpm_pkg_get_epoch(localpkg);

	if(spkg_epoch > localpkg_epoch) {
		return(1);
	} else if(spkg_epoch < localpkg_epoch) {
		return(-1);
	}

	/* equal epoch values, move on to version comparison */
	return alpm_pkg_vercmp(alpm_pkg_get_version(spkg),
			alpm_pkg_get_version(localpkg));
}

/* Helper function for comparing packages
 */
int _alpm_pkg_cmp(const void *p1, const void *p2)
{
	pmpkg_t *pkg1 = (pmpkg_t *)p1;
	pmpkg_t *pkg2 = (pmpkg_t *)p2;
	return(strcoll(pkg1->name, pkg2->name));
}

/* Test for existence of a package in a alpm_list_t*
 * of pmpkg_t*
 */
pmpkg_t *_alpm_pkg_find(alpm_list_t *haystack, const char *needle)
{
	alpm_list_t *lp;

	ALPM_LOG_FUNC;

	if(needle == NULL || haystack == NULL) {
		return(NULL);
	}

	for(lp = haystack; lp; lp = lp->next) {
		pmpkg_t *info = lp->data;

		if(info && strcmp(info->name, needle) == 0) {
			return(info);
		}
	}
	return(NULL);
}

/** Test if a package should be ignored.
 *
 * Checks if the package is ignored via IgnorePkg, or if the package is
 * in a group ignored via IgnoreGrp.
 *
 * @param pkg the package to test
 *
 * @return 1 if the package should be ignored, 0 otherwise
 */
int _alpm_pkg_should_ignore(pmpkg_t *pkg)
{
	alpm_list_t *groups = NULL;

	/* first see if the package is ignored */
	if(alpm_list_find_str(handle->ignorepkg, alpm_pkg_get_name(pkg))) {
		return(1);
	}

	/* next see if the package is in a group that is ignored */
	for(groups = handle->ignoregrp; groups; groups = alpm_list_next(groups)) {
		char *grp = (char *)alpm_list_getdata(groups);
		if(alpm_list_find_str(alpm_pkg_get_groups(pkg), grp)) {
			return(1);
		}
	}

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
