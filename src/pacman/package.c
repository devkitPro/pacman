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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>

#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "package.h"
#include "util.h"

/* Display the content of an installed package
 *
 * level: <1 - omits N/A info for file query (-Qp)
 *         1 - normal level
 *        >1 - extra information (backup files)
 */
void dump_pkg_full(pmpkg_t *pkg, int level)
{
	const char *reason, *descheader;
	time_t bdate, idate;
	char bdatestr[50], idatestr[50];
	const alpm_list_t *i;
	alpm_list_t *depstrings = NULL;

	if(pkg == NULL) {
		return;
	}

	/* set variables here, do all output below */
	bdate = alpm_pkg_get_builddate(pkg);
	strftime(bdatestr, 50, "%c", localtime(&bdate));
	idate = alpm_pkg_get_installdate(pkg);
	strftime(idatestr, 50, "%c", localtime(&idate));

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

	/* turn depends list into a text list */
	for(i = alpm_pkg_get_depends(pkg); i; i = alpm_list_next(i)) {
		pmdepend_t *dep = (pmdepend_t*)alpm_list_getdata(i);
		depstrings = alpm_list_add(depstrings, alpm_dep_get_string(dep));
	}

	descheader = _("Description    : ");

	/* actual output */
	printf(_("Name           : %s\n"), (char *)alpm_pkg_get_name(pkg));
	printf(_("Version        : %s\n"), (char *)alpm_pkg_get_version(pkg));
	printf(_("URL            : %s\n"), (char *)alpm_pkg_get_url(pkg));
	list_display(_("License        :"), alpm_pkg_get_licenses(pkg));
	list_display(_("Groups         :"), alpm_pkg_get_groups(pkg));
	list_display(_("Provides       :"), alpm_pkg_get_provides(pkg));
	list_display(_("Depends On     :"), depstrings);
	list_display(_("Optional Deps  :"), alpm_pkg_get_optdepends(pkg));
	/* Only applicable if installed */
	if(level > 0) {
		alpm_list_t *requiredby = alpm_pkg_compute_requiredby(pkg);
		list_display(_("Required By    :"), requiredby);
		FREELIST(requiredby);
	}
	list_display(_("Conflicts With :"), alpm_pkg_get_conflicts(pkg));
	list_display(_("Replaces       :"), alpm_pkg_get_replaces(pkg));
	printf(_("Installed Size : %6.2f K\n"),
			(float)alpm_pkg_get_isize(pkg) / 1024.0);
	printf(_("Packager       : %s\n"), (char *)alpm_pkg_get_packager(pkg));
	printf(_("Architecture   : %s\n"), (char *)alpm_pkg_get_arch(pkg));
	printf(_("Build Date     : %s\n"), bdatestr);
	if(level > 0) {
		printf(_("Install Date   : %s\n"), idatestr);
		printf(_("Install Reason : %s\n"), reason);
	}
	printf(_("Install Script : %s\n"),
	         alpm_pkg_has_scriptlet(pkg) ?  _("Yes") : _("No"));

	/* printed using a variable to make i18n safe */
	printf("%s", descheader);
	indentprint(alpm_pkg_get_desc(pkg), mbstowcs(NULL, descheader, 0));
	printf("\n");

	/* Print additional package info if info flag passed more than once */
	if(level > 1) {
		/* call new backup function */
		printf("\n");
		dump_pkg_backups(pkg);
	}
	printf("\n");

	FREELIST(depstrings);
}

/* Display the content of a sync package
 */
void dump_pkg_sync(pmpkg_t *pkg, const char *treename)
{
	const char *descheader, *md5sum;
	const alpm_list_t *i;
	alpm_list_t *depstrings = NULL;
	if(pkg == NULL) {
		return;
	}

	/* turn depends list into a text list */
	for(i = alpm_pkg_get_depends(pkg); i; i = alpm_list_next(i)) {
		pmdepend_t *dep = (pmdepend_t*)alpm_list_getdata(i);
		depstrings = alpm_list_add(depstrings, alpm_dep_get_string(dep));
	}

	descheader = _("Description    : ");

	md5sum = alpm_pkg_get_md5sum(pkg);

	printf(_("Repository     : %s\n"), treename);
	printf(_("Name           : %s\n"), (char *)alpm_pkg_get_name(pkg));
	printf(_("Version        : %s\n"), (char *)alpm_pkg_get_version(pkg));
	list_display(_("Groups         :"), alpm_pkg_get_groups(pkg));
	list_display(_("Provides       :"), alpm_pkg_get_provides(pkg));
	list_display(_("Depends On     :"), depstrings);
	list_display(_("Conflicts With :"), alpm_pkg_get_conflicts(pkg));
	list_display(_("Replaces       :"), alpm_pkg_get_replaces(pkg));
	printf(_("Download Size  : %6.2f K\n"), (float)alpm_pkg_get_size(pkg) / 1024.0);
	printf(_("Installed Size : %6.2f K\n"), (float)alpm_pkg_get_isize(pkg) / 1024.0);

	/* printed using a variable to make i18n safe */
	printf("%s", descheader);
	indentprint(alpm_pkg_get_desc(pkg), mbstowcs(NULL, descheader, 0));
	printf("\n");

	if (md5sum != NULL && md5sum[0] != '\0') {
		printf(_("MD5 Sum        : %s"), md5sum);
	}
	printf("\n");

	FREELIST(depstrings);
}

/* Display list of backup files and their modification states
 */
void dump_pkg_backups(pmpkg_t *pkg)
{
	alpm_list_t *i;
	const char *root = alpm_option_get_root();
	printf(_("Backup Files:\n"));
	if(alpm_pkg_get_backup(pkg)) {
		/* package has backup files, so print them */
		for(i = alpm_pkg_get_backup(pkg); i; i = alpm_list_next(i)) {
			struct stat buf;
			char path[PATH_MAX];
			char *str = strdup(alpm_list_getdata(i));
			char *ptr = index(str, '\t');
			if(ptr == NULL) {
				free(str);
				continue;
			}
			*ptr = '\0';
			ptr++;
			snprintf(path, PATH_MAX-1, "%s%s", root, str);
			/* if we find the file, calculate checksums, otherwise it is missing */
			if(!stat(path, &buf)) {
				char *md5sum = alpm_get_md5sum(path);

				if(md5sum == NULL) {
					fprintf(stderr, _("error: could not calculate checksums for %s\n"),
					        path);
					free(str);
					continue;
				}

				/* if checksums don't match, file has been modified */
				if (strcmp(md5sum, ptr)) {
					printf(_("MODIFIED\t%s\n"), path);
				} else {
					printf(_("Not Modified\t%s\n"), path);
				}
				free(md5sum);
			} else {
				printf(_("MISSING\t\t%s\n"), path);
			}
			free(str);
		}
	} else {
		/* package had no backup files */
		printf(_("(none)\n"));
	}
}

/* List all files contained in a package
 */
void dump_pkg_files(pmpkg_t *pkg)
{
	const char *pkgname, *root, *filestr;
	alpm_list_t *i, *pkgfiles;
	struct stat buf;
	char path[PATH_MAX];

	pkgname = alpm_pkg_get_name(pkg);
	pkgfiles = alpm_pkg_get_files(pkg);
	root = alpm_option_get_root();

	for(i = pkgfiles; i; i = alpm_list_next(i)) {
		filestr = (char*)alpm_list_getdata(i);
		/* build a path so we can stat the filename */
		snprintf(path, PATH_MAX-1, "%s%s", root, filestr);
		if(!lstat(path, &buf)) {
			if(!S_ISDIR(buf.st_mode)) {
				/* don't print directories */
				fprintf(stdout, "%s %s\n", pkgname, path);
			}
		} else {
			fprintf(stderr, "%s %s : %s\n", pkgname, path, strerror(errno));
		}
	}

	fflush(stdout);
	fflush(stderr);
}

/* Display the changelog of an installed package
 */
void dump_pkg_changelog(char *clfile, const char *pkgname)
{
	FILE* fp = NULL;
	char line[PATH_MAX+1];

	if((fp = fopen(clfile, "r")) == NULL)
	{
		fprintf(stderr, _("error: no changelog available for '%s'.\n"), pkgname);
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
