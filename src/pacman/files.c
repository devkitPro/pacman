/*
 *  files.c
 *
 *  Copyright (c) 2015 Pacman Development Team <pacman-dev@archlinux.org>
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

#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "pacman.h"
#include "util.h"
#include "conf.h"
#include "package.h"


static int files_fileowner(alpm_list_t *syncs, alpm_list_t *targets) {
	int ret = 0;
	alpm_list_t *t;

	for(t = targets; t; t = alpm_list_next(t)) {
		char *filename = NULL, *f;
		int found = 0;
		alpm_list_t *s;
		size_t len;

		if((filename = strdup(t->data)) == NULL) {
			goto notfound;
		}

		len = strlen(filename);
		f = filename;
		while(len > 1 && f[0] == '/') {
			f = f + 1;
			len--;
		}

		for(s = syncs; s; s = alpm_list_next(s)) {
			alpm_list_t *p;
			alpm_db_t *repo = s->data;
			alpm_list_t *packages = alpm_db_get_pkgcache(repo);

			for(p = packages; p; p = alpm_list_next(p)) {
				alpm_pkg_t *pkg = p->data;
				alpm_filelist_t *files = alpm_pkg_get_files(pkg);

				if(alpm_filelist_contains(files, f)) {

					if(!config->quiet) {
						printf(_("%s is owned by %s/%s %s\n"), filename,
								alpm_db_get_name(repo), alpm_pkg_get_name(pkg),
								alpm_pkg_get_version(pkg));
					} else {
						printf("%s/%s\n", alpm_db_get_name(repo), alpm_pkg_get_name(pkg));
					}

					found = 1;
				}
			}
		}

		free(filename);

notfound:
		if(!found) {
			ret++;
		}
	}

	return 0;
}

static int files_search(alpm_list_t __attribute__((unused)) *syncs, alpm_list_t __attribute__((unused)) *targets) {
	return 0;
}

static int files_list(alpm_list_t *syncs, alpm_list_t *targets) {
	alpm_list_t *i, *j;
	int ret = 0, found = 0;

	if(targets != NULL) {
		for(i = targets; i; i = alpm_list_next(i)) {
			/* TODO: handle repo/pkg stype arguements */
			char *targ = i->data;
			
			for(j = syncs; j; j = alpm_list_next(j)) {
				alpm_pkg_t *pkg;
				alpm_db_t *db = j->data;
				if((pkg = alpm_db_get_pkg(db, targ)) != NULL) {
					found = 1;
					dump_pkg_files(pkg, config->quiet);
				}
			}
			if(!found) {
				pm_printf(ALPM_LOG_ERROR,
						_("package '%s' was not found\n"), targ);
				ret += 1;
			}
		}
	} else {
		for(i = syncs; i; i = alpm_list_next(i)) {
		alpm_db_t *db = i->data;

			for(j = alpm_db_get_pkgcache(db); j; j = alpm_list_next(j)) {
				alpm_pkg_t *pkg = j->data;
				dump_pkg_files(pkg, config->quiet);
			}
		}
	}

	return ret;
}


int pacman_files(alpm_list_t *targets)
{
	alpm_list_t *files_dbs = NULL;

	if(check_syncdbs(1, 0)) {
		return 1;
	}

	files_dbs = alpm_get_syncdbs(config->handle);

	if(config->op_s_sync) {
		/* grab a fresh package list */
		colon_printf(_("Synchronizing package databases...\n"));
		alpm_logaction(config->handle, PACMAN_CALLER_PREFIX,
				"synchronizing package lists\n");
		if(!sync_syncdbs(config->op_s_sync, files_dbs)) {
			return 1;
		}
	}

	if(targets == NULL && (config->op_s_search || config->op_q_owns)) {
		pm_printf(ALPM_LOG_ERROR, _("no targets specified (use -h for help)\n"));
		return 1;
	}

	/* determine the owner of a file */
	if(config->op_q_owns) {
		return files_fileowner(files_dbs, targets);
	}

	/* search for a file */
	if(config->op_s_search) {
		return files_search(files_dbs, targets);
	}

	/* get a listing of files in sync DBs */
	if(config->op_q_list) {
		return files_list(files_dbs, targets);
	}


	return 0;
}
