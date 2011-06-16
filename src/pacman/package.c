/*
 *  package.c
 *
 *  Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "package.h"
#include "util.h"
#include "conf.h"

#define CLBUF_SIZE 4096

/**
 * Display the details of a package.
 * Extra information entails 'required by' info for sync packages and backup
 * files info for local packages.
 * @param pkg package to display information for
 * @param from the type of package we are dealing with
 * @param extra should we show extra information
 */
void dump_pkg_full(pmpkg_t *pkg, enum pkg_from from, int extra)
{
	const char *reason;
	time_t bdate, idate;
	char bdatestr[50] = "", idatestr[50] = "";
	const char *label;
	double size;
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
		pmdepend_t *dep = (pmdepend_t *)alpm_list_getdata(i);
		depstrings = alpm_list_add(depstrings, alpm_dep_compute_string(dep));
	}

	if(extra || from == PKG_FROM_LOCALDB) {
		/* compute this here so we don't get a pause in the middle of output */
		requiredby = alpm_pkg_compute_requiredby(pkg);
	}

	/* actual output */
	if(from == PKG_FROM_SYNCDB) {
		string_display(_("Repository     :"),
				alpm_db_get_name(alpm_pkg_get_db(pkg)));
	}
	string_display(_("Name           :"), alpm_pkg_get_name(pkg));
	string_display(_("Version        :"), alpm_pkg_get_version(pkg));
	string_display(_("URL            :"), alpm_pkg_get_url(pkg));
	list_display(_("Licenses       :"), alpm_pkg_get_licenses(pkg));
	list_display(_("Groups         :"), alpm_pkg_get_groups(pkg));
	list_display(_("Provides       :"), alpm_pkg_get_provides(pkg));
	list_display(_("Depends On     :"), depstrings);
	list_display_linebreak(_("Optional Deps  :"), alpm_pkg_get_optdepends(pkg));
	if(extra || from == PKG_FROM_LOCALDB) {
		list_display(_("Required By    :"), requiredby);
	}
	list_display(_("Conflicts With :"), alpm_pkg_get_conflicts(pkg));
	list_display(_("Replaces       :"), alpm_pkg_get_replaces(pkg));

	size = humanize_size(alpm_pkg_get_size(pkg), 'K', 1, &label);
	if(from == PKG_FROM_SYNCDB) {
		printf(_("Download Size  : %6.2f %s\n"), size, label);
	} else if(from == PKG_FROM_FILE) {
		printf(_("Compressed Size: %6.2f %s\n"), size, label);
	}

	size = humanize_size(alpm_pkg_get_isize(pkg), 'K', 1, &label);
	printf(_("Installed Size : %6.2f %s\n"), size, label);

	string_display(_("Packager       :"), alpm_pkg_get_packager(pkg));
	string_display(_("Architecture   :"), alpm_pkg_get_arch(pkg));
	string_display(_("Build Date     :"), bdatestr);
	if(from == PKG_FROM_LOCALDB) {
		string_display(_("Install Date   :"), idatestr);
		string_display(_("Install Reason :"), reason);
	}
	if(from == PKG_FROM_FILE || from == PKG_FROM_LOCALDB) {
		string_display(_("Install Script :"),
				alpm_pkg_has_scriptlet(pkg) ?  _("Yes") : _("No"));
	}

	if(from == PKG_FROM_SYNCDB) {
		string_display(_("MD5 Sum        :"), alpm_pkg_get_md5sum(pkg));
	}
	string_display(_("Description    :"), alpm_pkg_get_desc(pkg));

	/* Print additional package info if info flag passed more than once */
	if(from == PKG_FROM_LOCALDB && extra) {
		dump_pkg_backups(pkg);
	}

	/* final newline to separate packages */
	printf("\n");

	FREELIST(depstrings);
	FREELIST(requiredby);
}

static const char *get_backup_file_status(const char *root,
		const pmbackup_t *backup)
{
	char path[PATH_MAX];
	const char *ret;

	snprintf(path, PATH_MAX, "%s%s", root, backup->name);

	/* if we find the file, calculate checksums, otherwise it is missing */
	if(access(path, R_OK) == 0) {
		char *md5sum = alpm_compute_md5sum(path);

		if(md5sum == NULL) {
			pm_fprintf(stderr, PM_LOG_ERROR,
					_("could not calculate checksums for %s\n"), path);
			return NULL;
		}

		/* if checksums don't match, file has been modified */
		if(strcmp(md5sum, backup->hash) != 0) {
			ret = "MODIFIED";
		} else {
			ret = "UNMODIFIED";
		}
		free(md5sum);
	} else {
		switch(errno) {
			case EACCES:
				ret = "UNREADABLE";
				break;
			case ENOENT:
				ret = "MISSING";
				break;
			default:
				ret = "UNKNOWN";
		}
	}
	return ret;
}

/* Display list of backup files and their modification states
 */
void dump_pkg_backups(pmpkg_t *pkg)
{
	alpm_list_t *i;
	const char *root = alpm_option_get_root(config->handle);
	printf(_("Backup Files:\n"));
	if(alpm_pkg_get_backup(pkg)) {
		/* package has backup files, so print them */
		for(i = alpm_pkg_get_backup(pkg); i; i = alpm_list_next(i)) {
			const pmbackup_t *backup = alpm_list_getdata(i);
			const char *value;
			if(!backup->hash) {
				continue;
			}
			value = get_backup_file_status(root, backup);
			printf("%s\t%s%s\n", value, root, backup->name);
		}
	} else {
		/* package had no backup files */
		printf(_("(none)\n"));
	}
}

/* List all files contained in a package
 */
void dump_pkg_files(pmpkg_t *pkg, int quiet)
{
	const char *pkgname, *root, *filestr;
	alpm_list_t *i, *pkgfiles;

	pkgname = alpm_pkg_get_name(pkg);
	pkgfiles = alpm_pkg_get_files(pkg);
	root = alpm_option_get_root(config->handle);

	for(i = pkgfiles; i; i = alpm_list_next(i)) {
		filestr = alpm_list_getdata(i);
		if(!quiet){
			fprintf(stdout, "%s %s%s\n", pkgname, root, filestr);
		} else {
			fprintf(stdout, "%s%s\n", root, filestr);
		}
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
		size_t ret = 0;
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
