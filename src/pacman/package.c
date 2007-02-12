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
#include <alpm_list.h>
/* pacman */
#include "log.h"
#include "util.h"
#include "package.h"

/* Display the content of an installed package
 *
 * level: <1 - omits N/A info for file query (-Qp)
 *         1 - normal level
 *        >1 - extra information (backup files)
 */
void dump_pkg_full(pmpkg_t *pkg, int level)
{
	const char *bdate, *type, *idate, *reason, *descheader;

	if(pkg == NULL) {
		return;
	}

	/* set variables here, do all output below */
	bdate = alpm_pkg_get_builddate(pkg);
	type = alpm_pkg_get_buildtype(pkg);
	idate = alpm_pkg_get_installdate(pkg);

	switch((long)alpm_pkg_get_reason(pkg)) {
		case PM_PKG_REASON_EXPLICIT:
			reason = _("Explicitly installed");
			break;
		case PM_PKG_REASON_DEPEND:
			reason = _("Installed as a dependency for another package");
			break;
		default:
			reason = _("Unknown");
			break;
	}

	descheader = _("Description    : ");

	/* actual output */
	printf(_("Name           : %s\n"), (char *)alpm_pkg_get_name(pkg));
	printf(_("Version        : %s\n"), (char *)alpm_pkg_get_version(pkg));
	printf(_("URL            : %s\n"), (char *)alpm_pkg_get_url(pkg));
	list_display(_("License        :"), alpm_pkg_get_licenses(pkg));
	list_display(_("Groups         :"), alpm_pkg_get_groups(pkg));
	list_display(_("Provides       :"), alpm_pkg_get_provides(pkg));
	list_display(_("Depends On     :"), alpm_pkg_get_depends(pkg));
	list_display(_("Removes        :"), alpm_pkg_get_removes(pkg));
	/* Only applicable if installed */
	if(level > 0) {
		list_display(_("Required By    :"), alpm_pkg_get_requiredby(pkg));
	}
	list_display(_("Conflicts With :"), alpm_pkg_get_conflicts(pkg));
	printf(_("Installed Size : %ld K\n"), (long)alpm_pkg_get_size(pkg) / 1024);
	printf(_("Packager       : %s\n"), (char *)alpm_pkg_get_packager(pkg));
	printf(_("Architecture   : %s\n"), (char *)alpm_pkg_get_arch(pkg));
	printf(_("Build Date     : %s %s\n"), bdate, strlen(bdate) ? "UTC" : "");
	printf(_("Build Type     : %s\n"), strlen(type) ? type : _("Unknown"));
	if(level > 0) {
		printf(_("Install Date   : %s %s\n"), idate, strlen(idate) ? "UTC" : "");
		printf(_("Install Reason : %s\n"), reason);
	}
	printf(_("Install Script : %s\n"),
	         alpm_pkg_has_scriptlet(pkg) ?  _("Yes") : _("No"));

	/* printed using a variable to make i18n safe */
	printf("%s", descheader);
	indentprint(alpm_pkg_get_desc(pkg), strlen(descheader));
	printf("\n");

	/* Print additional package info if info flag passed more than once */
	if(level > 1) {
		/* call new backup function */
		printf("\n");
		dump_pkg_backups(pkg);
	}
	printf("\n");
}

/* Display the content of a sync package
 */
void dump_pkg_sync(pmpkg_t *pkg, const char *treename)
{
	char *descheader, *md5sum, *sha1sum;
	if(pkg == NULL) {
		return;
	}

	descheader = _("Description    : ");

	md5sum = (char *)alpm_pkg_get_md5sum(pkg);
	sha1sum = (char *)alpm_pkg_get_sha1sum(pkg);
	
	printf(_("Repository     : %s\n"), treename);
	printf(_("Name           : %s\n"), (char *)alpm_pkg_get_name(pkg));
	printf(_("Version        : %s\n"), (char *)alpm_pkg_get_version(pkg));
	list_display(_("Groups         :"), alpm_pkg_get_groups(pkg));
	list_display(_("Provides       :"), alpm_pkg_get_provides(pkg));
	list_display(_("Depends On     :"), alpm_pkg_get_depends(pkg));
	list_display(_("Removes        :"), alpm_pkg_get_removes(pkg));
	list_display(_("Conflicts With :"), alpm_pkg_get_conflicts(pkg));
	list_display(_("Replaces       :"), alpm_pkg_get_replaces(pkg));
	printf(_("Download Size  : %6.2f K\n"), (float)alpm_pkg_get_size(pkg) / 1024.0);
	printf(_("Installed Size : %6.2f K\n"), (float)alpm_pkg_get_isize(pkg) / 1024.0);
	
	/* printed using a variable to make i18n safe */
	printf("%s", descheader);
	indentprint(alpm_pkg_get_desc(pkg), strlen(descheader));
	printf("\n");
	
	if (md5sum != NULL && md5sum[0] != '\0') {
		printf(_("MD5 Sum        : %s"), md5sum);
	}
	if (sha1sum != NULL && sha1sum[0] != '\0') {
		printf(_("SHA1 Sum       : %s"), sha1sum);
	}
	printf("\n");
}

/* Display list of backup files and their modification states
 */
void dump_pkg_backups(pmpkg_t *pkg)
{
	alpm_list_t *i;
	const char *root = alpm_option_get_root();
	printf(_("Backup Files :\n"));
	for(i = alpm_pkg_get_backup(pkg); i; i = alpm_list_next(i)) {
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
		/* if we find the file, calculate checksums, otherwise it is missing */
		if(!stat(path, &buf)) {
			char *sum;
			char *md5sum = alpm_get_md5sum(path);
			char *sha1sum = alpm_get_sha1sum(path);

			if(md5sum == NULL || sha1sum == NULL) {
				ERR(NL, _("error calculating checksums for %s\n"), path);
				FREE(str);
				continue;
			}
			/* TODO Is this a good way to check type of backup stored?
			 * We aren't storing it anywhere in the database. */
			if (strlen(ptr) == 32) {
				sum = md5sum;
			} else { /*if (strlen(ptr) == 40) */
				sum = sha1sum;
			}
			/* if checksums don't match, file has been modified */
			if (strcmp(sum, ptr)) {
				printf(_("MODIFIED\t%s\n"), path);
			} else {
				printf(_("Not Modified\t%s\n"), path);
			}
			FREE(md5sum);
			FREE(sha1sum);
		} else {
			printf(_("MISSING\t\t%s\n"), path);
		}
		FREE(str);
	}
}
	
/* List all files contained in a package
 */
void dump_pkg_files(pmpkg_t *pkg)
{
	const char *pkgname;
	alpm_list_t *i, *pkgfiles;

	pkgname = alpm_pkg_get_name(pkg);
	pkgfiles = alpm_pkg_get_files(pkg);

	for(i = pkgfiles; i; i = alpm_list_next(i)) {
		fprintf(stdout, "%s %s%s\n", pkgname, alpm_option_get_root(),
						(char *)alpm_list_getdata(i));
	}

	fflush(stdout);
}

/* Display the changelog of an installed package
 */
void dump_pkg_changelog(char *clfile, const char *pkgname)
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
			fgets(line, (int)PATH_MAX, fp);
			printf("%s", line);
			line[0] = '\0';
		}
		fclose(fp);
		return;
	}
}

/* vim: set ts=2 sw=2 noet: */
