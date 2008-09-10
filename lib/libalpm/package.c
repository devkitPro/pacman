/*
 *  package.c
 *
 *  Copyright (c) 2002-2007 by Judd Vinet <jvinet@zeroflux.org>
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

/* libarchive */
#include <archive.h>
#include <archive_entry.h>

/* libalpm */
#include "package.h"
#include "alpm_list.h"
#include "log.h"
#include "util.h"
#include "db.h"
#include "cache.h"
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
	if(pkg->origin != PKG_FROM_CACHE) {
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
	ASSERT(pkg->origin == PKG_FROM_CACHE, RET_ERR(PM_ERR_PKG_INVALID, -1));
	ASSERT(pkg->origin_data.db != handle->db_local, RET_ERR(PM_ERR_PKG_INVALID, -1));

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

const char SYMEXPORT *alpm_pkg_get_filename(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_DESC);
	}

	return pkg->filename;
}

const char SYMEXPORT *alpm_pkg_get_name(pmpkg_t *pkg)
{
	ASSERT(pkg != NULL, return(NULL));
	return pkg->name;
}

const char SYMEXPORT *alpm_pkg_get_version(pmpkg_t *pkg)
{
	ASSERT(pkg != NULL, return(NULL));
	return pkg->version;
}

const char SYMEXPORT *alpm_pkg_get_desc(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_DESC);
	}
	return pkg->desc;
}

const char SYMEXPORT *alpm_pkg_get_url(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_DESC);
	}
	return pkg->url;
}

time_t SYMEXPORT alpm_pkg_get_builddate(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(0));
	ASSERT(pkg != NULL, return(0));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_DESC);
	}
	return pkg->builddate;
}

time_t SYMEXPORT alpm_pkg_get_installdate(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(0));
	ASSERT(pkg != NULL, return(0));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_DESC);
	}
	return pkg->installdate;
}

const char SYMEXPORT *alpm_pkg_get_packager(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_DESC);
	}
	return pkg->packager;
}

const char SYMEXPORT *alpm_pkg_get_md5sum(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_DESC);
	}
	return pkg->md5sum;
}

const char SYMEXPORT *alpm_pkg_get_arch(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_DESC);
	}
	return pkg->arch;
}

off_t SYMEXPORT alpm_pkg_get_size(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(-1));
	ASSERT(pkg != NULL, return(-1));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_DESC);
	}
	return pkg->size;
}

off_t SYMEXPORT alpm_pkg_get_isize(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(-1));
	ASSERT(pkg != NULL, return(-1));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_DESC);
	}
	return pkg->isize;
}

pmpkgreason_t SYMEXPORT alpm_pkg_get_reason(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(-1));
	ASSERT(pkg != NULL, return(-1));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_DESC);
	}
	return pkg->reason;
}

alpm_list_t SYMEXPORT *alpm_pkg_get_licenses(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_DESC);
	}
	return pkg->licenses;
}

alpm_list_t SYMEXPORT *alpm_pkg_get_groups(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_DESC);
	}
	return pkg->groups;
}

unsigned short SYMEXPORT alpm_pkg_has_force(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(-1));
	ASSERT(pkg != NULL, return(-1));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_DESC);
	}
	return pkg->force;
}

alpm_list_t SYMEXPORT *alpm_pkg_get_depends(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DEPENDS)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_DEPENDS);
	}
	return pkg->depends;
}

alpm_list_t SYMEXPORT *alpm_pkg_get_optdepends(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DEPENDS)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_DEPENDS);
	}
	return pkg->optdepends;
}

alpm_list_t SYMEXPORT *alpm_pkg_get_conflicts(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DEPENDS)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_DEPENDS);
	}
	return pkg->conflicts;
}

alpm_list_t SYMEXPORT *alpm_pkg_get_provides(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DEPENDS)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_DEPENDS);
	}
	return pkg->provides;
}

alpm_list_t SYMEXPORT *alpm_pkg_get_deltas(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DELTAS)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_DELTAS);
	}
	return pkg->deltas;
}

alpm_list_t SYMEXPORT *alpm_pkg_get_replaces(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_DESC);
	}
	return pkg->replaces;
}

alpm_list_t SYMEXPORT *alpm_pkg_get_files(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && pkg->origin_data.db == handle->db_local
		 && !(pkg->infolevel & INFRQ_FILES)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_FILES);
	}
	return pkg->files;
}

alpm_list_t SYMEXPORT *alpm_pkg_get_backup(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && pkg->origin_data.db == handle->db_local
		 && !(pkg->infolevel & INFRQ_FILES)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_FILES);
	}
	return pkg->backup;
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
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE) {
		char clfile[PATH_MAX];
		snprintf(clfile, PATH_MAX, "%s/%s/%s-%s/changelog",
				alpm_option_get_dbpath(),
				alpm_db_get_name(handle->db_local),
				alpm_pkg_get_name(pkg),
				alpm_pkg_get_version(pkg));
		return fopen(clfile, "r");
	} else if(pkg->origin == PKG_FROM_FILE) {
		struct archive *archive = NULL;
		struct archive_entry *entry;
		const char *pkgfile = pkg->origin_data.file;
		int ret = ARCHIVE_OK;

		if((archive = archive_read_new()) == NULL) {
			RET_ERR(PM_ERR_LIBARCHIVE, NULL);
		}

		archive_read_support_compression_all(archive);
		archive_read_support_format_all(archive);

		if (archive_read_open_filename(archive, pkgfile,
					ARCHIVE_DEFAULT_BYTES_PER_BLOCK) != ARCHIVE_OK) {
			RET_ERR(PM_ERR_PKG_OPEN, NULL);
		}

		while((ret = archive_read_next_header(archive, &entry)) == ARCHIVE_OK) {
			const char *entry_name = archive_entry_pathname(entry);

			if(strcmp(entry_name, ".CHANGELOG") == 0) {
				return(archive);
			}
		}
		/* we didn't find a changelog */
		archive_read_finish(archive);
		errno = ENOENT;
	}
	return(NULL);
}

/**
 * Read data from an open changelog 'file stream'. Similar to fread in
 * functionality, this function takes a buffer and amount of data to read.
 * @param ptr a buffer to fill with raw changelog data
 * @param size the size of the buffer
 * @param pkg the package that the changelog is being read from
 * @param fp a 'file stream' to the package changelog
 * @return the number of characters read, or 0 if there is no more data
 */
size_t SYMEXPORT alpm_pkg_changelog_read(void *ptr, size_t size,
		const pmpkg_t *pkg, const void *fp)
{
	size_t ret = 0;
	if(pkg->origin == PKG_FROM_CACHE) {
		ret = fread(ptr, 1, size, (FILE*)fp);
	} else if(pkg->origin == PKG_FROM_FILE) {
		ret = archive_read_data((struct archive*)fp, ptr, size);
	}
	return(ret);
}

/*
int SYMEXPORT alpm_pkg_changelog_feof(const pmpkg_t *pkg, void *fp)
{
	int ret = 0;
	if(pkg->origin == PKG_FROM_CACHE) {
		ret = feof((FILE*)fp);
	} else if(pkg->origin == PKG_FROM_FILE) {
		// note: this doesn't quite work, no feof in libarchive
		ret = archive_read_data((struct archive*)fp, NULL, 0);
	}
	return(ret);
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
	int ret = 0;
	if(pkg->origin == PKG_FROM_CACHE) {
		ret = fclose((FILE*)fp);
	} else if(pkg->origin == PKG_FROM_FILE) {
		ret = archive_read_finish((struct archive *)fp);
	}
	return(ret);
}

unsigned short SYMEXPORT alpm_pkg_has_scriptlet(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(-1));
	ASSERT(pkg != NULL, return(-1));

	if(pkg->origin == PKG_FROM_CACHE && pkg->origin_data.db == handle->db_local
		 && !(pkg->infolevel & INFRQ_SCRIPTLET)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_SCRIPTLET);
	}
	return pkg->scriptlet;
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

	pmdb_t *localdb = alpm_option_get_localdb();
	for(i = _alpm_db_get_pkgcache(localdb); i; i = i->next) {
		if(!i->data) {
			continue;
		}
		pmpkg_t *cachepkg = i->data;
		if(_alpm_dep_edge(cachepkg, pkg)) {
			const char *cachepkgname = alpm_pkg_get_name(cachepkg);
			reqs = alpm_list_add(reqs, strdup(cachepkgname));
		}
	}
	return(reqs);
}

/** Compare two version strings and determine which one is 'newer'.
 * Returns a value comparable to the way strcmp works. Returns 1
 * if a is newer than b, 0 if a and b are the same version, or -1
 * if b is newer than a.
 *
 * This function has been adopted from the rpmvercmp function located
 * at lib/rpmvercmp.c, and was most recently updated against rpm
 * version 4.4.2.3. Small modifications have been made to make it more
 * consistent with the libalpm coding style.
 *
 * Keep in mind that the pkgrel is only compared if it is available
 * on both versions handed to this function. For example, comparing
 * 1.5-1 and 1.5 will yield 0; comparing 1.5-1 and 1.5-2 will yield
 * -1 as expected. This is mainly for supporting versioned dependencies
 * that do not include the pkgrel.
 */
int SYMEXPORT alpm_pkg_vercmp(const char *a, const char *b)
{
	char oldch1, oldch2;
	char *str1, *str2;
	char *ptr1, *ptr2;
	char *one, *two;
	int rc;
	int isnum;
	int ret = 0;

	ALPM_LOG_FUNC;

	/* libalpm added code. ensure our strings are not null */
	if(!a) {
		if(!b) return(0);
		return(-1);
	}
	if(!b) return(1);

	/* easy comparison to see if versions are identical */
	if(strcmp(a, b) == 0) return(0);

	str1 = strdup(a);
	str2 = strdup(b);

	one = str1;
	two = str2;

	/* loop through each version segment of str1 and str2 and compare them */
	while(*one && *two) {
		while(*one && !isalnum((int)*one)) one++;
		while(*two && !isalnum((int)*two)) two++;

		/* If we ran to the end of either, we are finished with the loop */
		if(!(*one && *two)) break;

		ptr1 = one;
		ptr2 = two;

		/* grab first completely alpha or completely numeric segment */
		/* leave one and two pointing to the start of the alpha or numeric */
		/* segment and walk ptr1 and ptr2 to end of segment */
		if(isdigit((int)*ptr1)) {
			while(*ptr1 && isdigit((int)*ptr1)) ptr1++;
			while(*ptr2 && isdigit((int)*ptr2)) ptr2++;
			isnum = 1;
		} else {
			while(*ptr1 && isalpha((int)*ptr1)) ptr1++;
			while(*ptr2 && isalpha((int)*ptr2)) ptr2++;
			isnum = 0;
		}

		/* save character at the end of the alpha or numeric segment */
		/* so that they can be restored after the comparison */
		oldch1 = *ptr1;
		*ptr1 = '\0';
		oldch2 = *ptr2;
		*ptr2 = '\0';

		/* this cannot happen, as we previously tested to make sure that */
		/* the first string has a non-null segment */
		if (one == ptr1) {
			ret = -1;	/* arbitrary */
			goto cleanup;
		}

		/* take care of the case where the two version segments are */
		/* different types: one numeric, the other alpha (i.e. empty) */
		/* numeric segments are always newer than alpha segments */
		/* XXX See patch #60884 (and details) from bugzilla #50977. */
		if (two == ptr2) {
			ret = isnum ? 1 : -1;
			goto cleanup;
		}

		if (isnum) {
			/* this used to be done by converting the digit segments */
			/* to ints using atoi() - it's changed because long  */
			/* digit segments can overflow an int - this should fix that. */

			/* throw away any leading zeros - it's a number, right? */
			while (*one == '0') one++;
			while (*two == '0') two++;

			/* whichever number has more digits wins */
			if (strlen(one) > strlen(two)) {
				ret = 1;
				goto cleanup;
			}
			if (strlen(two) > strlen(one)) {
				ret = -1;
				goto cleanup;
			}
		}

		/* strcmp will return which one is greater - even if the two */
		/* segments are alpha or if they are numeric.  don't return  */
		/* if they are equal because there might be more segments to */
		/* compare */
		rc = strcmp(one, two);
		if (rc) {
			ret = rc < 1 ? -1 : 1;
			goto cleanup;
		}

		/* restore character that was replaced by null above */
		*ptr1 = oldch1;
		one = ptr1;
		*ptr2 = oldch2;
		two = ptr2;

		/* libalpm added code. check if version strings have hit the pkgrel
		 * portion. depending on which strings have hit, take correct action.
		 * this is all based on the premise that we only have one dash in
		 * the version string, and it separates pkgver from pkgrel. */
		if(*ptr1 == '-' && *ptr2 == '-') {
			/* no-op, continue comparing since we are equivalent throughout */
		} else if(*ptr1 == '-') {
			/* ptr1 has hit the pkgrel and ptr2 has not. continue version
			 * comparison after stripping the pkgrel from ptr1. */
			*ptr1 = '\0';
		} else if(*ptr2 == '-') {
			/* ptr2 has hit the pkgrel and ptr1 has not. continue version
			 * comparison after stripping the pkgrel from ptr2. */
			*ptr2 = '\0';
		}
	}

	/* this catches the case where all numeric and alpha segments have */
	/* compared identically but the segment separating characters were */
	/* different */
	if ((!*one) && (!*two)) {
		ret = 0;
		goto cleanup;
	}

	/* the final showdown. we never want a remaining alpha string to
	 * beat an empty string. the logic is a bit weird, but:
	 * - if one is empty and two is not an alpha, two is newer.
	 * - if one is an alpha, two is newer.
	 * - otherwise one is newer.
	 * */
	if ( ( !*one && !isalpha((int)*two) )
			|| isalpha((int)*one) ) {
		ret = -1;
	} else {
		ret = 1;
	}

cleanup:
	free(str1);
	free(str2);
	return(ret);
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
	newpkg->force = pkg->force;
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

	if(pkg->origin == PKG_FROM_FILE) {
		FREE(pkg->origin_data.file);
	}
	FREE(pkg);
}

/* Is spkg an upgrade for locapkg? */
int _alpm_pkg_compare_versions(pmpkg_t *spkg, pmpkg_t *localpkg)
{
	int cmp = 0;

	ALPM_LOG_FUNC;

	cmp = alpm_pkg_vercmp(alpm_pkg_get_version(spkg),
			alpm_pkg_get_version(localpkg));

	if(cmp < 0 && alpm_pkg_has_force(spkg)) {
		cmp = 1;
	}

	return(cmp);
}

/* Helper function for comparing packages
 */
int _alpm_pkg_cmp(const void *p1, const void *p2)
{
	pmpkg_t *pkg1 = (pmpkg_t *)p1;
	pmpkg_t *pkg2 = (pmpkg_t *)p2;
	return(strcmp(alpm_pkg_get_name(pkg1), alpm_pkg_get_name(pkg2)));
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

		if(info && strcmp(alpm_pkg_get_name(info), needle) == 0) {
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
