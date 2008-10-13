/*
 *  package.c
 *
 *  Copyright (c) 2002-2007 by Judd Vinet <jvinet@zeroflux.org>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <wchar.h>

#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "package.h"
#include "util.h"

#define CLBUF_SIZE 4096

/* Display the content of a package
 *
 * level: <0 - sync package [-Si]
 *        =0 - file query [-Qip]
 *         1 - localdb query, normal level [-Qi]
 *        >1 - localdb query, extra information (backup files) [-Qii]
 */
void dump_pkg_full(pmpkg_t *pkg, int level)
{
	const char *reason;
	time_t bdate, idate;
	char bdatestr[50] = "", idatestr[50] = "";
	const alpm_list_t *i;
	alpm_list_t *requiredby = NULL, *depstrings = NULL;

	if(pkg == NULL) {
		return;
	}

	/* set variables here, do all output below */
	bdate = alpm_pkg_get_builddate(pkg);
	if(bdate) {
		strftime(bdatestr, 50, "%c", localtime(&bdate));
	}
	idate = alpm_pkg_get_installdate(pkg);
	if(idate) {
		strftime(idatestr, 50, "%c", localtime(&idate));
	}

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

	if(level>0) {
		/* compute this here so we don't get a puase in the middle of output */
		requiredby = alpm_pkg_compute_requiredby(pkg);
	}

	/* actual output */
	string_display(_("Name           :"), alpm_pkg_get_name(pkg));
	string_display(_("Version        :"), alpm_pkg_get_version(pkg));
	string_display(_("URL            :"), alpm_pkg_get_url(pkg));
	list_display(_("Licenses       :"), alpm_pkg_get_licenses(pkg));
	list_display(_("Groups         :"), alpm_pkg_get_groups(pkg));
	list_display(_("Provides       :"), alpm_pkg_get_provides(pkg));
	list_display(_("Depends On     :"), depstrings);
	list_display_linebreak(_("Optional Deps  :"), alpm_pkg_get_optdepends(pkg));
	/* Only applicable if installed */
	if(level > 0) {
		list_display(_("Required By    :"), requiredby);
		FREELIST(requiredby);
	}
	list_display(_("Conflicts With :"), alpm_pkg_get_conflicts(pkg));
	list_display(_("Replaces       :"), alpm_pkg_get_replaces(pkg));
	if(level < 0) {
		printf(_("Download Size  : %6.2f K\n"),
			(float)alpm_pkg_get_size(pkg) / 1024.0);
	}
	if(level == 0) {
		printf(_("Compressed Size: %6.2f K\n"),
			(float)alpm_pkg_get_size(pkg) / 1024.0);
	}

	printf(_("Installed Size : %6.2f K\n"),
			(float)alpm_pkg_get_isize(pkg) / 1024.0);
	string_display(_("Packager       :"), alpm_pkg_get_packager(pkg));
	string_display(_("Architecture   :"), alpm_pkg_get_arch(pkg));
	string_display(_("Build Date     :"), bdatestr);
	if(level > 0) {
		string_display(_("Install Date   :"), idatestr);
		string_display(_("Install Reason :"), reason);
	}
	if(level >= 0) {
		string_display(_("Install Script :"),
				alpm_pkg_has_scriptlet(pkg) ?  _("Yes") : _("No"));
	}

	/* MD5 Sum for sync package */
	if(level < 0) {
		string_display(_("MD5 Sum        :"), alpm_pkg_get_md5sum(pkg));
	}
	string_display(_("Description    :"), alpm_pkg_get_desc(pkg));

	/* Print additional package info if info flag passed more than once */
	if(level > 1) {
		dump_pkg_backups(pkg);
	}

	/* final newline to separate packages */
	printf("\n");

	FREELIST(depstrings);
}

/* Display the content of a sync package
 */
void dump_pkg_sync(pmpkg_t *pkg, const char *treename)
{
	if(pkg == NULL) {
		return;
	}
	string_display(_("Repository     :"), treename);
	dump_pkg_full(pkg, -1);
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
			if(access(path, R_OK) == 0) {
				char *md5sum = alpm_get_md5sum(path);

				if(md5sum == NULL) {
					pm_fprintf(stderr, PM_LOG_ERROR,
						_("could not calculate checksums for %s\n"), path);
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

	pkgname = alpm_pkg_get_name(pkg);
	pkgfiles = alpm_pkg_get_files(pkg);
	root = alpm_option_get_root();

	for(i = pkgfiles; i; i = alpm_list_next(i)) {
		filestr = alpm_list_getdata(i);
		fprintf(stdout, "%s %s%s\n", pkgname, root, filestr);
	}

	fflush(stdout);
}

/* Display the changelog of a package
 */
void dump_pkg_changelog(pmpkg_t *pkg)
{
	void *fp = NULL;

	if((fp = alpm_pkg_changelog_open(pkg)) == NULL) {
		pm_fprintf(stderr, PM_LOG_ERROR, _("no changelog available for '%s'.\n"),
				alpm_pkg_get_name(pkg));
		return;
	} else {
		/* allocate a buffer to get the changelog back in chunks */
		char buf[CLBUF_SIZE];
		int ret = 0;
		while((ret = alpm_pkg_changelog_read(buf, CLBUF_SIZE, pkg, fp))) {
			if(ret < CLBUF_SIZE) {
				/* if we hit the end of the file, we need to add a null terminator */
				*(buf + ret) = '\0';
			}
			printf("%s", buf);
		}
		alpm_pkg_changelog_close(pkg, fp);
		printf("\n");
	}
}

/* vim: set ts=2 sw=2 noet: */
