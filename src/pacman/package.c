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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <alpm.h>
/* pacman */
#include "util.h"
#include "list.h"
#include "package.h"

/* Display the content of an installed package
 */
void dump_pkg_full(PM_PKG *pkg, int level)
{
	char *date;

	if(pkg == NULL) {
		return;
	}

	printf("Name           : %s\n", (char *)alpm_pkg_getinfo(pkg, PM_PKG_NAME));
	printf("Version        : %s\n", (char *)alpm_pkg_getinfo(pkg, PM_PKG_VERSION));

	PM_LIST_display("Groups         :", alpm_pkg_getinfo(pkg, PM_PKG_GROUPS));

	printf("Packager       : %s\n", (char *)alpm_pkg_getinfo(pkg, PM_PKG_PACKAGER));
	printf("URL            : %s\n", (char *)alpm_pkg_getinfo(pkg, PM_PKG_URL));
	printf("License        : %s\n", (char *)alpm_pkg_getinfo(pkg, PM_PKG_LICENSE));
	printf("Architecture   : %s\n", (char *)alpm_pkg_getinfo(pkg, PM_PKG_ARCH));
	printf("Size           : %ld\n", (long int)alpm_pkg_getinfo(pkg, PM_PKG_SIZE));

	date = alpm_pkg_getinfo(pkg, PM_PKG_BUILDDATE);
	printf("Build Date     : %s %s\n", date, strlen(date) ? "UTC" : "");

	date = alpm_pkg_getinfo(pkg, PM_PKG_INSTALLDATE);
	printf("Install Date   : %s %s\n", date, strlen(date) ? "UTC" : "");

	printf("Install Script : %s\n", (alpm_pkg_getinfo(pkg, PM_PKG_SCRIPLET) ? "Yes" : "No"));

	printf("Reason:        : ");
	switch((int)alpm_pkg_getinfo(pkg, PM_PKG_REASON)) {
		case PM_PKG_REASON_EXPLICIT:
			printf("explicitly installed\n");
			break;
		case PM_PKG_REASON_DEPEND:
			printf("installed as a dependency for another package\n");
			break;
		default:
			printf("unknown\n");
			break;
	}

	PM_LIST_display("Provides       :", alpm_pkg_getinfo(pkg, PM_PKG_PROVIDES));
	PM_LIST_display("Depends On     :", alpm_pkg_getinfo(pkg, PM_PKG_DEPENDS));
	PM_LIST_display("Required By    :", alpm_pkg_getinfo(pkg, PM_PKG_REQUIREDBY));
	PM_LIST_display("Conflicts With :", alpm_pkg_getinfo(pkg, PM_PKG_CONFLICTS));

	printf("Description    : ");
	indentprint(alpm_pkg_getinfo(pkg, PM_PKG_DESC), 17);
	printf("\n");

	/*if(level > 1 && info->backup) {
		PM_LIST *i;
		char *root;

		alpm_get_option(PM_OPT_ROOT, (long *)&root);
		fprintf(stdout, "\n");
		for(i = alpm_first_entry(info->backup); i; i = alpm_next_entry(i)) {
			struct stat buf;
			char path[PATH_MAX];
			char *md5sum;
			char *str = strdup(alpm_get_entry(i));
			char *ptr = index(str, '\t');
			if(ptr == NULL) {
				free(str);
				str = NULL;
				continue;
			}
			*ptr = '\0';
			ptr++;
			snprintf(path, PATH_MAX-1, "%s%s", root, str);
			if(!stat(path, &buf)) {
				md5sum = alpm_get_md5sum(path);
				if(md5sum == NULL) {
					fprintf(stderr, "error calculating md5sum for %s\n", path);
					continue;
				}
				if(strcmp(md5sum, ptr)) {
					fprintf(stdout, "MODIFIED\t%s\n", path);
				} else {
					fprintf(stdout, "NOT MODIFIED\t%s\n", path);
				}
			} else {
				fprintf(stdout, "MISSING\t\t%s\n", path);
			}
			free(str);
			str = NULL;
		}
	}*/

	printf("\n");
}


/* Display the content of a sync package
 */
void dump_pkg_sync(PM_PKG *pkg, char *treename)
{
	if(pkg == NULL) {
		return;
	}

	printf("Repository        : %s\n", treename);
	printf("Name              : %s\n", (char *)alpm_pkg_getinfo(pkg, PM_PKG_NAME));
	printf("Version           : %s\n", (char *)alpm_pkg_getinfo(pkg, PM_PKG_VERSION));

	PM_LIST_display("Groups            :", alpm_pkg_getinfo(pkg, PM_PKG_GROUPS));
	PM_LIST_display("Provides          :", alpm_pkg_getinfo(pkg, PM_PKG_PROVIDES));
	PM_LIST_display("Depends On        :", alpm_pkg_getinfo(pkg, PM_PKG_DEPENDS));
	PM_LIST_display("Conflicts With    :", alpm_pkg_getinfo(pkg, PM_PKG_CONFLICTS));
	PM_LIST_display("Replaces          :", alpm_pkg_getinfo(pkg, PM_PKG_REPLACES));

	printf("Size (compressed) : %ld\n", (long)alpm_pkg_getinfo(pkg, PM_PKG_SIZE));
	printf("Description       : ");
	indentprint(alpm_pkg_getinfo(pkg, PM_PKG_DESC), 17);
	printf("\nMD5 Sum           : %s\n", (char *)alpm_pkg_getinfo(pkg, PM_PKG_MD5SUM));
}

void dump_pkg_files(PM_PKG *pkg)
{
	char *pkgname;
	PM_LIST *i, *pkgfiles;

	pkgname = alpm_pkg_getinfo(pkg, PM_PKG_NAME);
	pkgfiles = alpm_pkg_getinfo(pkg, PM_PKG_FILES);

	for(i = pkgfiles; i; i = alpm_list_next(i)) {
		fprintf(stdout, "%s %s\n", (char *)pkgname, (char *)alpm_list_getdata(i));
	}

	fflush(stdout);
}

int split_pkgname(char *target, char *name, char *version)
{
	char tmp[512];
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
	strncpy(tmp, p, 512);
	/* trim file extension (if any) */
	if((p = strstr(tmp, PM_EXT_PKG))) {
		*p = 0;
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
	strncpy(version, p+1, 64);
	*p = 0;

	strncpy(name, tmp, 256);

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
