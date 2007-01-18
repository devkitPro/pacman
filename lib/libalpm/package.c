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
#include <libintl.h>
#include <locale.h>
/* pacman */
#include "log.h"
#include "util.h"
#include "error.h"
#include "list.h"
#include "package.h"
#include "db.h"
#include "handle.h"
#include "alpm.h"

pmpkg_t *_alpm_pkg_new(const char *name, const char *version)
{
	pmpkg_t* pkg = NULL;

	if((pkg = (pmpkg_t *)malloc(sizeof(pmpkg_t))) == NULL) {
		RET_ERR(PM_ERR_MEMORY, (pmpkg_t *)-1);
	}

	if(name && name[0] != 0) {
		STRNCPY(pkg->name, name, PKG_NAME_LEN);
	} else {
		pkg->name[0]        = '\0';
	}
	if(version && version[0] != 0) {
		STRNCPY(pkg->version, version, PKG_VERSION_LEN);
	} else {
		pkg->version[0]     = '\0';
	}
	pkg->filename[0]    = '\0';
	pkg->desc[0]        = '\0';
	pkg->url[0]         = '\0';
	pkg->license        = NULL;
	pkg->desc_localized = NULL;
	pkg->builddate[0]   = '\0';
	pkg->buildtype[0]   = '\0';
	pkg->installdate[0] = '\0';
	pkg->packager[0]    = '\0';
	pkg->md5sum[0]      = '\0';
	pkg->sha1sum[0]     = '\0';
	pkg->arch[0]        = '\0';
	pkg->size           = 0;
	pkg->isize          = 0;
	pkg->scriptlet      = 0;
	pkg->force          = 0;
	pkg->reason         = PM_PKG_REASON_EXPLICIT;
	pkg->requiredby     = NULL;
	pkg->conflicts      = NULL;
	pkg->files          = NULL;
	pkg->backup         = NULL;
	pkg->depends        = NULL;
	pkg->removes        = NULL;
	pkg->groups         = NULL;
	pkg->provides       = NULL;
	pkg->replaces       = NULL;
	/* internal */
	pkg->origin         = 0;
	pkg->data           = NULL;
	pkg->infolevel      = 0;

	return(pkg);
}

pmpkg_t *_alpm_pkg_dup(pmpkg_t *pkg)
{
	pmpkg_t* newpkg = NULL;

	newpkg = (pmpkg_t *)malloc(sizeof(pmpkg_t));
	if(newpkg == NULL) {
		_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(pmpkg_t));
		RET_ERR(PM_ERR_MEMORY, NULL);
	}

	STRNCPY(newpkg->filename, pkg->filename, PKG_FILENAME_LEN);
	STRNCPY(newpkg->name, pkg->name, PKG_NAME_LEN);
	STRNCPY(newpkg->version, pkg->version, PKG_VERSION_LEN);
	STRNCPY(newpkg->desc, pkg->desc, PKG_DESC_LEN);
	STRNCPY(newpkg->url, pkg->url, PKG_URL_LEN);
	STRNCPY(newpkg->builddate, pkg->builddate, PKG_DATE_LEN);
	STRNCPY(newpkg->buildtype, pkg->buildtype, PKG_DATE_LEN);
	STRNCPY(newpkg->installdate, pkg->installdate, PKG_DATE_LEN);
	STRNCPY(newpkg->packager, pkg->packager, PKG_PACKAGER_LEN);
	STRNCPY(newpkg->md5sum, pkg->md5sum, PKG_MD5SUM_LEN);
	STRNCPY(newpkg->sha1sum, pkg->sha1sum, PKG_SHA1SUM_LEN);
	STRNCPY(newpkg->arch, pkg->arch, PKG_ARCH_LEN);
	newpkg->size       = pkg->size;
	newpkg->isize      = pkg->isize;
	newpkg->force      = pkg->force;
	newpkg->scriptlet  = pkg->scriptlet;
	newpkg->reason     = pkg->reason;
	newpkg->license    = _alpm_list_strdup(pkg->license);
	newpkg->desc_localized = _alpm_list_strdup(pkg->desc_localized);
	newpkg->requiredby = _alpm_list_strdup(pkg->requiredby);
	newpkg->conflicts  = _alpm_list_strdup(pkg->conflicts);
	newpkg->files      = _alpm_list_strdup(pkg->files);
	newpkg->backup     = _alpm_list_strdup(pkg->backup);
	newpkg->depends    = _alpm_list_strdup(pkg->depends);
	newpkg->removes    = _alpm_list_strdup(pkg->removes);
	newpkg->groups     = _alpm_list_strdup(pkg->groups);
	newpkg->provides   = _alpm_list_strdup(pkg->provides);
	newpkg->replaces   = _alpm_list_strdup(pkg->replaces);
	/* internal */
	newpkg->origin     = pkg->origin;
	newpkg->data = (newpkg->origin == PKG_FROM_FILE) ? strdup(pkg->data) : pkg->data;
	newpkg->infolevel  = pkg->infolevel;

	return(newpkg);
}

void _alpm_pkg_free(void *data)
{
	pmpkg_t *pkg = data;

	if(pkg == NULL) {
		return;
	}

	FREELIST(pkg->license);
	FREELIST(pkg->desc_localized);
	FREELIST(pkg->files);
	FREELIST(pkg->backup);
	FREELIST(pkg->depends);
	FREELIST(pkg->removes);
	FREELIST(pkg->conflicts);
	FREELIST(pkg->requiredby);
	FREELIST(pkg->groups);
	FREELIST(pkg->provides);
	FREELIST(pkg->replaces);
	if(pkg->origin == PKG_FROM_FILE) {
		FREE(pkg->data);
	}
	FREE(pkg);

	return;
}

/* Helper function for comparing packages
 */
int _alpm_pkg_cmp(const void *p1, const void *p2)
{
	return(strcmp(((pmpkg_t *)p1)->name, ((pmpkg_t *)p2)->name));
}

/* Parses the package description file for the current package
 *
 * Returns: 0 on success, 1 on error
 *
 */
static int parse_descfile(char *descfile, pmpkg_t *info, int output)
{
	FILE* fp = NULL;
	char line[PATH_MAX];
	char* ptr = NULL;
	char* key = NULL;
	int linenum = 0;

	if((fp = fopen(descfile, "r")) == NULL) {
		_alpm_log(PM_LOG_ERROR, _("could not open file %s"), descfile);
		return(-1);
	}

	while(!feof(fp)) {
		fgets(line, PATH_MAX, fp);
		linenum++;
		_alpm_strtrim(line);
		if(strlen(line) == 0 || line[0] == '#') {
			continue;
		}
		if(output) {
			_alpm_log(PM_LOG_DEBUG, "%s", line);
		}
		ptr = line;
		key = strsep(&ptr, "=");
		if(key == NULL || ptr == NULL) {
			_alpm_log(PM_LOG_DEBUG, _("%s: syntax error in description file line %d"),
				info->name[0] != '\0' ? info->name : "error", linenum);
		} else {
			_alpm_strtrim(key);
			key = _alpm_strtoupper(key);
			_alpm_strtrim(ptr);
			if(!strcmp(key, "PKGNAME")) {
				STRNCPY(info->name, ptr, sizeof(info->name));
			} else if(!strcmp(key, "PKGVER")) {
				STRNCPY(info->version, ptr, sizeof(info->version));
			} else if(!strcmp(key, "PKGDESC")) {
				char *lang_tmp;
				info->desc_localized = _alpm_list_add(info->desc_localized, strdup(ptr));
				if((lang_tmp = (char *)malloc(strlen(setlocale(LC_ALL, "")))) == NULL) {
					RET_ERR(PM_ERR_MEMORY, -1);
				}
				STRNCPY(lang_tmp, setlocale(LC_ALL, ""), strlen(setlocale(LC_ALL, "")));
				if(info->desc_localized && !info->desc_localized->next) {
					STRNCPY(info->desc, ptr, sizeof(info->desc));
				} else if (ptr && !strncmp(ptr, lang_tmp, strlen(lang_tmp))) {
					STRNCPY(info->desc, ptr+strlen(lang_tmp)+1, sizeof(info->desc));
				}
				FREE(lang_tmp);
			} else if(!strcmp(key, "GROUP")) {
				info->groups = _alpm_list_add(info->groups, strdup(ptr));
			} else if(!strcmp(key, "URL")) {
				STRNCPY(info->url, ptr, sizeof(info->url));
			} else if(!strcmp(key, "LICENSE")) {
				info->license = _alpm_list_add(info->license, strdup(ptr));
			} else if(!strcmp(key, "BUILDDATE")) {
				STRNCPY(info->builddate, ptr, sizeof(info->builddate));
			} else if(!strcmp(key, "BUILDTYPE")) {
				STRNCPY(info->buildtype, ptr, sizeof(info->buildtype));
			} else if(!strcmp(key, "INSTALLDATE")) {
				STRNCPY(info->installdate, ptr, sizeof(info->installdate));
			} else if(!strcmp(key, "PACKAGER")) {
				STRNCPY(info->packager, ptr, sizeof(info->packager));
			} else if(!strcmp(key, "ARCH")) {
				STRNCPY(info->arch, ptr, sizeof(info->arch));
			} else if(!strcmp(key, "SIZE")) {
				char tmp[32];
				STRNCPY(tmp, ptr, sizeof(tmp));
				info->size = atol(ptr);
			} else if(!strcmp(key, "ISIZE")) {
				char tmp[32];
				STRNCPY(tmp, ptr, sizeof(tmp));
				info->isize = atol(ptr);
			} else if(!strcmp(key, "DEPEND")) {
				info->depends = _alpm_list_add(info->depends, strdup(ptr));
			} else if(!strcmp(key, "REMOVE")) {
				info->removes = _alpm_list_add(info->removes, strdup(ptr));
			} else if(!strcmp(key, "CONFLICT")) {
				info->conflicts = _alpm_list_add(info->conflicts, strdup(ptr));
			} else if(!strcmp(key, "REPLACES")) {
				info->replaces = _alpm_list_add(info->replaces, strdup(ptr));
			} else if(!strcmp(key, "PROVIDES")) {
				info->provides = _alpm_list_add(info->provides, strdup(ptr));
			} else if(!strcmp(key, "BACKUP")) {
				info->backup = _alpm_list_add(info->backup, strdup(ptr));
			} else {
				_alpm_log(PM_LOG_DEBUG, _("%s: syntax error in description file line %d"),
					info->name[0] != '\0' ? info->name : "error", linenum);
			}
		}
		line[0] = '\0';
	}
	fclose(fp);
	unlink(descfile);

	return(0);
}

pmpkg_t *_alpm_pkg_load(char *pkgfile)
{
	char *expath;
	int i;
	int config = 0;
	int filelist = 0;
	int scriptcheck = 0;
	register struct archive *archive;
	struct archive_entry *entry;
	pmpkg_t *info = NULL;

	if(pkgfile == NULL || strlen(pkgfile) == 0) {
		RET_ERR(PM_ERR_WRONG_ARGS, NULL);
	}

	if ((archive = archive_read_new ()) == NULL)
		RET_ERR(PM_ERR_LIBARCHIVE_ERROR, NULL);

	archive_read_support_compression_all (archive);
	archive_read_support_format_all (archive);

	if (archive_read_open_file (archive, pkgfile, ARCHIVE_DEFAULT_BYTES_PER_BLOCK) != ARCHIVE_OK)
		RET_ERR(PM_ERR_PKG_OPEN, NULL);

	info = _alpm_pkg_new(NULL, NULL);
	if(info == NULL) {
		archive_read_finish (archive);
		RET_ERR(PM_ERR_MEMORY, NULL);
	}

	for(i = 0; archive_read_next_header (archive, &entry) == ARCHIVE_OK; i++) {
		if(config && filelist && scriptcheck) {
			/* we have everything we need */
			break;
		}
		if(!strcmp(archive_entry_pathname (entry), ".PKGINFO")) {
			char *descfile;
			int fd;

			/* extract this file into /tmp. it has info for us */
			descfile = strdup("/tmp/alpm_XXXXXX");
			fd = mkstemp(descfile);
			archive_read_data_into_fd (archive, fd);
			/* parse the info file */
			if(parse_descfile(descfile, info, 0) == -1) {
				_alpm_log(PM_LOG_ERROR, _("could not parse the package description file"));
				pm_errno = PM_ERR_PKG_INVALID;
				unlink(descfile);
				FREE(descfile);
				close(fd);
				goto error;
			}
			if(!strlen(info->name)) {
				_alpm_log(PM_LOG_ERROR, _("missing package name in %s"), pkgfile);
				pm_errno = PM_ERR_PKG_INVALID;
				unlink(descfile);
				FREE(descfile);
				close(fd);
				goto error;
			}
			if(!strlen(info->version)) {
				_alpm_log(PM_LOG_ERROR, _("missing package version in %s"), pkgfile);
				pm_errno = PM_ERR_PKG_INVALID;
				unlink(descfile);
				FREE(descfile);
				close(fd);
				goto error;
			}
			config = 1;
			unlink(descfile);
			FREE(descfile);
			close(fd);
			continue;
		} else if(!strcmp(archive_entry_pathname (entry), "._install") || !strcmp(archive_entry_pathname (entry),  ".INSTALL")) {
			info->scriptlet = 1;
			scriptcheck = 1;
		} else if(!strcmp(archive_entry_pathname (entry), ".FILELIST")) {
			/* Build info->files from the filelist */
			FILE *fp;
			char *fn;
			char *str;
			int fd;
			
			if((str = (char *)malloc(PATH_MAX)) == NULL) {
				RET_ERR(PM_ERR_MEMORY, (pmpkg_t *)-1);
			}
			fn = strdup("/tmp/alpm_XXXXXX");
			fd = mkstemp(fn);
			archive_read_data_into_fd (archive,fd);
			fp = fopen(fn, "r");
			while(!feof(fp)) {
				if(fgets(str, PATH_MAX, fp) == NULL) {
					continue;
				}
				_alpm_strtrim(str);
				info->files = _alpm_list_add(info->files, strdup(str));
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
		} else {
			scriptcheck = 1;
			if(!filelist) {
				/* no .FILELIST present in this package..  build the filelist the */
				/* old-fashioned way, one at a time */
				expath = strdup(archive_entry_pathname (entry));
				info->files = _alpm_list_add(info->files, expath);
			}
		}

		if(archive_read_data_skip (archive)) {
			_alpm_log(PM_LOG_ERROR, _("bad package file in %s"), pkgfile);
			goto error;
		}
		expath = NULL;
	}
	archive_read_finish (archive);

	if(!config) {
		_alpm_log(PM_LOG_ERROR, _("missing package info file in %s"), pkgfile);
		goto error;
	}

	/* internal */
	info->origin = PKG_FROM_FILE;
	info->data = strdup(pkgfile);
	info->infolevel = 0xFF;

	return(info);

error:
	FREEPKG(info);
	archive_read_finish (archive);

	return(NULL);
}

/* Test for existence of a package in a pmlist_t*
 * of pmpkg_t*
 */
pmpkg_t *_alpm_pkg_isin(char *needle, pmlist_t *haystack)
{
	pmlist_t *lp;

	if(needle == NULL || haystack == NULL) {
		return(NULL);
	}

	for(lp = haystack; lp; lp = lp->next) {
		pmpkg_t *info = lp->data;

		if(info && !strcmp(info->name, needle)) {
			return(lp->data);
		}
	}
	return(NULL);
}

int _alpm_pkg_splitname(char *target, char *name, char *version, int witharch)
{
	char tmp[PKG_FULLNAME_LEN+7];
	char *p, *q;

	if(target == NULL) {
		return(-1);
	}

	/* trim path name (if any) */
	if((p = strrchr(target, '/')) == NULL) {
		p = target;
	} else {
		p++;
	}
	STRNCPY(tmp, p, PKG_FULLNAME_LEN+7);
	/* trim file extension (if any) */
	if((p = strstr(tmp, PM_EXT_PKG))) {
		*p = '\0';
	}

	if(witharch) {
		/* trim architecture */
		if((p = alpm_pkg_name_hasarch(tmp))) {
			*p = 0;
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
		STRNCPY(version, p+1, PKG_VERSION_LEN);
	}
	*p = '\0';

	if(name) {
		STRNCPY(name, tmp, PKG_NAME_LEN);
	}

	return(0);
}

const char *alpm_pkg_get_filename(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(!strlen(pkg->filename)) {
		/* construct the file name, it's not in the desc file */
		if(pkg->arch && strlen(pkg->arch) > 0) {
			snprintf(pkg->filename, PKG_FILENAME_LEN, "%s-%s-%s" PM_EXT_PKG, pkg->name, pkg->version, pkg->arch);
		} else {
			snprintf(pkg->filename, PKG_FILENAME_LEN, "%s-%s" PM_EXT_PKG, pkg->name, pkg->version);
		}
	}

	return pkg->filename;
}

const char *alpm_pkg_get_name(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	return pkg->name;
}

const char *alpm_pkg_get_version(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	return pkg->version;
}

const char *alpm_pkg_get_desc(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->data, INFRQ_DESC, pkg);
	}
	return pkg->desc;
}

const char *alpm_pkg_get_url(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->data, INFRQ_DESC, pkg);
	}
	return pkg->url;
}

const char *alpm_pkg_get_builddate(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->data, INFRQ_DESC, pkg);
	}
	return pkg->builddate;
}

const char *alpm_pkg_get_buildtype(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->data, INFRQ_DESC, pkg);
	}
	return pkg->buildtype;
}

const char *alpm_pkg_get_installdate(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->data, INFRQ_DESC, pkg);
	}
	return pkg->installdate;
}

const char *alpm_pkg_get_packager(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->data, INFRQ_DESC, pkg);
	}
	return pkg->packager;
}

const char *alpm_pkg_get_md5sum(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->data, INFRQ_DESC, pkg);
	}
	return pkg->md5sum;
}

const char *alpm_pkg_get_sha1sum(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->data, INFRQ_DESC, pkg);
	}
	return pkg->sha1sum;
}

const char *alpm_pkg_get_arch(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->data, INFRQ_DESC, pkg);
	}
	return pkg->arch;
}

unsigned long alpm_pkg_get_size(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(-1));
	ASSERT(pkg != NULL, return(-1));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->data, INFRQ_DESC, pkg);
	}
	return pkg->size;
}

unsigned long alpm_pkg_get_isize(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(-1));
	ASSERT(pkg != NULL, return(-1));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->data, INFRQ_DESC, pkg);
	}
	return pkg->isize;
}

unsigned char alpm_pkg_get_reason(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(-1));
	ASSERT(pkg != NULL, return(-1));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->data, INFRQ_DESC, pkg);
	}
	return pkg->reason;
}

pmlist_t *alpm_pkg_get_licenses(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->data, INFRQ_DESC, pkg);
	}
	return pkg->license;
}

pmlist_t *alpm_pkg_get_groups(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->data, INFRQ_DESC, pkg);
	}
	return pkg->groups;
}

/* depends */
pmlist_t *alpm_pkg_get_depends(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DEPENDS)) {
		_alpm_db_read(pkg->data, INFRQ_DEPENDS, pkg);
	}
	return pkg->depends;
}

pmlist_t *alpm_pkg_get_removes(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DEPENDS)) {
		_alpm_db_read(pkg->data, INFRQ_DEPENDS, pkg);
	}
	return pkg->removes;
}

pmlist_t *alpm_pkg_get_requiredby(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DEPENDS)) {
		_alpm_db_read(pkg->data, INFRQ_DEPENDS, pkg);
	}
	return pkg->requiredby;
}

pmlist_t *alpm_pkg_get_conflicts(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DEPENDS)) {
		_alpm_db_read(pkg->data, INFRQ_DEPENDS, pkg);
	}
	return pkg->conflicts;
}

pmlist_t *alpm_pkg_get_provides(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DEPENDS)) {
		_alpm_db_read(pkg->data, INFRQ_DEPENDS, pkg);
	}
	return pkg->provides;
}

pmlist_t *alpm_pkg_get_replaces(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DEPENDS)) {
		_alpm_db_read(pkg->data, INFRQ_DEPENDS, pkg);
	}
	return pkg->replaces;
}

pmlist_t *alpm_pkg_get_files(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && pkg->data == handle->db_local
		 && !(pkg->infolevel & INFRQ_FILES)) {
		_alpm_db_read(pkg->data, INFRQ_FILES, pkg);
	}
	return pkg->files;
}

pmlist_t *alpm_pkg_get_backup(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && pkg->data == handle->db_local
		 && !(pkg->infolevel & INFRQ_FILES)) {
		_alpm_db_read(pkg->data, INFRQ_FILES, pkg);
	}
	return pkg->backup;
}

unsigned char alpm_pkg_has_scriptlet(pmpkg_t *pkg)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(-1));
	ASSERT(pkg != NULL, return(-1));

	if(pkg->origin == PKG_FROM_CACHE && pkg->data == handle->db_local
		 && !(pkg->infolevel & INFRQ_SCRIPTLET)) {
		_alpm_db_read(pkg->data, INFRQ_SCRIPTLET, pkg);
	}
	return pkg->scriptlet;
}

/* vim: set ts=2 sw=2 noet: */
