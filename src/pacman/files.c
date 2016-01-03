/*
 *  files.c
 *
 *  Copyright (c) 2015-2016 Pacman Development Team <pacman-dev@archlinux.org>
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
#include <regex.h>

/* pacman */
#include "pacman.h"
#include "util.h"
#include "conf.h"
#include "package.h"

static void print_line_machinereadable(alpm_db_t *db, alpm_pkg_t *pkg, char *filename)
{
	/* Fields are repo, pkgname, pkgver, filename separated with \0 */
	fputs(alpm_db_get_name(db), stdout);
	fputc(0, stdout);
	fputs(alpm_pkg_get_name(pkg), stdout);
	fputc(0, stdout);
	fputs(alpm_pkg_get_version(pkg), stdout);
	fputc(0, stdout);
	fputs(filename, stdout);
	fputs("\n", stdout);
}

static void dump_pkg_machinereadable(alpm_db_t *db, alpm_pkg_t *pkg)
{
	alpm_filelist_t *pkgfiles = alpm_pkg_get_files(pkg);
	for(size_t filenum = 0; filenum < pkgfiles->count; filenum++) {
		const alpm_file_t *file = pkgfiles->files + filenum;
		print_line_machinereadable(db, pkg, file->name);
	}
}

static int files_fileowner(alpm_list_t *syncs, alpm_list_t *targets) {
	int ret = 0;
	alpm_list_t *t;

	for(t = targets; t; t = alpm_list_next(t)) {
		char *filename = t->data;
		int found = 0;
		alpm_list_t *s;
		size_t len = strlen(filename);

		while(len > 1 && filename[0] == '/') {
			filename++;
			len--;
		}

		for(s = syncs; s; s = alpm_list_next(s)) {
			alpm_list_t *p;
			alpm_db_t *repo = s->data;
			alpm_list_t *packages = alpm_db_get_pkgcache(repo);

			for(p = packages; p; p = alpm_list_next(p)) {
				alpm_pkg_t *pkg = p->data;
				alpm_filelist_t *files = alpm_pkg_get_files(pkg);

				if(alpm_filelist_contains(files, filename)) {
					if(config->op_f_machinereadable) {
						print_line_machinereadable(repo, pkg, filename);
					} else if(!config->quiet) {
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

		if(!found) {
			ret++;
		}
	}

	return 0;
}

static int files_search(alpm_list_t *syncs, alpm_list_t *targets, int regex) {
	int ret = 0;
	alpm_list_t *t;
	const colstr_t *colstr = &config->colstr;

	for(t = targets; t; t = alpm_list_next(t)) {
		char *targ = t->data;
		alpm_list_t *s;
		int found = 0;
		regex_t reg;

		if(regex) {
			if(regcomp(&reg, targ, REG_EXTENDED | REG_NOSUB | REG_ICASE | REG_NEWLINE) != 0) {
				/* TODO: error message */
				goto notfound;
			}
		}

		for(s = syncs; s; s = alpm_list_next(s)) {
			alpm_list_t *p;
			alpm_db_t *repo = s->data;
			alpm_list_t *packages = alpm_db_get_pkgcache(repo);
			int m;

			for(p = packages; p; p = alpm_list_next(p)) {
				size_t f = 0;
				char* c;
				alpm_pkg_t *pkg = p->data;
				alpm_filelist_t *files = alpm_pkg_get_files(pkg);
				alpm_list_t *match = NULL;

				while(f < files->count) {
					c = strrchr(files->files[f].name, '/');
					if(c && *(c + 1)) {
						if(regex) {
							m = regexec(&reg, (c + 1), 0, 0, 0);
						} else {
							m = strcmp(c + 1, targ);
						}
						if(m == 0) {
							match = alpm_list_add(match, files->files[f].name);
							found = 1;
						}
					}
					f++;
				}

				if(match != NULL) {
					if(config->op_f_machinereadable) {
						alpm_list_t *ml;
						for(ml = match; ml; ml = alpm_list_next(ml)) {
							char *filename = ml->data;
							print_line_machinereadable(repo, pkg, filename);
						}
					} else if(config->quiet) {
						printf("%s/%s\n", alpm_db_get_name(repo), alpm_pkg_get_name(pkg));
					} else {
						alpm_list_t *ml;
						printf("%s%s/%s%s %s%s%s\n", colstr->repo, alpm_db_get_name(repo),
							colstr->title, alpm_pkg_get_name(pkg),
							colstr->version, alpm_pkg_get_version(pkg), colstr->nocolor);

						for(ml = match; ml; ml = alpm_list_next(ml)) {
							c = ml->data;
							printf("    %s\n", c);
						}
					}
					alpm_list_free(match);
				}
			}
		}

		if(regex) {
			regfree(&reg);
		}

notfound:
		if(!found) {
			ret++;
		}
	}

	return 0;
}

static void dump_file_list(alpm_pkg_t *pkg) {
	const char *pkgname;
	alpm_filelist_t *pkgfiles;
	size_t i;

	pkgname = alpm_pkg_get_name(pkg);
	pkgfiles = alpm_pkg_get_files(pkg);

	for(i = 0; i < pkgfiles->count; i++) {
		const alpm_file_t *file = pkgfiles->files + i;
		/* Regular: '<pkgname> <filepath>\n'
		 * Quiet  : '<filepath>\n'
		 */
		if(!config->quiet) {
			printf("%s%s%s ", config->colstr.title, pkgname, config->colstr.nocolor);
		}
		printf("%s\n", file->name);
	}

	fflush(stdout);
}

static int files_list(alpm_list_t *syncs, alpm_list_t *targets) {
	alpm_list_t *i, *j;
	int ret = 0;

	if(targets != NULL) {
		for(i = targets; i; i = alpm_list_next(i)) {
			int found = 0;
			char *targ = i->data;
			char *repo = NULL;
			char *c = strchr(targ, '/');

			if(c) {
				if(! *(c + 1)) {
					pm_printf(ALPM_LOG_ERROR,
						_("invalid package: '%s'\n"), targ);
					ret += 1;
					continue;
				}

				repo = strndup(targ, c - targ);
				targ = c + 1;
			}

			for(j = syncs; j; j = alpm_list_next(j)) {
				alpm_pkg_t *pkg;
				alpm_db_t *db = j->data;

				if(repo) {
					if(strcmp(alpm_db_get_name(db), repo) != 0) {
						continue;
					}
				}

				if((pkg = alpm_db_get_pkg(db, targ)) != NULL) {
					found = 1;
					if(config->op_f_machinereadable) {
						dump_pkg_machinereadable(db, pkg);
					} else {
						dump_file_list(pkg);
					}
					break;
				}
			}
			if(!found) {
				targ = i->data;
				pm_printf(ALPM_LOG_ERROR,
						_("package '%s' was not found\n"), targ);
				ret += 1;
			}
			free(repo);
		}
	} else {
		for(i = syncs; i; i = alpm_list_next(i)) {
		alpm_db_t *db = i->data;

			for(j = alpm_db_get_pkgcache(db); j; j = alpm_list_next(j)) {
				alpm_pkg_t *pkg = j->data;
				if(config->op_f_machinereadable) {
					dump_pkg_machinereadable(db, pkg);
				} else {
					dump_file_list(pkg);
				}
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

	if(targets == NULL && (config->op_q_owns | config->op_s_search)) {
		pm_printf(ALPM_LOG_ERROR, _("no targets specified (use -h for help)\n"));
		return 1;
	}

	/* determine the owner of a file */
	if(config->op_q_owns) {
		return files_fileowner(files_dbs, targets);
	}

	/* search for a file */
	if(config->op_s_search) {
		return files_search(files_dbs, targets, config->op_f_regex);
	}

	/* get a listing of files in sync DBs */
	if(config->op_q_list) {
		return files_list(files_dbs, targets);
	}

	if(targets != NULL) {
		pm_printf(ALPM_LOG_ERROR, _("no options specified (use -h for help)\n"));
		return 1;
	}

	return 0;
}

/* vim: set noet: */
