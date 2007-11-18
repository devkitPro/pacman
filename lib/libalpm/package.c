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
#include <unistd.h>
#include <locale.h> /* setlocale */

/* libarchive */
#include <archive.h>
#include <archive_entry.h>

/* libalpm */
#include "package.h"
#include "alpm_list.h"
#include "log.h"
#include "util.h"
#include "error.h"
#include "db.h"
#include "cache.h"
#include "delta.h"
#include "handle.h"
#include "deps.h"

/** \addtogroup alpm_packages Package Functions
 * @brief Functions to manipulate libalpm packages
 * @{
 */

/** Create a package from a file.
 * @param filename location of the package tarball
 * @param full whether to stop the load after metadata is read or continue
 *             through the full archive
 * @param pkg address of the package pointer
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_pkg_load(const char *filename, unsigned short full,
		pmpkg_t **pkg)
{
	_alpm_log(PM_LOG_FUNCTION, "enter alpm_pkg_load\n");

	/* Sanity checks */
	ASSERT(filename != NULL && strlen(filename) != 0,
			RET_ERR(PM_ERR_WRONG_ARGS, -1));
	ASSERT(pkg != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	*pkg = _alpm_pkg_load(filename, full);
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
	_alpm_log(PM_LOG_FUNCTION, "enter alpm_pkg_free\n");

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
	char *md5sum = NULL;
	int retval = 0;

	ALPM_LOG_FUNC;

	ASSERT(pkg != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));
	/* We only inspect packages from sync repositories */
	ASSERT(pkg->origin == PKG_FROM_CACHE, RET_ERR(PM_ERR_PKG_INVALID, -1));
	ASSERT(pkg->origin_data.db != handle->db_local, RET_ERR(PM_ERR_PKG_INVALID, -1));

	fpath = _alpm_filecache_find(alpm_pkg_get_filename(pkg));
	md5sum = alpm_get_md5sum(fpath);

	if(md5sum == NULL) {
		_alpm_log(PM_LOG_ERROR, _("could not get md5sum for package %s-%s\n"),
							alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
		pm_errno = PM_ERR_NOT_A_FILE;
		retval = -1;
	} else {
		if(strcmp(md5sum, alpm_pkg_get_md5sum(pkg)) == 0) {
			_alpm_log(PM_LOG_DEBUG, "md5sums for package %s-%s match\n",
								alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
		} else {
			_alpm_log(PM_LOG_ERROR, _("md5sums do not match for package %s-%s\n"),
								alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
			pm_errno = PM_ERR_PKG_INVALID;
			retval = -1;
		}
	}

	FREE(fpath);
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

const char SYMEXPORT *alpm_pkg_get_filename(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->filename == NULL || strlen(pkg->filename) == 0) {
		/* construct the file name, it's not in the desc file */
		if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
			_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_DESC);
		}
		char buffer[PATH_MAX];
		if(pkg->arch && strlen(pkg->arch) > 0) {
			snprintf(buffer, PATH_MAX, "%s-%s-%s" PKGEXT,
			         pkg->name, pkg->version, pkg->arch);
		} else {
			snprintf(buffer, PATH_MAX, "%s-%s" PKGEXT,
			         pkg->name, pkg->version);
		}
		STRDUP(pkg->filename, buffer, RET_ERR(PM_ERR_MEMORY, NULL));
	}

	return pkg->filename;
}

const char SYMEXPORT *alpm_pkg_get_name(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_BASE)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_BASE);
	}
	return pkg->name;
}

const char SYMEXPORT *alpm_pkg_get_version(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_BASE)) {
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_BASE);
	}
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

unsigned long SYMEXPORT alpm_pkg_get_size(pmpkg_t *pkg)
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

unsigned long SYMEXPORT alpm_pkg_get_isize(pmpkg_t *pkg)
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
			RET_ERR(PM_ERR_LIBARCHIVE_ERROR, NULL);
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
 *
 * A depends on B through n depends <=> A listed in B's requiredby n times
 * n == 0 or 1 in almost all cases */
alpm_list_t SYMEXPORT *alpm_pkg_compute_requiredby(pmpkg_t *pkg)
{
	const alpm_list_t *i, *j;
	alpm_list_t *reqs = NULL;

	pmdb_t *localdb = alpm_option_get_localdb();
	for(i = _alpm_db_get_pkgcache(localdb); i; i = i->next) {
		if(!i->data) {
			continue;
		}
		pmpkg_t *cachepkg = i->data;
		const char *cachepkgname = alpm_pkg_get_name(cachepkg);

		for(j = alpm_pkg_get_depends(cachepkg); j; j = j->next) {
			pmdepend_t *dep = j->data;

			if(alpm_depcmp(pkg, dep)) {
				_alpm_log(PM_LOG_DEBUG, "adding '%s' in requiredby field for '%s'\n",
				          cachepkgname, pkg->name);
				reqs = alpm_list_add(reqs, strdup(cachepkgname));
			}
		}
	}
	return(reqs);
}

/** @} */

/* this function was taken from rpm 4.0.4 and rewritten */
int _alpm_versioncmp(const char *a, const char *b)
{
	char str1[64], str2[64];
	char *ptr1, *ptr2;
	char *one, *two;
	char *rel1 = NULL, *rel2 = NULL;
	char oldch1, oldch2;
	int is1num, is2num;
	int rc;

	ALPM_LOG_FUNC;

	if(!strcmp(a,b)) {
		return(0);
	}

	strncpy(str1, a, 64);
	str1[63] = 0;
	strncpy(str2, b, 64);
	str2[63] = 0;

	/* lose the release number */
	for(one = str1; *one && *one != '-'; one++);
	if(one) {
		*one = '\0';
		rel1 = ++one;
	}
	for(two = str2; *two && *two != '-'; two++);
	if(two) {
		*two = '\0';
		rel2 = ++two;
	}

	one = str1;
	two = str2;

	while(*one || *two) {
		while(*one && !isalnum((int)*one)) one++;
		while(*two && !isalnum((int)*two)) two++;

		ptr1 = one;
		ptr2 = two;

		/* find the next segment for each string */
		if(isdigit((int)*ptr1)) {
			is1num = 1;
			while(*ptr1 && isdigit((int)*ptr1)) ptr1++;
		} else {
			is1num = 0;
			while(*ptr1 && isalpha((int)*ptr1)) ptr1++;
		}
		if(isdigit((int)*ptr2)) {
			is2num = 1;
			while(*ptr2 && isdigit((int)*ptr2)) ptr2++;
		} else {
			is2num = 0;
			while(*ptr2 && isalpha((int)*ptr2)) ptr2++;
		}

		oldch1 = *ptr1;
		*ptr1 = '\0';
		oldch2 = *ptr2;
		*ptr2 = '\0';

		/* see if we ran out of segments on one string */
		if(one == ptr1 && two != ptr2) {
			return(is2num ? -1 : 1);
		}
		if(one != ptr1 && two == ptr2) {
			return(is1num ? 1 : -1);
		}

		/* see if we have a type mismatch (ie, one is alpha and one is digits) */
		if(is1num && !is2num) return(1);
		if(!is1num && is2num) return(-1);

		if(is1num) while(*one == '0') one++;
		if(is2num) while(*two == '0') two++;

		rc = strverscmp(one, two);
		if(rc) return(rc);

		*ptr1 = oldch1;
		*ptr2 = oldch2;
		one = ptr1;
		two = ptr2;
	}

	if((!*one) && (!*two)) {
		/* compare release numbers */
		if(rel1 && rel2 && strlen(rel1) && strlen(rel2)) return(_alpm_versioncmp(rel1, rel2));
		return(0);
	}

	return(*one ? 1 : -1);
}


pmpkg_t *_alpm_pkg_new(const char *name, const char *version)
{
	pmpkg_t* pkg;

	ALPM_LOG_FUNC;

	CALLOC(pkg, 1, sizeof(pmpkg_t), RET_ERR(PM_ERR_MEMORY, NULL));

	if(name) {
		STRDUP(pkg->name, name, RET_ERR(PM_ERR_MEMORY, pkg));
	}

	if(version) {
		STRDUP(pkg->version, version, RET_ERR(PM_ERR_MEMORY, pkg));
	}

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

	newpkg->licenses   = alpm_list_strdup(alpm_pkg_get_licenses(pkg));
	newpkg->replaces   = alpm_list_strdup(alpm_pkg_get_replaces(pkg));
	newpkg->groups     = alpm_list_strdup(alpm_pkg_get_groups(pkg));
	newpkg->files      = alpm_list_strdup(alpm_pkg_get_files(pkg));
	newpkg->backup     = alpm_list_strdup(alpm_pkg_get_backup(pkg));
	for(i = alpm_pkg_get_depends(pkg); i; i = alpm_list_next(i)) {
		newpkg->depends = alpm_list_add(newpkg->depends, _alpm_dep_dup(i->data));
	}
	newpkg->optdepends = alpm_list_strdup(alpm_pkg_get_optdepends(pkg));
	newpkg->conflicts  = alpm_list_strdup(alpm_pkg_get_conflicts(pkg));
	newpkg->provides   = alpm_list_strdup(alpm_pkg_get_provides(pkg));
	newpkg->deltas     = alpm_list_copy_data(alpm_pkg_get_deltas(pkg),
																					 sizeof(pmdelta_t));

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

	if(pkg->origin == PKG_FROM_FILE) {
		FREE(pkg->origin_data.file);
	}
	FREE(pkg);
}

/* Is pkgB an upgrade for pkgA ? */
int _alpm_pkg_compare_versions(pmpkg_t *local_pkg, pmpkg_t *pkg)
{
	int cmp = 0;

	ALPM_LOG_FUNC;

	if(pkg->origin == PKG_FROM_CACHE) {
		/* ensure we have the /desc file, which contains the 'force' option */
		_alpm_db_read(pkg->origin_data.db, pkg, INFRQ_DESC);
	}

	/* compare versions and see if we need to upgrade */
	cmp = _alpm_versioncmp(alpm_pkg_get_version(pkg), alpm_pkg_get_version(local_pkg));

	if(cmp != 0 && pkg->force) {
		cmp = 1;
		_alpm_log(PM_LOG_WARNING, _("%s: forcing upgrade to version %s\n"),
							alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
	} else if(cmp < 0) {
		/* local version is newer */
		pmdb_t *db = pkg->origin_data.db;
		_alpm_log(PM_LOG_WARNING, _("%s: local (%s) is newer than %s (%s)\n"),
							alpm_pkg_get_name(local_pkg), alpm_pkg_get_version(local_pkg),
							alpm_db_get_name(db), alpm_pkg_get_version(pkg));
		cmp = 0;
	}

	return(cmp);
}

/* Helper function for comparing packages
 */
int _alpm_pkg_cmp(const void *p1, const void *p2)
{
	pmpkg_t *pk1 = (pmpkg_t *)p1;
	pmpkg_t *pk2 = (pmpkg_t *)p2;

	return(strcmp(alpm_pkg_get_name(pk1), alpm_pkg_get_name(pk2)));
}

int _alpm_pkgname_pkg_cmp(const void *pkgname, const void *package)
{
	return(strcmp(alpm_pkg_get_name((pmpkg_t *) package), (char *) pkgname));
}


/* Parses the package description file for the current package
 * TODO: this should ALL be in a backend interface (be_files), we should
 *       be dealing with the abstracted concepts only in this file
 * Returns: 0 on success, 1 on error
 *
 */
static int parse_descfile(const char *descfile, pmpkg_t *info)
{
	FILE* fp = NULL;
	char line[PATH_MAX];
	char *ptr = NULL;
	char *key = NULL;
	int linenum = 0;

	ALPM_LOG_FUNC;

	if((fp = fopen(descfile, "r")) == NULL) {
		_alpm_log(PM_LOG_ERROR, _("could not open file %s: %s\n"), descfile, strerror(errno));
		return(-1);
	}

	while(!feof(fp)) {
		fgets(line, PATH_MAX, fp);
		linenum++;
		_alpm_strtrim(line);
		if(strlen(line) == 0 || line[0] == '#') {
			continue;
		}
		ptr = line;
		key = strsep(&ptr, "=");
		if(key == NULL || ptr == NULL) {
			_alpm_log(PM_LOG_DEBUG, "%s: syntax error in description file line %d\n",
								info->name[0] != '\0' ? info->name : "error", linenum);
		} else {
			key = _alpm_strtrim(key);
			ptr = _alpm_strtrim(ptr);
			if(!strcmp(key, "pkgname")) {
				STRDUP(info->name, ptr, RET_ERR(PM_ERR_MEMORY, -1));
			} else if(!strcmp(key, "pkgver")) {
				STRDUP(info->version, ptr, RET_ERR(PM_ERR_MEMORY, -1));
			} else if(!strcmp(key, "pkgdesc")) {
				STRDUP(info->desc, ptr, RET_ERR(PM_ERR_MEMORY, -1));
			} else if(!strcmp(key, "group")) {
				info->groups = alpm_list_add(info->groups, strdup(ptr));
			} else if(!strcmp(key, "url")) {
				STRDUP(info->url, ptr, RET_ERR(PM_ERR_MEMORY, -1));
			} else if(!strcmp(key, "license")) {
				info->licenses = alpm_list_add(info->licenses, strdup(ptr));
			} else if(!strcmp(key, "builddate")) {
				char first = tolower(ptr[0]);
				if(first > 'a' && first < 'z') {
					struct tm tmp_tm = {0}; //initialize to null incase of failure
					setlocale(LC_TIME, "C");
					strptime(ptr, "%a %b %e %H:%M:%S %Y", &tmp_tm);
					info->builddate = mktime(&tmp_tm);
					setlocale(LC_TIME, "");
				} else {
					info->builddate = atol(ptr);
				}
			} else if(!strcmp(key, "packager")) {
				STRDUP(info->packager, ptr, RET_ERR(PM_ERR_MEMORY, -1));
			} else if(!strcmp(key, "arch")) {
				STRDUP(info->arch, ptr, RET_ERR(PM_ERR_MEMORY, -1));
			} else if(!strcmp(key, "size")) {
				/* size in the raw package is uncompressed (installed) size */
				info->isize = atol(ptr);
			} else if(!strcmp(key, "depend")) {
				pmdepend_t *dep = _alpm_splitdep(ptr);
				info->depends = alpm_list_add(info->depends, dep);
			} else if(!strcmp(key, "optdepend")) {
				info->optdepends = alpm_list_add(info->optdepends, strdup(ptr));
			} else if(!strcmp(key, "conflict")) {
				info->conflicts = alpm_list_add(info->conflicts, strdup(ptr));
			} else if(!strcmp(key, "replaces")) {
				info->replaces = alpm_list_add(info->replaces, strdup(ptr));
			} else if(!strcmp(key, "provides")) {
				info->provides = alpm_list_add(info->provides, strdup(ptr));
			} else if(!strcmp(key, "backup")) {
				info->backup = alpm_list_add(info->backup, strdup(ptr));
			} else {
				_alpm_log(PM_LOG_DEBUG, "%s: syntax error in description file line %d\n",
									info->name[0] != '\0' ? info->name : "error", linenum);
			}
		}
		line[0] = '\0';
	}
	fclose(fp);
	unlink(descfile);

	return(0);
}


/**
 * Load a package and create the corresponding pmpkg_t struct.
 * @param pkgfile path to the package file
 * @param full whether to stop the load after metadata is read or continue
 *             through the full archive
 * @return An information filled pmpkg_t struct
 */
pmpkg_t *_alpm_pkg_load(const char *pkgfile, unsigned short full)
{
	int ret = ARCHIVE_OK;
	int config = 0;
	struct archive *archive;
	struct archive_entry *entry;
	pmpkg_t *info = NULL;
	char *descfile = NULL;
	int fd = -1;
	struct stat st;

	ALPM_LOG_FUNC;

	if(pkgfile == NULL || strlen(pkgfile) == 0) {
		RET_ERR(PM_ERR_WRONG_ARGS, NULL);
	}

	if((archive = archive_read_new()) == NULL) {
		RET_ERR(PM_ERR_LIBARCHIVE_ERROR, NULL);
	}

	archive_read_support_compression_all(archive);
	archive_read_support_format_all(archive);

	if (archive_read_open_filename(archive, pkgfile,
				ARCHIVE_DEFAULT_BYTES_PER_BLOCK) != ARCHIVE_OK) {
		RET_ERR(PM_ERR_PKG_OPEN, NULL);
	}

	info = _alpm_pkg_new(NULL, NULL);
	if(info == NULL) {
		archive_read_finish(archive);
		RET_ERR(PM_ERR_MEMORY, NULL);
	}

  if(stat(pkgfile, &st) == 0) {
		info->size = st.st_size;
	}

	/* TODO there is no reason to make temp files to read
	 * from a libarchive archive, it can be done by reading
	 * directly from the archive
	 * See: archive_read_data_into_buffer
	 * requires changes 'parse_descfile' as well
	 * */

	/* If full is false, only read through the archive until we find our needed
	 * metadata. If it is true, read through the entire archive, which serves
	 * as a verfication of integrity and allows us to create the filelist. */
	while((ret = archive_read_next_header(archive, &entry)) == ARCHIVE_OK) {
		const char *entry_name = archive_entry_pathname(entry);

		/* NOTE: we used to look for .FILELIST, but it is easier (and safer) for
		 * us to just generate this on our own. */
		if(strcmp(entry_name, ".PKGINFO") == 0) {
			/* extract this file into /tmp. it has info for us */
			descfile = strdup("/tmp/alpm_XXXXXX");
			fd = mkstemp(descfile);
			if(archive_read_data_into_fd(archive, fd) != ARCHIVE_OK) {
				_alpm_log(PM_LOG_ERROR, _("error extracting package description file to %s\n"),
						descfile);
				goto pkg_invalid;
			}
			/* parse the info file */
			if(parse_descfile(descfile, info) == -1) {
				_alpm_log(PM_LOG_ERROR, _("could not parse package description file in %s\n"),
						pkgfile);
				goto pkg_invalid;
			}
			if(info->name == NULL || strlen(info->name) == 0) {
				_alpm_log(PM_LOG_ERROR, _("missing package name in %s\n"), pkgfile);
				goto pkg_invalid;
			}
			if(info->version == NULL || strlen(info->version) == 0) {
				_alpm_log(PM_LOG_ERROR, _("missing package version in %s\n"), pkgfile);
				goto pkg_invalid;
			}
			config = 1;
			unlink(descfile);
			FREE(descfile);
			close(fd);
			continue;
		} else if(strcmp(entry_name,  ".INSTALL") == 0) {
			info->scriptlet = 1;
		} else if(*entry_name == '.') {
			/* for now, ignore all files starting with '.' that haven't
			 * already been handled (for future possibilities) */
		} else {
			/* Keep track of all files for filelist generation */
			info->files = alpm_list_add(info->files, strdup(entry_name));
		}

		if(archive_read_data_skip(archive)) {
			_alpm_log(PM_LOG_ERROR, _("error while reading package %s: %s\n"),
					pkgfile, archive_error_string(archive));
			pm_errno = PM_ERR_LIBARCHIVE_ERROR;
			goto error;
		}

		/* if we are not doing a full read, see if we have all we need */
		if(!full && config) {
			break;
		}
	}

	if(ret != ARCHIVE_EOF && ret != ARCHIVE_OK) { /* An error occured */
		_alpm_log(PM_LOG_ERROR, _("error while reading package %s: %s\n"),
				pkgfile, archive_error_string(archive));
		pm_errno = PM_ERR_LIBARCHIVE_ERROR;
		goto error;
	}

	if(!config) {
		_alpm_log(PM_LOG_ERROR, _("missing package metadata in %s\n"), pkgfile);
		goto error;
	}

  archive_read_finish(archive);

	/* internal fields for package struct */
	info->origin = PKG_FROM_FILE;
	info->origin_data.file = strdup(pkgfile);

	if(full) {
		/* "checking for conflicts" requires a sorted list, so we ensure that here */
		_alpm_log(PM_LOG_DEBUG, "sorting package filelist for %s\n", pkgfile);
		info->files = alpm_list_msort(info->files, alpm_list_count(info->files),
				_alpm_str_cmp);
		info->infolevel = INFRQ_ALL;
	} else {
		/* get rid of any partial filelist we may have collected, as it is invalid */
		FREELIST(info->files);
		info->infolevel = INFRQ_BASE | INFRQ_DESC | INFRQ_DEPENDS;
	}

	return(info);

pkg_invalid:
	pm_errno = PM_ERR_PKG_INVALID;
	if(descfile) {
		unlink(descfile);
		FREE(descfile);
	}
	if(fd != -1) {
		close(fd);
	}
error:
	_alpm_pkg_free(info);
	archive_read_finish(archive);

	return(NULL);
}

/* Test for existence of a package in a alpm_list_t*
 * of pmpkg_t*
 */
pmpkg_t *_alpm_pkg_find(const char *needle, alpm_list_t *haystack)
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
