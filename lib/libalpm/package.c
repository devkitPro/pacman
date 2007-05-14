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
#include <locale.h>
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
#include "versioncmp.h"
#include "alpm.h"

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
	/*newpkg->desc_localized = alpm_list_strdup(pkg->desc_localized);*/
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
  /*FREELIST(pkg->desc_localized);*/
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

	if(alpm_list_find_str(handle->ignorepkg, alpm_pkg_get_name(pkg))) {
		/* package should be ignored (IgnorePkg) */
		_alpm_log(PM_LOG_WARNING, _("%s-%s: ignoring package upgrade (%s)"),
							alpm_pkg_get_name(local_pkg), alpm_pkg_get_version(local_pkg),
							alpm_pkg_get_version(pkg));
		return(0);
	}

	/* compare versions and see if we need to upgrade */
	cmp = _alpm_versioncmp(alpm_pkg_get_version(pkg), alpm_pkg_get_version(local_pkg));

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
			_alpm_log(PM_LOG_DEBUG, _("%s: syntax error in description file line %d"),
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
				/*
				char *lang_tmp;
				info->desc_localized = alpm_list_add(info->desc_localized, strdup(ptr));
				if((lang_tmp = malloc(strlen(setlocale(LC_ALL, "")))) == NULL) {
					RET_ERR(PM_ERR_MEMORY, -1);
				}
				strncpy(lang_tmp, setlocale(LC_ALL, ""), strlen(setlocale(LC_ALL, "")));
				if(info->desc_localized && !info->desc_localized->next) {
				*/
				strncpy(info->desc, ptr, sizeof(info->desc));
				/*
				} else if (ptr && !strncmp(ptr, lang_tmp, strlen(lang_tmp))) {
					strncpy(info->desc, ptr+strlen(lang_tmp)+1, sizeof(info->desc));
				}
				FREE(lang_tmp);
				*/
			} else if(!strcmp(key, "GROUP")) {
				info->groups = alpm_list_add(info->groups, strdup(ptr));
			} else if(!strcmp(key, "URL")) {
				strncpy(info->url, ptr, sizeof(info->url));
			} else if(!strcmp(key, "LICENSE")) {
				info->licenses = alpm_list_add(info->licenses, strdup(ptr));
			} else if(!strcmp(key, "BUILDDATE")) {
				strncpy(info->builddate, ptr, sizeof(info->builddate));
			} else if(!strcmp(key, "BUILDTYPE")) {
				strncpy(info->buildtype, ptr, sizeof(info->buildtype));
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
	if((p = strstr(tmp, PM_EXT_PKG))) {
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


void _alpm_pkg_update_requiredby(pmpkg_t *pkg)
{
	alpm_list_t *i, *j, *k;
	const char *pkgname = alpm_pkg_get_name(pkg);

	pmdb_t *localdb = alpm_option_get_localdb();
	for(i = _alpm_db_get_pkgcache(localdb); i; i = i->next) {
		if(!i->data) {
			continue;
		}
		pmpkg_t *cachepkg = i->data;
		const char *cachepkgname = alpm_pkg_get_name(cachepkg);

		for(j = alpm_pkg_get_depends(cachepkg); j; j = j->next) {
			pmdepend_t *dep;
			if(!j->data) {
				continue;
			}
			dep = alpm_splitdep(j->data);
			if(dep == NULL) {
					continue;
			}
			
			/* check the actual package itself */
			if(strcmp(dep->name, pkgname) == 0) {
				alpm_list_t *reqs = alpm_pkg_get_requiredby(pkg);

				if(!alpm_list_find_str(reqs, cachepkgname)) {
					_alpm_log(PM_LOG_DEBUG, _("adding '%s' in requiredby field for '%s'"),
					          cachepkgname, pkg->name);
					reqs = alpm_list_add(reqs, strdup(cachepkgname));
					pkg->requiredby = reqs;
				}
			}

			/* check for provisions as well */
			for(k = alpm_pkg_get_provides(pkg); k; k = k->next) {
				const char *provname = k->data;
				if(strcmp(dep->name, provname) == 0) {
					alpm_list_t *reqs = alpm_pkg_get_requiredby(pkg);

					if(!alpm_list_find_str(reqs, cachepkgname)) {
						_alpm_log(PM_LOG_DEBUG, _("adding '%s' in requiredby field for '%s' (provides: %s)"),
						          cachepkgname, pkgname, provname);
						reqs = alpm_list_add(reqs, strdup(cachepkgname));
						pkg->requiredby = reqs;
					}
				}
			}
			free(dep);
		}
	}
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
			snprintf(pkg->filename, PKG_FILENAME_LEN, "%s-%s-%s" PM_EXT_PKG,
			         pkg->name, pkg->version, pkg->arch);
		} else {
			snprintf(pkg->filename, PKG_FILENAME_LEN, "%s-%s" PM_EXT_PKG,
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

const char SYMEXPORT *alpm_pkg_get_buildtype(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(pkg != NULL, return(NULL));

	if(pkg->origin == PKG_FROM_CACHE && !(pkg->infolevel & INFRQ_DESC)) {
		_alpm_db_read(pkg->data, pkg, INFRQ_DESC);
	}
	return pkg->buildtype;
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
