/*
 *  package.c
 *
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
 *  USA.
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
#include "provide.h"
#include "handle.h"
#include "alpm.h"

/** \addtogroup alpm_packages Package Functions
 * @brief Functions to manipulate libalpm packages
 * @{
 */

/** Create a package from a file.
 * @param filename location of the package tarball
 * @param pkg address of the package pointer
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_pkg_load(const char *filename, pmpkg_t **pkg)
{
	_alpm_log(PM_LOG_FUNCTION, "enter alpm_pkg_load");

	/* Sanity checks */
	ASSERT(filename != NULL && strlen(filename) != 0, RET_ERR(PM_ERR_WRONG_ARGS, -1));
	ASSERT(pkg != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	*pkg = _alpm_pkg_load(filename);
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
	_alpm_log(PM_LOG_FUNCTION, "enter alpm_pkg_free");

	ASSERT(pkg != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	/* Only free packages loaded in user space */
	if(pkg->origin != PKG_FROM_CACHE) {
		_alpm_pkg_free(pkg);
	}

	return(0);
}

/** Check the integrity (with sha1) of a package from the sync cache.
 * @param pkg package pointer
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_pkg_checksha1sum(pmpkg_t *pkg)
{
	char path[PATH_MAX];
	struct stat buf;
	char *sha1sum = NULL;
	alpm_list_t *i;
	int retval = 0;

	ALPM_LOG_FUNC;

	ASSERT(pkg != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));
	/* We only inspect packages from sync repositories */
	ASSERT(pkg->origin == PKG_FROM_CACHE, RET_ERR(PM_ERR_PKG_INVALID, -1));
	ASSERT(pkg->data != handle->db_local, RET_ERR(PM_ERR_PKG_INVALID, -1));

	/* Loop through the cache dirs until we find a matching file */
	for(i = alpm_option_get_cachedirs(); i; i = alpm_list_next(i)) {
		snprintf(path, PATH_MAX, "%s%s-%s" PKGEXT, (char*)alpm_list_getdata(i),
		         alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
		if(stat(path, &buf) == 0) {
			break;
		}
	}

	sha1sum = alpm_get_sha1sum(path);
	if(sha1sum == NULL) {
		_alpm_log(PM_LOG_ERROR, _("could not get sha1sum for package %s-%s"),
							alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
		pm_errno = PM_ERR_NOT_A_FILE;
		retval = -1;
	} else {
		if(strcmp(sha1sum, alpm_pkg_get_sha1sum(pkg)) == 0) {
			_alpm_log(PM_LOG_DEBUG, "sha1sums for package %s-%s match",
								alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
		} else {
			_alpm_log(PM_LOG_ERROR, _("sha1sums do not match for package %s-%s"),
								alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
			pm_errno = PM_ERR_PKG_INVALID;
			retval = -1;
		}
	}

	FREE(sha1sum);

	return(retval);
}

/** Check the integrity (with md5) of a package from the sync cache.
 * @param pkg package pointer
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_pkg_checkmd5sum(pmpkg_t *pkg)
{
	char path[PATH_MAX];
	struct stat buf;
	char *md5sum = NULL;
	alpm_list_t *i;
	int retval = 0;

	ALPM_LOG_FUNC;

	ASSERT(pkg != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));
	/* We only inspect packages from sync repositories */
	ASSERT(pkg->origin == PKG_FROM_CACHE, RET_ERR(PM_ERR_PKG_INVALID, -1));
	ASSERT(pkg->data != handle->db_local, RET_ERR(PM_ERR_PKG_INVALID, -1));

	/* Loop through the cache dirs until we find a matching file */
	for(i = alpm_option_get_cachedirs(); i; i = alpm_list_next(i)) {
		snprintf(path, PATH_MAX, "%s%s-%s" PKGEXT, (char*)alpm_list_getdata(i),
		         alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
		if(stat(path, &buf) == 0) {
			break;
		}
	}

	md5sum = alpm_get_md5sum(path);
	if(md5sum == NULL) {
		_alpm_log(PM_LOG_ERROR, _("could not get md5sum for package %s-%s"),
							alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
		pm_errno = PM_ERR_NOT_A_FILE;
		retval = -1;
	} else {
		if(strcmp(md5sum, alpm_pkg_get_md5sum(pkg)) == 0) {
			_alpm_log(PM_LOG_DEBUG, "md5sums for package %s-%s match",
								alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
		} else {
			_alpm_log(PM_LOG_ERROR, _("md5sums do not match for package %s-%s"),
								alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
			pm_errno = PM_ERR_PKG_INVALID;
			retval = -1;
		}
	}

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

/* internal */
static char *_supported_archs[] = {
	"i586",
	"i686",
	"ppc",
	"x86_64",
};

/**
 * @brief Determine if a package name has -ARCH tacked on.
 * @param pkgname name of the package to parse
 * @return pointer to start of -ARCH text if it exists, else NULL
 */
char SYMEXPORT *alpm_pkg_name_hasarch(const char *pkgname)
{
	/* TODO remove this when we transfer everything over to -ARCH
	 *
	 * this parsing sucks... it's done to support
	 * two package formats for the time being:
	 *    package-name-foo-1.0.0-1-i686
	 * and
	 *    package-name-bar-1.2.3-1
	 */
	size_t i = 0;
	char *arch, *cmp, *p;

	ALPM_LOG_FUNC;

	if((p = strrchr(pkgname, '-'))) {
		for(i=0; i < sizeof(_supported_archs)/sizeof(char*); ++i) {
			cmp = p+1;
			arch = _supported_archs[i];

			/* whee, case insensitive compare */
			while(*arch && *cmp && tolower(*arch++) == tolower(*cmp++)) ;
			if(*arch || *cmp) {
				continue;
			}

			return(p);
		}
	}
	return(NULL);
}

const char SYMEXPORT *alpm_pkg_get_filename(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(!strlen(pkg->filename)) {
		/* construct the file name, it's not in the desc file */
		if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
			_alpm_db_read(pkg->data, pkg, INFRQ_DESC);
		}
		if(pkg->arch && strlen(pkg->arch) > 0) {
			snprintf(pkg->filename, PKG_FILENAME_LEN, "%s-%s-%s" PKGEXT,
			         pkg->name, pkg->version, pkg->arch);
		} else {
			snprintf(pkg->filename, PKG_FILENAME_LEN, "%s-%s" PKGEXT,
			         pkg->name, pkg->version);
		}
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
		_alpm_db_read(pkg->data, pkg, INFRQ_BASE);
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
		_alpm_db_read(pkg->data, pkg, INFRQ_BASE);
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
		_alpm_db_read(pkg->data, pkg, INFRQ_DESC);
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
		_alpm_db_read(pkg->data, pkg, INFRQ_DESC);
	}
	return pkg->url;
}

const char SYMEXPORT *alpm_pkg_get_builddate(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->data, pkg, INFRQ_DESC);
	}
	return pkg->builddate;
}

const char SYMEXPORT *alpm_pkg_get_installdate(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->data, pkg, INFRQ_DESC);
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
		_alpm_db_read(pkg->data, pkg, INFRQ_DESC);
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
		_alpm_db_read(pkg->data, pkg, INFRQ_DESC);
	}
	return pkg->md5sum;
}

const char SYMEXPORT *alpm_pkg_get_sha1sum(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->data, pkg, INFRQ_DESC);
	}
	return pkg->sha1sum;
}

const char SYMEXPORT *alpm_pkg_get_arch(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->data, pkg, INFRQ_DESC);
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
		_alpm_db_read(pkg->data, pkg, INFRQ_DESC);
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
		_alpm_db_read(pkg->data, pkg, INFRQ_DESC);
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
		_alpm_db_read(pkg->data, pkg, INFRQ_DESC);
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
		_alpm_db_read(pkg->data, pkg, INFRQ_DESC);
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
		_alpm_db_read(pkg->data, pkg, INFRQ_DESC);
	}
	return pkg->groups;
}

/* depends */
alpm_list_t SYMEXPORT *alpm_pkg_get_depends(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DEPENDS)) {
		_alpm_db_read(pkg->data, pkg, INFRQ_DEPENDS);
	}
	return pkg->depends;
}

alpm_list_t SYMEXPORT *alpm_pkg_get_requiredby(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DEPENDS)) {
		_alpm_db_read(pkg->data, pkg, INFRQ_DEPENDS);
	}
	return pkg->requiredby;
}

alpm_list_t SYMEXPORT *alpm_pkg_get_conflicts(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DEPENDS)) {
		_alpm_db_read(pkg->data, pkg, INFRQ_DEPENDS);
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
		_alpm_db_read(pkg->data, pkg, INFRQ_DEPENDS);
	}
	return pkg->provides;
}

alpm_list_t SYMEXPORT *alpm_pkg_get_replaces(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->data, pkg, INFRQ_DESC);
	}
	return pkg->replaces;
}

alpm_list_t SYMEXPORT *alpm_pkg_get_files(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && pkg->data == handle->db_local
		 && !(pkg->infolevel & INFRQ_FILES)) {
		_alpm_db_read(pkg->data, pkg, INFRQ_FILES);
	}
	return pkg->files;
}

alpm_list_t SYMEXPORT *alpm_pkg_get_backup(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && pkg->data == handle->db_local
		 && !(pkg->infolevel & INFRQ_FILES)) {
		_alpm_db_read(pkg->data, pkg, INFRQ_FILES);
	}
	return pkg->backup;
}

unsigned short SYMEXPORT alpm_pkg_has_scriptlet(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(-1));
	ASSERT(pkg != NULL, return(-1));

	if(pkg->origin == PKG_FROM_CACHE && pkg->data == handle->db_local
		 && !(pkg->infolevel & INFRQ_SCRIPTLET)) {
		_alpm_db_read(pkg->data, pkg, INFRQ_SCRIPTLET);
	}
	return pkg->scriptlet;
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

	if((pkg = calloc(1,sizeof(pmpkg_t))) == NULL) {
		RET_ERR(PM_ERR_MEMORY, NULL);
	}

	if(name && name[0] != 0) {
		strncpy(pkg->name, name, PKG_NAME_LEN);
	} else {
		pkg->name[0]        = '\0';
	}
	if(version && version[0] != 0) {
		strncpy(pkg->version, version, PKG_VERSION_LEN);
	} else {
		pkg->version[0]     = '\0';
	}

	return(pkg);
}

pmpkg_t *_alpm_pkg_dup(pmpkg_t *pkg)
{
	pmpkg_t* newpkg;

	ALPM_LOG_FUNC;

	if((newpkg = calloc(1, sizeof(pmpkg_t))) == NULL) {
		_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(pmpkg_t));
		RET_ERR(PM_ERR_MEMORY, NULL);
	}

	memcpy(newpkg, pkg, sizeof(pmpkg_t));
	newpkg->licenses    = alpm_list_strdup(alpm_pkg_get_licenses(pkg));
	newpkg->requiredby = alpm_list_strdup(alpm_pkg_get_requiredby(pkg));
	newpkg->conflicts  = alpm_list_strdup(alpm_pkg_get_conflicts(pkg));
	newpkg->files      = alpm_list_strdup(alpm_pkg_get_files(pkg));
	newpkg->backup     = alpm_list_strdup(alpm_pkg_get_backup(pkg));
	newpkg->depends    = alpm_list_strdup(alpm_pkg_get_depends(pkg));
	newpkg->groups     = alpm_list_strdup(alpm_pkg_get_groups(pkg));
	newpkg->provides   = alpm_list_strdup(alpm_pkg_get_provides(pkg));
	newpkg->replaces   = alpm_list_strdup(alpm_pkg_get_replaces(pkg));
	/* internal */
	newpkg->data = (newpkg->origin == PKG_FROM_FILE) ? strdup(pkg->data) : pkg->data;

	return(newpkg);
}

void _alpm_pkg_free(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	if(pkg == NULL) {
		return;
	}

	FREELIST(pkg->licenses);
	FREELIST(pkg->files);
	FREELIST(pkg->backup);
	FREELIST(pkg->depends);
	FREELIST(pkg->conflicts);
	FREELIST(pkg->requiredby);
	FREELIST(pkg->groups);
	FREELIST(pkg->provides);
	FREELIST(pkg->replaces);
	if(pkg->origin == PKG_FROM_FILE) {
		FREE(pkg->data);
	}
	FREE(pkg);
}

/* Is pkgB an upgrade for pkgA ? */
int alpm_pkg_compare_versions(pmpkg_t *local_pkg, pmpkg_t *pkg)
{
	int cmp = 0;

	ALPM_LOG_FUNC;

	if(pkg->origin == PKG_FROM_CACHE) {
		/* ensure we have the /desc file, which contains the 'force' option */
		_alpm_db_read(pkg->data, pkg, INFRQ_DESC);
	}

	/* compare versions and see if we need to upgrade */
	cmp = _alpm_versioncmp(alpm_pkg_get_version(pkg), alpm_pkg_get_version(local_pkg));

	if(alpm_list_find_str(handle->ignorepkg, alpm_pkg_get_name(pkg))) {
		/* package should be ignored (IgnorePkg) */
		if(cmp > 0) {
			_alpm_log(PM_LOG_WARNING, _("%s-%s: ignoring package upgrade (%s)"),
								alpm_pkg_get_name(local_pkg), alpm_pkg_get_version(local_pkg),
								alpm_pkg_get_version(pkg));
		}
		return(0);
	}

	if(cmp != 0 && pkg->force) {
		cmp = 1;
		_alpm_log(PM_LOG_WARNING, _("%s: forcing upgrade to version %s"),
							alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
	} else if(cmp < 0) {
		/* local version is newer */
		pmdb_t *db = pkg->data;
		_alpm_log(PM_LOG_WARNING, _("%s: local (%s) is newer than %s (%s)"),
							alpm_pkg_get_name(local_pkg), alpm_pkg_get_version(local_pkg),
							alpm_db_get_name(db), alpm_pkg_get_version(pkg));
		cmp = 0;
	} else if(cmp > 0) {
		/* we have an upgrade, make sure we should actually do it */
		if(_alpm_pkg_istoonew(pkg)) {
			/* package too new (UpgradeDelay) */
			_alpm_log(PM_LOG_WARNING, _("%s-%s: delaying upgrade of package (%s)"),
								alpm_pkg_get_name(local_pkg), alpm_pkg_get_version(local_pkg),
								alpm_pkg_get_version(pkg));
			cmp = 0;
		}
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
		_alpm_log(PM_LOG_ERROR, _("could not open file %s: %s"), descfile, strerror(errno));
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
			_alpm_log(PM_LOG_DEBUG, "%s: syntax error in description file line %d",
								info->name[0] != '\0' ? info->name : "error", linenum);
		} else {
			_alpm_strtrim(key);
			key = _alpm_strtoupper(key);
			_alpm_strtrim(ptr);
			if(!strcmp(key, "PKGNAME")) {
				strncpy(info->name, ptr, sizeof(info->name));
			} else if(!strcmp(key, "PKGVER")) {
				strncpy(info->version, ptr, sizeof(info->version));
			} else if(!strcmp(key, "PKGDESC")) {
				strncpy(info->desc, ptr, sizeof(info->desc));
			} else if(!strcmp(key, "GROUP")) {
				info->groups = alpm_list_add(info->groups, strdup(ptr));
			} else if(!strcmp(key, "URL")) {
				strncpy(info->url, ptr, sizeof(info->url));
			} else if(!strcmp(key, "LICENSE")) {
				info->licenses = alpm_list_add(info->licenses, strdup(ptr));
			} else if(!strcmp(key, "BUILDDATE")) {
				strncpy(info->builddate, ptr, sizeof(info->builddate));
			} else if(!strcmp(key, "INSTALLDATE")) {
				strncpy(info->installdate, ptr, sizeof(info->installdate));
			} else if(!strcmp(key, "PACKAGER")) {
				strncpy(info->packager, ptr, sizeof(info->packager));
			} else if(!strcmp(key, "ARCH")) {
				strncpy(info->arch, ptr, sizeof(info->arch));
			} else if(!strcmp(key, "SIZE")) {
				/* size in the raw package is uncompressed (installed) size */
				info->isize = atol(ptr);
			} else if(!strcmp(key, "DEPEND")) {
				info->depends = alpm_list_add(info->depends, strdup(ptr));
			} else if(!strcmp(key, "CONFLICT")) {
				info->conflicts = alpm_list_add(info->conflicts, strdup(ptr));
			} else if(!strcmp(key, "REPLACES")) {
				info->replaces = alpm_list_add(info->replaces, strdup(ptr));
			} else if(!strcmp(key, "PROVIDES")) {
				info->provides = alpm_list_add(info->provides, strdup(ptr));
			} else if(!strcmp(key, "BACKUP")) {
				info->backup = alpm_list_add(info->backup, strdup(ptr));
			} else {
				_alpm_log(PM_LOG_DEBUG, "%s: syntax error in description file line %d",
									info->name[0] != '\0' ? info->name : "error", linenum);
			}
		}
		line[0] = '\0';
	}
	fclose(fp);
	unlink(descfile);

	return(0);
}

pmpkg_t *_alpm_pkg_load(const char *pkgfile)
{
	char *expath;
	int ret = ARCHIVE_OK;
	int config = 0;
	int filelist = 0;
	int scriptcheck = 0;
	struct archive *archive;
	struct archive_entry *entry;
	pmpkg_t *info = NULL;
	char *descfile = NULL;
	int fd = -1;
	alpm_list_t *all_files = NULL;
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

	if (archive_read_open_file(archive, pkgfile, ARCHIVE_DEFAULT_BYTES_PER_BLOCK) != ARCHIVE_OK) {
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

	/* Read through the entire archive for metadata.  We will continue reading
	 * even if all metadata is found, to verify the integrity of the archive in
	 * full */
	while((ret = archive_read_next_header (archive, &entry)) == ARCHIVE_OK) {
		const char *entry_name = archive_entry_pathname(entry);

		if(strcmp(entry_name, ".PKGINFO") == 0) {
			/* extract this file into /tmp. it has info for us */
			descfile = strdup("/tmp/alpm_XXXXXX");
			fd = mkstemp(descfile);
			archive_read_data_into_fd (archive, fd);
			/* parse the info file */
			if(parse_descfile(descfile, info) == -1) {
				_alpm_log(PM_LOG_ERROR, _("could not parse the package description file"));
				goto pkg_invalid;
			}
			if(!strlen(info->name)) {
				_alpm_log(PM_LOG_ERROR, _("missing package name in %s"), pkgfile);
				goto pkg_invalid;
			}
			if(!strlen(info->version)) {
				_alpm_log(PM_LOG_ERROR, _("missing package version in %s"), pkgfile);
				goto pkg_invalid;
			}
			config = 1;
			unlink(descfile);
			FREE(descfile);
			close(fd);
			continue;
		} else if(strcmp(entry_name,  ".INSTALL") == 0) {
			info->scriptlet = 1;
			scriptcheck = 1;
		} else if(strcmp(entry_name, ".FILELIST") == 0) {
			/* Build info->files from the filelist */
			FILE *fp;
			char *fn;
			char *str;
			int fd;
			
			if((str = malloc(PATH_MAX)) == NULL) {
				RET_ERR(PM_ERR_MEMORY, (pmpkg_t *)-1);
			}
			fn = strdup("/tmp/alpm_XXXXXX");
			fd = mkstemp(fn);
			archive_read_data_into_fd(archive,fd);
			fp = fopen(fn, "r");
			while(!feof(fp)) {
				if(fgets(str, PATH_MAX, fp) == NULL) {
					continue;
				}
				_alpm_strtrim(str);
				info->files = alpm_list_add(info->files, strdup(str));
			}
			FREE(str);
			fclose(fp);
			if(unlink(fn)) {
				_alpm_log(PM_LOG_WARNING, _("could not remove tempfile %s"), fn);
			}
			FREE(fn);
			close(fd);
			filelist = 1;
			continue;
		} else if(*entry_name == '.') {
					/* for now, ignore all files starting with '.' that haven't
					 * already been handled (for future possibilities) */
		} else {
			scriptcheck = 1;
			/* Keep track of all files so we can generate a filelist later if missing */
			all_files = alpm_list_add(all_files, strdup(entry_name));
		}

		if(archive_read_data_skip(archive)) {
			_alpm_log(PM_LOG_ERROR, _("error while reading package: %s"), archive_error_string(archive));
			pm_errno = PM_ERR_LIBARCHIVE_ERROR;
			goto error;
		}
		expath = NULL;
	}
	if(ret != ARCHIVE_EOF) { /* An error occured */
		_alpm_log(PM_LOG_ERROR, _("error while reading package: %s"), archive_error_string(archive));
		pm_errno = PM_ERR_LIBARCHIVE_ERROR;
		goto error;
	}

	if(!config) {
		_alpm_log(PM_LOG_ERROR, _("missing package metadata"), pkgfile);
		goto error;
	}
	
  archive_read_finish(archive);

	if(!filelist) {
		_alpm_log(PM_LOG_ERROR, _("missing package filelist in %s, generating one"), pkgfile);
		info->files = all_files;
	} else {
		alpm_list_free_inner(all_files, free);
		alpm_list_free(all_files);
	}

	/* this is IMPORTANT - "checking for conflicts" requires a sorted list, so we
	 * ensure that here */
	info->files = alpm_list_msort(info->files, alpm_list_count(info->files), _alpm_str_cmp);

	/* internal */
	info->origin = PKG_FROM_FILE;
	info->data = strdup(pkgfile);
	info->infolevel = 0xFF;

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

int _alpm_pkg_splitname(const char *target, char *name, char *version, int witharch)
{
	char tmp[PKG_FULLNAME_LEN+7];
	const char *t;
	char *p, *q;

	ALPM_LOG_FUNC;

	if(target == NULL) {
		return(-1);
	}

	/* trim path name (if any) */
	if((t = strrchr(target, '/')) == NULL) {
		t = target;
	} else {
		t++;
	}
	strncpy(tmp, t, PKG_FULLNAME_LEN+7);
	/* trim file extension (if any) */
	if((p = strstr(tmp, PKGEXT))) {
		*p = '\0';
	}

	if(witharch) {
		/* trim architecture */
		if((p = alpm_pkg_name_hasarch(tmp))) {
			*p = '\0';
		}
	}

	p = tmp + strlen(tmp);

	for(q = --p; *q && *q != '-'; q--);
	if(*q != '-' || q == tmp) {
		return(-1);
	}
	for(p = --q; *p && *p != '-'; p--);
	if(*p != '-' || p == tmp) {
		return(-1);
	}
	if(version) {
		strncpy(version, p+1, PKG_VERSION_LEN);
	}
	*p = '\0';

	if(name) {
		strncpy(name, tmp, PKG_NAME_LEN);
	}

	return(0);
}

/* scan the local db to fill in requiredby field of package,
 * used when we want to install or add a package */
void _alpm_pkg_update_requiredby(pmpkg_t *pkg)
{
	const alpm_list_t *i, *j;

	pmdb_t *localdb = alpm_option_get_localdb();
	for(i = _alpm_db_get_pkgcache(localdb); i; i = i->next) {
		if(!i->data) {
			continue;
		}
		pmpkg_t *cachepkg = i->data;
		const char *cachepkgname = alpm_pkg_get_name(cachepkg);

		for(j = alpm_pkg_get_depends(cachepkg); j; j = j->next) {
			pmdepend_t *dep;
			int satisfies;

			if(!j->data) {
				continue;
			}
			dep = alpm_splitdep(j->data);
			if(dep == NULL) {
					continue;
			}
			
			satisfies = alpm_depcmp(pkg, dep);
			free(dep);
			if(satisfies) {
				alpm_list_t *reqs = alpm_pkg_get_requiredby(pkg);
				_alpm_log(PM_LOG_DEBUG, "adding '%s' in requiredby field for '%s'",
				          cachepkgname, pkg->name);
				reqs = alpm_list_add(reqs, strdup(cachepkgname));
				pkg->requiredby = reqs;
				break;
			}
		}
	}
}

/* TODO this should either be public, or done somewhere else */
int _alpm_pkg_istoonew(pmpkg_t *pkg)
{
	time_t t;

	ALPM_LOG_FUNC;

	if (!handle->upgradedelay)
		return 0;
	time(&t);
	return((pkg->date + handle->upgradedelay) > t);
}
/* vim: set ts=2 sw=2 noet: */
