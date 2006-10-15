/*
 *  package.c
 * 
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
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
#include <limits.h>
#include <sys/stat.h>
#include <libintl.h>

#include <alpm.h>
/* pacman */
#include "log.h"
#include "util.h"
#include "list.h"
#include "package.h"

/* Display the content of an installed package
 */
void dump_pkg_full(PM_PKG *pkg, int level)
{
	char *date, *type;

	if(pkg == NULL) {
		return;
	}

	printf(_("Name           : %s\n"), (char *)alpm_pkg_getinfo(pkg, PM_PKG_NAME));
	printf(_("Version        : %s\n"), (char *)alpm_pkg_getinfo(pkg, PM_PKG_VERSION));

	PM_LIST_display(_("Groups         :"), alpm_pkg_getinfo(pkg, PM_PKG_GROUPS));

	printf(_("Packager       : %s\n"), (char *)alpm_pkg_getinfo(pkg, PM_PKG_PACKAGER));
	printf("URL            : %s\n", (char *)alpm_pkg_getinfo(pkg, PM_PKG_URL));
	PM_LIST_display(_("License        :"), alpm_pkg_getinfo(pkg, PM_PKG_LICENSE));
	printf(_("Architecture   : %s\n"), (char *)alpm_pkg_getinfo(pkg, PM_PKG_ARCH));
	printf(_("Size           : %ld\n"), (long int)alpm_pkg_getinfo(pkg, PM_PKG_SIZE));

	date = alpm_pkg_getinfo(pkg, PM_PKG_BUILDDATE);
	printf(_("Build Date     : %s %s\n"), date, strlen(date) ? "UTC" : "");
	type = alpm_pkg_getinfo(pkg, PM_PKG_BUILDTYPE);
	printf(_("Build Type     : %s\n"), strlen(type) ? type : _("Unknown"));
	date = alpm_pkg_getinfo(pkg, PM_PKG_INSTALLDATE);
	printf(_("Install Date   : %s %s\n"), date, strlen(date) ? "UTC" : "");

	printf(_("Install Script : %s\n"), alpm_pkg_getinfo(pkg, PM_PKG_SCRIPLET) ? _("Yes") : _("No"));

	printf(_("Reason         : "));
	switch((long)alpm_pkg_getinfo(pkg, PM_PKG_REASON)) {
		case PM_PKG_REASON_EXPLICIT:
			printf(_("Explicitly installed\n"));
			break;
		case PM_PKG_REASON_DEPEND:
			printf(_("Installed as a dependency for another package\n"));
			break;
		default:
			printf(_("Unknown\n"));
			break;
	}

	PM_LIST_display(_("Provides       :"), alpm_pkg_getinfo(pkg, PM_PKG_PROVIDES));
	PM_LIST_display(_("Depends On     :"), alpm_pkg_getinfo(pkg, PM_PKG_DEPENDS));
	PM_LIST_display(_("Removes        :"), alpm_pkg_getinfo(pkg, PM_PKG_REMOVES));
	PM_LIST_display(_("Required By    :"), alpm_pkg_getinfo(pkg, PM_PKG_REQUIREDBY));
	PM_LIST_display(_("Conflicts With :"), alpm_pkg_getinfo(pkg, PM_PKG_CONFLICTS));

	printf(_("Description    : "));
	indentprint(alpm_pkg_getinfo(pkg, PM_PKG_DESC), 17);
	printf("\n");

	if(level > 1) {
		PM_LIST *i;
		char *root;
		alpm_get_option(PM_OPT_ROOT, (long *)&root);
		fprintf(stdout, "\n");
		for(i = alpm_list_first(alpm_pkg_getinfo(pkg, PM_PKG_BACKUP)); i; i = alpm_list_next(i)) {
			struct stat buf;
			char path[PATH_MAX];
			char *str = strdup(alpm_list_getdata(i));
			char *ptr = index(str, '\t');
			if(ptr == NULL) {
				FREE(str);
				continue;
			}
			*ptr = '\0';
			ptr++;
			snprintf(path, PATH_MAX-1, "%s%s", root, str);
			if(!stat(path, &buf)) {
				char *md5sum = alpm_get_md5sum(path);
				char *sha1sum = alpm_get_sha1sum(path);
				if(md5sum == NULL && sha1sum == NULL) {
					ERR(NL, _("error calculating md5sum or sha1sum for %s\n"), path);
					FREE(str);
					continue;
				}
				if (!sha1sum) 
				    printf(_("%sMODIFIED\t%s\n"), strcmp(md5sum, ptr) ? "" : _("NOT "), path);
				if (!md5sum)
				    printf(_("%sMODIFIED\t%s\n"), strcmp(sha1sum, ptr) ? "" : _("NOT "), path);
				FREE(md5sum);
				FREE(sha1sum);
			} else {
				printf(_("MISSING\t\t%s\n"), path);
			}
			FREE(str);
		}
	}

	printf("\n");
}

/* Display the content of a sync package
 */
void dump_pkg_sync(PM_PKG *pkg, char *treename)
{
	char *tmp1, *tmp2;
	if(pkg == NULL) {
		return;
	}

	printf(_("Repository        : %s\n"), treename);
	printf(_("Name              : %s\n"), (char *)alpm_pkg_getinfo(pkg, PM_PKG_NAME));
	printf(_("Version           : %s\n"), (char *)alpm_pkg_getinfo(pkg, PM_PKG_VERSION));

	PM_LIST_display(_("Groups            :"), alpm_pkg_getinfo(pkg, PM_PKG_GROUPS));
	PM_LIST_display(_("Provides          :"), alpm_pkg_getinfo(pkg, PM_PKG_PROVIDES));
	PM_LIST_display(_("Depends On        :"), alpm_pkg_getinfo(pkg, PM_PKG_DEPENDS));
	PM_LIST_display(_("Removes           :"), alpm_pkg_getinfo(pkg, PM_PKG_REMOVES));
	PM_LIST_display(_("Conflicts With    :"), alpm_pkg_getinfo(pkg, PM_PKG_CONFLICTS));
	PM_LIST_display(_("Replaces          :"), alpm_pkg_getinfo(pkg, PM_PKG_REPLACES));

	printf(_("Size (compressed) : %ld\n"), (long)alpm_pkg_getinfo(pkg, PM_PKG_SIZE));
	printf(_("Size (uncompressed):%ld\n"), (long)alpm_pkg_getinfo(pkg, PM_PKG_USIZE));
	printf(_("Description       : "));
	indentprint(alpm_pkg_getinfo(pkg, PM_PKG_DESC), 20);
	tmp1 = (char *)alpm_pkg_getinfo(pkg, PM_PKG_MD5SUM);
	if (tmp1 != NULL && tmp1[0] != '\0') {
	    printf(_("\nMD5 Sum           : %s"), (char *)alpm_pkg_getinfo(pkg, PM_PKG_MD5SUM));
	    }
	tmp2 = (char *)alpm_pkg_getinfo(pkg, PM_PKG_SHA1SUM);
	if (tmp2 != NULL && tmp2[0] != '\0') {
	    printf(_("\nSHA1 Sum          : %s"), (char *)alpm_pkg_getinfo(pkg, PM_PKG_SHA1SUM));
	}
	printf("\n");
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

/* Display the changelog of an installed package
 */
void dump_pkg_changelog(char *clfile, char *pkgname)
{
	FILE* fp = NULL;
	char line[PATH_MAX+1];

	if((fp = fopen(clfile, "r")) == NULL)
	{
		ERR(NL, _("No changelog available for '%s'.\n"), pkgname);
		return;
	}
	else
	{
		while(!feof(fp))
		{
			fgets(line, PATH_MAX, fp);
			printf("%s", line);
			line[0] = '\0';
		}
		fclose(fp);
		return;
	}
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
	/* trim architecture */
	if((p = strrchr(tmp, '-'))) {
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
