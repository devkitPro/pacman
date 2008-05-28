/*
 *  be_package.c
 *
 *  Copyright (c) 2002-2008 by Judd Vinet <jvinet@zeroflux.org>
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
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <locale.h> /* setlocale */

/* libarchive */
#include <archive.h>
#include <archive_entry.h>

/* libalpm */
#include "alpm_list.h"
#include "util.h"
#include "log.h"
#include "package.h"
#include "deps.h" /* _alpm_splitdep */

/**
 * Parses the package description file for a package into a pmpkg_t struct.
 * @param archive the archive to read from, pointed at the .PKGINFO entry
 * @param newpkg an empty pmpkg_t struct to fill with package info
 *
 * @return 0 on success, 1 on error
 */
static int parse_descfile(struct archive *a, pmpkg_t *newpkg)
{
	char line[PATH_MAX];
	char *ptr = NULL;
	char *key = NULL;
	int linenum = 0;

	ALPM_LOG_FUNC;

	/* loop until we reach EOF (where archive_fgets will return NULL) */
	while(_alpm_archive_fgets(line, PATH_MAX, a) != NULL) {
		linenum++;
		_alpm_strtrim(line);
		if(strlen(line) == 0 || line[0] == '#') {
			continue;
		}
		ptr = line;
		key = strsep(&ptr, "=");
		if(key == NULL || ptr == NULL) {
			_alpm_log(PM_LOG_DEBUG, "%s: syntax error in description file line %d\n",
								newpkg->name ? newpkg->name : "error", linenum);
		} else {
			key = _alpm_strtrim(key);
			ptr = _alpm_strtrim(ptr);
			if(!strcmp(key, "pkgname")) {
				STRDUP(newpkg->name, ptr, RET_ERR(PM_ERR_MEMORY, -1));
			} else if(!strcmp(key, "pkgver")) {
				STRDUP(newpkg->version, ptr, RET_ERR(PM_ERR_MEMORY, -1));
			} else if(!strcmp(key, "pkgdesc")) {
				STRDUP(newpkg->desc, ptr, RET_ERR(PM_ERR_MEMORY, -1));
			} else if(!strcmp(key, "group")) {
				newpkg->groups = alpm_list_add(newpkg->groups, strdup(ptr));
			} else if(!strcmp(key, "url")) {
				STRDUP(newpkg->url, ptr, RET_ERR(PM_ERR_MEMORY, -1));
			} else if(!strcmp(key, "license")) {
				newpkg->licenses = alpm_list_add(newpkg->licenses, strdup(ptr));
			} else if(!strcmp(key, "builddate")) {
				char first = tolower(ptr[0]);
				if(first > 'a' && first < 'z') {
					struct tm tmp_tm = {0}; //initialize to null in case of failure
					setlocale(LC_TIME, "C");
					strptime(ptr, "%a %b %e %H:%M:%S %Y", &tmp_tm);
					newpkg->builddate = mktime(&tmp_tm);
					setlocale(LC_TIME, "");
				} else {
					newpkg->builddate = atol(ptr);
				}
			} else if(!strcmp(key, "packager")) {
				STRDUP(newpkg->packager, ptr, RET_ERR(PM_ERR_MEMORY, -1));
			} else if(!strcmp(key, "arch")) {
				STRDUP(newpkg->arch, ptr, RET_ERR(PM_ERR_MEMORY, -1));
			} else if(!strcmp(key, "size")) {
				/* size in the raw package is uncompressed (installed) size */
				newpkg->isize = atol(ptr);
			} else if(!strcmp(key, "depend")) {
				pmdepend_t *dep = _alpm_splitdep(ptr);
				newpkg->depends = alpm_list_add(newpkg->depends, dep);
			} else if(!strcmp(key, "optdepend")) {
				newpkg->optdepends = alpm_list_add(newpkg->optdepends, strdup(ptr));
			} else if(!strcmp(key, "conflict")) {
				newpkg->conflicts = alpm_list_add(newpkg->conflicts, strdup(ptr));
			} else if(!strcmp(key, "replaces")) {
				newpkg->replaces = alpm_list_add(newpkg->replaces, strdup(ptr));
			} else if(!strcmp(key, "provides")) {
				newpkg->provides = alpm_list_add(newpkg->provides, strdup(ptr));
			} else if(!strcmp(key, "backup")) {
				newpkg->backup = alpm_list_add(newpkg->backup, strdup(ptr));
			} else {
				_alpm_log(PM_LOG_DEBUG, "%s: syntax error in description file line %d\n",
									newpkg->name ? newpkg->name : "error", linenum);
			}
		}
		line[0] = '\0';
	}

	return(0);
}

/**
 * Load a package and create the corresponding pmpkg_t struct.
 * @param pkgfile path to the package file
 * @param full whether to stop the load after metadata is read or continue
 *             through the full archive
 * @return An information filled pmpkg_t struct
 */
static pmpkg_t *pkg_load(const char *pkgfile, unsigned short full)
{
	int ret = ARCHIVE_OK;
	int config = 0;
	struct archive *archive;
	struct archive_entry *entry;
	pmpkg_t *newpkg = NULL;
	struct stat st;

	ALPM_LOG_FUNC;

	if(pkgfile == NULL || strlen(pkgfile) == 0) {
		RET_ERR(PM_ERR_WRONG_ARGS, NULL);
	}

	if(stat(pkgfile, &st) != 0) {
		RET_ERR(PM_ERR_PKG_OPEN, NULL);
	}

	if((archive = archive_read_new()) == NULL) {
		RET_ERR(PM_ERR_LIBARCHIVE, NULL);
	}

	archive_read_support_compression_all(archive);
	archive_read_support_format_all(archive);

	if (archive_read_open_filename(archive, pkgfile,
				ARCHIVE_DEFAULT_BYTES_PER_BLOCK) != ARCHIVE_OK) {
		RET_ERR(PM_ERR_PKG_OPEN, NULL);
	}

	newpkg = _alpm_pkg_new();
	if(newpkg == NULL) {
		archive_read_finish(archive);
		RET_ERR(PM_ERR_MEMORY, NULL);
	}

	newpkg->filename = strdup(pkgfile);
	newpkg->size = st.st_size;

	/* If full is false, only read through the archive until we find our needed
	 * metadata. If it is true, read through the entire archive, which serves
	 * as a verfication of integrity and allows us to create the filelist. */
	while((ret = archive_read_next_header(archive, &entry)) == ARCHIVE_OK) {
		const char *entry_name = archive_entry_pathname(entry);

		if(strcmp(entry_name, ".PKGINFO") == 0) {
			/* parse the info file */
			if(parse_descfile(archive, newpkg) != 0) {
				_alpm_log(PM_LOG_ERROR, _("could not parse package description file in %s\n"),
						pkgfile);
				goto pkg_invalid;
			}
			if(newpkg->name == NULL || strlen(newpkg->name) == 0) {
				_alpm_log(PM_LOG_ERROR, _("missing package name in %s\n"), pkgfile);
				goto pkg_invalid;
			}
			if(newpkg->version == NULL || strlen(newpkg->version) == 0) {
				_alpm_log(PM_LOG_ERROR, _("missing package version in %s\n"), pkgfile);
				goto pkg_invalid;
			}
			config = 1;
			continue;
		} else if(strcmp(entry_name,  ".INSTALL") == 0) {
			newpkg->scriptlet = 1;
		} else if(*entry_name == '.') {
			/* for now, ignore all files starting with '.' that haven't
			 * already been handled (for future possibilities) */
		} else {
			/* Keep track of all files for filelist generation */
			newpkg->files = alpm_list_add(newpkg->files, strdup(entry_name));
		}

		if(archive_read_data_skip(archive)) {
			_alpm_log(PM_LOG_ERROR, _("error while reading package %s: %s\n"),
					pkgfile, archive_error_string(archive));
			pm_errno = PM_ERR_LIBARCHIVE;
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
		pm_errno = PM_ERR_LIBARCHIVE;
		goto error;
	}

	if(!config) {
		_alpm_log(PM_LOG_ERROR, _("missing package metadata in %s\n"), pkgfile);
		goto pkg_invalid;
	}

  archive_read_finish(archive);

	/* internal fields for package struct */
	newpkg->origin = PKG_FROM_FILE;
	newpkg->origin_data.file = strdup(pkgfile);

	if(full) {
		/* "checking for conflicts" requires a sorted list, ensure that here */
		_alpm_log(PM_LOG_DEBUG, "sorting package filelist for %s\n", pkgfile);
		newpkg->files = alpm_list_msort(newpkg->files, alpm_list_count(newpkg->files),
				_alpm_str_cmp);
		newpkg->infolevel = INFRQ_ALL;
	} else {
		/* get rid of any partial filelist we may have collected, it is invalid */
		FREELIST(newpkg->files);
		newpkg->infolevel = INFRQ_BASE | INFRQ_DESC | INFRQ_DEPENDS;
	}

	return(newpkg);

pkg_invalid:
	pm_errno = PM_ERR_PKG_INVALID;
error:
	_alpm_pkg_free(newpkg);
	archive_read_finish(archive);

	return(NULL);
}

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
int SYMEXPORT alpm_pkg_load(const char *filename, unsigned short full,
		pmpkg_t **pkg)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(filename != NULL && strlen(filename) != 0,
			RET_ERR(PM_ERR_WRONG_ARGS, -1));
	ASSERT(pkg != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	*pkg = pkg_load(filename, full);
	if(*pkg == NULL) {
		/* pm_errno is set by pkg_load */
		return(-1);
	}

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
