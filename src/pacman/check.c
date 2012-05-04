/*
 *  check.c
 *
 *  Copyright (c) 2012 Pacman Development Team <pacman-dev@archlinux.org>
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

#include <limits.h>
#include <string.h>
#include <errno.h>

/* pacman */
#include "check.h"
#include "conf.h"
#include "util.h"

static int check_file_exists(const char *pkgname, const char * filepath,
		struct stat * st)
{
	/* use lstat to prevent errors from symlinks */
	if(lstat(filepath, st) != 0) {
		if(config->quiet) {
			printf("%s %s\n", pkgname, filepath);
		} else {
			pm_printf(ALPM_LOG_WARNING, "%s: %s (%s)\n",
					pkgname, filepath, strerror(errno));
		}
		return 1;
	}

	return 0;
}

/* Loop through the files of the package to check if they exist. */
int check(alpm_pkg_t *pkg)
{
	const char *root, *pkgname;
	size_t errors = 0;
	size_t rootlen;
	char filepath[PATH_MAX];
	alpm_filelist_t *filelist;
	size_t i;

	root = alpm_option_get_root(config->handle);
	rootlen = strlen(root);
	if(rootlen + 1 > PATH_MAX) {
		/* we are in trouble here */
		pm_printf(ALPM_LOG_ERROR, _("path too long: %s%s\n"), root, "");
		return 1;
	}
	strcpy(filepath, root);

	pkgname = alpm_pkg_get_name(pkg);
	filelist = alpm_pkg_get_files(pkg);
	for(i = 0; i < filelist->count; i++) {
		const alpm_file_t *file = filelist->files + i;
		struct stat st;
		const char *path = file->name;

		if(rootlen + 1 + strlen(path) > PATH_MAX) {
			pm_printf(ALPM_LOG_WARNING, _("path too long: %s%s\n"), root, path);
			continue;
		}
		strcpy(filepath + rootlen, path);

		errors += check_file_exists(pkgname, filepath, &st);
	}

	if(!config->quiet) {
		printf(_n("%s: %jd total file, ", "%s: %jd total files, ",
					(unsigned long)filelist->count), pkgname, (intmax_t)filelist->count);
		printf(_n("%jd missing file\n", "%jd missing files\n",
					(unsigned long)errors), (intmax_t)errors);
	}

	return (errors != 0 ? 1 : 0);
}

/* vim: set ts=2 sw=2 noet: */
