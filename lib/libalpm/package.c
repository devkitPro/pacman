/*
 *  package.c
 * 
 *  Copyright (c) 2002-2005 by Judd Vinet <jvinet@zeroflux.org>
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
#include <fcntl.h>
#include <string.h>
#include <libtar.h>
#include <zlib.h>
/* pacman */
#include "log.h"
#include "util.h"
#include "error.h"
#include "list.h"
#include "package.h"

pmpkg_t *pkg_new()
{
	pmpkg_t* pkg = NULL;

	MALLOC(pkg, sizeof(pmpkg_t));

	pkg->name[0]        = '\0';
	pkg->version[0]     = '\0';
	pkg->desc[0]        = '\0';
	pkg->url[0]         = '\0';
	pkg->license[0]     = '\0';
	pkg->builddate[0]   = '\0';
	pkg->installdate[0] = '\0';
	pkg->packager[0]    = '\0';
	pkg->md5sum[0]      = '\0';
	pkg->arch[0]        = '\0';
	pkg->size           = 0;
	pkg->scriptlet      = 0;
	pkg->force          = 0;
	pkg->reason         = PM_PKG_REASON_EXPLICIT;
	pkg->requiredby     = NULL;
	pkg->conflicts      = NULL;
	pkg->files          = NULL;
	pkg->backup         = NULL;
	pkg->depends        = NULL;
	pkg->groups         = NULL;
	pkg->provides       = NULL;
	pkg->replaces       = NULL;
	/* internal */
	pkg->origin         = 0;
	pkg->data           = NULL;
	pkg->infolevel      = 0;

	return(pkg);
}

void pkg_free(pmpkg_t *pkg)
{
	if(pkg == NULL) {
		return;
	}

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
	free(pkg);

	return;
}

/* Parses the package description file for the current package
 *
 * Returns: 0 on success, 1 on error
 *
 */
static int parse_descfile(char *descfile, pmpkg_t *info, int output)
{
	FILE* fp = NULL;
	char line[PATH_MAX+1];
	char* ptr = NULL;
	char* key = NULL;
	int linenum = 0;

	if((fp = fopen(descfile, "r")) == NULL) {
		_alpm_log(PM_LOG_ERROR, "could not open file %s", descfile);
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
			printf("%s\n", line);
		}
		ptr = line;
		key = strsep(&ptr, "=");
		if(key == NULL || ptr == NULL) {
			fprintf(stderr, "%s: syntax error in description file line %d\n",
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
				info->groups = pm_list_add(info->groups, strdup(ptr));
			} else if(!strcmp(key, "URL")) {
				strncpy(info->url, ptr, sizeof(info->url));
			} else if(!strcmp(key, "LICENSE")) {
				strncpy(info->license, ptr, sizeof(info->license));
			} else if(!strcmp(key, "BUILDDATE")) {
				strncpy(info->builddate, ptr, sizeof(info->builddate));
			} else if(!strcmp(key, "INSTALLDATE")) {
				strncpy(info->installdate, ptr, sizeof(info->installdate));
			} else if(!strcmp(key, "PACKAGER")) {
				strncpy(info->packager, ptr, sizeof(info->packager));
			} else if(!strcmp(key, "ARCH")) {
				strncpy(info->arch, ptr, sizeof(info->arch));
			} else if(!strcmp(key, "SIZE")) {
				char tmp[32];
				strncpy(tmp, ptr, sizeof(tmp));
				info->size = atol(tmp);
			} else if(!strcmp(key, "DEPEND")) {
				info->depends = pm_list_add(info->depends, strdup(ptr));
			} else if(!strcmp(key, "CONFLICT")) {
				info->conflicts = pm_list_add(info->conflicts, strdup(ptr));
			} else if(!strcmp(key, "REPLACES")) {
				info->replaces = pm_list_add(info->replaces, strdup(ptr));
			} else if(!strcmp(key, "PROVIDES")) {
				info->provides = pm_list_add(info->provides, strdup(ptr));
			} else if(!strcmp(key, "BACKUP")) {
				info->backup = pm_list_add(info->backup, strdup(ptr));
			} else {
				fprintf(stderr, "%s: syntax error in description file line %d\n",
					info->name[0] != '\0' ? info->name : "error", linenum);
			}
		}
		line[0] = '\0';
	}
	fclose(fp);
	unlink(descfile);

	return(0);
}

pmpkg_t *pkg_load(char *pkgfile)
{
	char *expath;
	int i;
	int config = 0;
	int filelist = 0;
	int scriptcheck = 0;
	TAR *tar;
	pmpkg_t *info = NULL;
	tartype_t gztype = {
		(openfunc_t)_alpm_gzopen_frontend,
		(closefunc_t)gzclose,
		(readfunc_t)gzread,
		(writefunc_t)gzwrite
	};

	if(pkgfile == NULL) {
		RET_ERR(PM_ERR_WRONG_ARGS, NULL);
	}

	if(tar_open(&tar, pkgfile, &gztype, O_RDONLY, 0, TAR_GNU) == -1) {
		RET_ERR(PM_ERR_NOT_A_FILE, NULL);
	}

	info = pkg_new();
	if(info == NULL) {
		tar_close(tar);
		RET_ERR(PM_ERR_MEMORY, NULL);
	}

	for(i = 0; !th_read(tar); i++) {
		if(config && filelist && scriptcheck) {
			/* we have everything we need */
			break;
		}
		if(!strcmp(th_get_pathname(tar), ".PKGINFO")) {
			char *descfile;

			/* extract this file into /tmp. it has info for us */
			descfile = strdup("/tmp/alpm_XXXXXX");
			mkstemp(descfile);
			tar_extract_file(tar, descfile);
			/* parse the info file */
			if(parse_descfile(descfile, info, 0) == -1) {
				goto error;
			}
			if(!strlen(info->name)) {
				_alpm_log(PM_LOG_ERROR, "missing package name in %s", pkgfile);
				goto error;
			}
			if(!strlen(info->version)) {
				_alpm_log(PM_LOG_ERROR, "missing package version in %s", pkgfile);
				goto error;
			}
			config = 1;
			FREE(descfile);
			continue;
		} else if(!strcmp(th_get_pathname(tar), "._install") || !strcmp(th_get_pathname(tar), ".INSTALL")) {
			info->scriptlet = 1;
			scriptcheck = 1;
		} else if(!strcmp(th_get_pathname(tar), ".FILELIST")) {
			/* Build info->files from the filelist */
			FILE *fp;
			char *fn;
			char *str;
			
			MALLOC(str, PATH_MAX);
			fn = strdup("/tmp/alpm_XXXXXX");
			mkstemp(fn);
			tar_extract_file(tar, fn);
			fp = fopen(fn, "r");
			while(!feof(fp)) {
				if(fgets(str, PATH_MAX, fp) == NULL) {
					continue;
				}
				_alpm_strtrim(str);
				info->files = pm_list_add(info->files, strdup(str));
			}
			FREE(str);
			fclose(fp);
			if(unlink(fn)) {
				_alpm_log(PM_LOG_WARNING, "could not remove tempfile %s\n", fn);
			}
			FREE(fn);
			filelist = 1;
			continue;
		} else {
			scriptcheck = 1;
			if(!filelist) {
				/* no .FILELIST present in this package..  build the filelist the */
				/* old-fashioned way, one at a time */
				expath = strdup(th_get_pathname(tar));
				info->files = pm_list_add(info->files, expath);
			}
		}

		if(TH_ISREG(tar) && tar_skip_regfile(tar)) {
			_alpm_log(PM_LOG_ERROR, "bad package file in %s", pkgfile);
			goto error;
		}
		expath = NULL;
	}
	tar_close(tar);

	if(!config) {
		_alpm_log(PM_LOG_ERROR, "missing package info file in %s", pkgfile);
		goto error;
	}

	/* internal */
	info->origin = PKG_FROM_FILE;
	info->data = strdup(pkgfile);
	info->infolevel = 0xFF;

	return(info);

error:
	FREEPKG(info);
	tar_close(tar);

	return(NULL);
}

/* Helper function for sorting packages
 */
int pkg_cmp(const void *p1, const void *p2)
{
	pmpkg_t *pkg1 = (pmpkg_t *)p1;
	pmpkg_t *pkg2 = (pmpkg_t *)p2;

	return(strcmp(pkg1->name, pkg2->name));
}

/* Test for existence of a package in a PMList*
 * of pmpkg_t*
 *
 * returns:  0 for no match
 *           1 for identical match
 *          -1 for name-only match (version mismatch)
 */
int pkg_isin(pmpkg_t *needle, PMList *haystack)
{
	PMList *lp;

	if(needle == NULL || haystack == NULL) {
		return(0);
	}

	for(lp = haystack; lp; lp = lp->next) {
		pmpkg_t *info = lp->data;

		if(info && !strcmp(info->name, needle->name)) {
			if(!strcmp(info->version, needle->version)) {
				return(1);
			}
			return(-1);
		}
	}
	return(0);
}

/* vim: set ts=2 sw=2 noet: */
