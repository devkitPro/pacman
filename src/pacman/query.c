/*
 *  query.c
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 *  USA.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "pacman.h"
#include "package.h"
#include "conf.h"
#include "util.h"

extern config_t *config;
extern pmdb_t *db_local;

static char *resolve_path(const char* file)
{
	char *str = NULL;

	str = calloc(PATH_MAX+1, sizeof(char));
	if(!str) {
		/* null hmmm.... */
		return(NULL);
	}

	if(!realpath(file, str)) {
		return(NULL);
	}

	return(str);
}


static int query_fileowner(alpm_list_t *targets)
{
	int ret = 0;
	alpm_list_t *t;

	if(targets == NULL) {
		fprintf(stderr, _("error: no file was specified for --owns\n"));
		return(1);
	}

	for(t = targets; t; t = alpm_list_next(t)) {
		int found = 0;
		char *filename = alpm_list_getdata(t);
		char *rpath;
		struct stat buf;
		alpm_list_t *i, *j;

		if(stat(filename, &buf) == -1) {
			fprintf(stderr, _("error: failed to read file '%s': %s\n"),
					filename, strerror(errno));
			ret++;
			continue;
		}

		if(S_ISDIR(buf.st_mode)) {
			fprintf(stderr, _("error: cannot determine ownership of a directory\n"));
			ret++;
			continue;
		}

		if(!(rpath = resolve_path(filename))) {
			fprintf(stderr, _("error: cannot determine real path for '%s': %s\n"),
					filename, strerror(errno));
			ret++;
			continue;
		}

		for(i = alpm_db_getpkgcache(db_local); i && !found; i = alpm_list_next(i)) {
			pmpkg_t *info = alpm_list_getdata(i);

			for(j = alpm_pkg_get_files(info); j && !found; j = alpm_list_next(j)) {
				char path[PATH_MAX], *ppath;
				snprintf(path, PATH_MAX, "%s%s",
				         alpm_option_get_root(), (const char *)alpm_list_getdata(j));

				ppath = resolve_path(path);

				if(ppath && strcmp(ppath, rpath) == 0) {
					printf(_("%s is owned by %s %s\n"), rpath,
					       alpm_pkg_get_name(info), alpm_pkg_get_version(info));
					found = 1;
				}
				free(ppath);
			}
		}
		if(!found) {
			fprintf(stderr, _("error: No package owns %s\n"), filename);
			ret++;
		}
		free(rpath);
	}

	return ret;
}

/* search the local database for a matching package */
static int query_search(alpm_list_t *targets)
{
	alpm_list_t *i, *searchlist;
	int freelist;

	/* if we have a targets list, search for packages matching it */
	if(targets) {
		searchlist = alpm_db_search(db_local, targets);
		freelist = 1;
	} else {
		searchlist = alpm_db_getpkgcache(db_local);
		freelist = 0;
	}
	if(searchlist == NULL) {
		return(1);
	}

	for(i = searchlist; i; i = alpm_list_next(i)) {
		char *group = NULL;
		alpm_list_t *grp;
		pmpkg_t *pkg = alpm_list_getdata(i);

		printf("local/%s %s", alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));

		/* print the package size with the output if ShowSize option set */
		if(config->showsize) {
			/* Convert byte size to MB */
			double mbsize = alpm_pkg_get_size(pkg) / (1024.0 * 1024.0);

			printf(" [%.2f MB]", mbsize);
		}

		if((grp = alpm_pkg_get_groups(pkg)) != NULL) {
			group = alpm_list_getdata(grp);
			printf(" (%s)", (char *)alpm_list_getdata(grp));
		}

		/* we need a newline and initial indent first */
		printf("\n    ");
		indentprint(alpm_pkg_get_desc(pkg), 4);
		printf("\n");
	}
		/* we only want to free if the list was a search list */
		if(freelist) {
			alpm_list_free(searchlist);
		}
	return(0);
}

static int query_group(alpm_list_t *targets)
{
	alpm_list_t *i, *j;
	char *package = NULL;
	int ret = 0;
	if(targets == NULL) {
		for(j = alpm_db_getgrpcache(db_local); j; j = alpm_list_next(j)) {
			pmgrp_t *grp = alpm_list_getdata(j);
			const alpm_list_t *p, *pkgnames;
			const char *grpname;

			grpname = alpm_grp_get_name(grp);
			pkgnames = alpm_grp_get_pkgs(grp);

			for(p = pkgnames; p; p = alpm_list_next(p)) {
				printf("%s %s\n", grpname, (char *)alpm_list_getdata(p));
			}
		}
	} else {
		for(i = targets; i; i = alpm_list_next(i)) {
			package = alpm_list_getdata(i);
			pmgrp_t *grp = alpm_db_readgrp(db_local, package);
			if(grp) {
				const alpm_list_t *p, *pkgnames = alpm_grp_get_pkgs(grp);
				for(p = pkgnames; p; p = alpm_list_next(p)) {
					printf("%s %s\n", package, (char *)alpm_list_getdata(p));
				}
			} else {
				fprintf(stderr, _("error: group \"%s\" was not found\n"), package);
				ret++;
			}
		}
	}
	return ret;
}

static int query_isfile(alpm_list_t *targets)
{
	int ret = 0;
	char *package = NULL;
	alpm_list_t *i;
	pmpkg_t *info = NULL;
	if(targets == NULL) {
		fprintf(stderr, _("error: no package file was specified for --file\n"));
		return(1);
	} else {
		for(i = targets; i; i = alpm_list_next(i)) {
			package = alpm_list_getdata(i);
			if(alpm_pkg_load(package, &info) == -1) {
				fprintf(stderr, _("error: failed to load package '%s' (%s)\n"),
						package, alpm_strerror(pm_errno));
				ret++;
				continue;
			}
			if(config->op_q_info) {
				dump_pkg_full(info, config->op_q_info);
			}
			if(config->op_q_list) {
				dump_pkg_files(info);
			}
			if(!config->op_q_info && !config->op_q_list) {
				printf("%s %s\n", alpm_pkg_get_name(info),
						alpm_pkg_get_version(info));
			}
			alpm_pkg_free(info);
			info = NULL;
		}
	}
	return(ret);
}

int pacman_query(alpm_list_t *targets)
{
	alpm_list_t *sync_dbs = NULL, *i, *j, *k;
	pmpkg_t *info = NULL;
	char *package = NULL;
	int ret = 0;

	if(config->op_q_search) {
		ret = query_search(targets);
		return(ret);
	}

	if(config->op_q_foreign) {
		sync_dbs = alpm_option_get_syncdbs();

		if(sync_dbs == NULL || alpm_list_count(sync_dbs) == 0) {
			fprintf(stderr, _("error: no usable package repositories configured.\n"));
			return(1);
		}
	}

	if(config->op_q_upgrade) {
		printf(_("Checking for package upgrades..."));
		alpm_list_t *syncpkgs;

		if((syncpkgs = alpm_db_get_upgrades()) != NULL) {
			display_targets(syncpkgs);
			return(0);
		} else {
			printf(_("no upgrades found"));
			return(1);
		}
	}

	/* looking for groups */
	if(config->group) {
		ret = query_group(targets);
		return(ret);
	}

	/* output info for a .tar.gz package */
	if(config->op_q_isfile) {
		ret = query_isfile(targets);
		return(ret);
	}

	/* determine the owner of a file */
	if(config->op_q_owns) {
		ret = query_fileowner(targets);
		return(ret);
	}

	/* find packages in the db */
	if(targets == NULL) {
		/* no target */
		for(i = alpm_db_getpkgcache(db_local); i; i = alpm_list_next(i)) {
			pmpkg_t *tmpp = alpm_list_getdata(i);
			const char *pkgname, *pkgver;

			pkgname = alpm_pkg_get_name(tmpp);
			pkgver = alpm_pkg_get_version(tmpp);

			if(config->op_q_list || config->op_q_orphans || config->op_q_foreign) {
				info = alpm_db_get_pkg(db_local, (char *)pkgname);
				if(info == NULL) {
					/* something weird happened */
					fprintf(stderr, _("error: package \"%s\" not found\n"), pkgname);
					ret++;
					continue;
				}
			}
			if(config->op_q_foreign) {
				int match = 0;
				for(j = sync_dbs; j; j = alpm_list_next(j)) {
					pmdb_t *db = (pmdb_t *)alpm_list_getdata(j);
					for(k = alpm_db_getpkgcache(db); k; k = alpm_list_next(k)) {
						pmpkg_t *pkg = alpm_list_getdata(k);
						if(strcmp(alpm_pkg_get_name(pkg), alpm_pkg_get_name(info)) == 0) {
							match = 1;
						}
					}
				}
				if(match==0) {
					printf("%s %s\n", pkgname, pkgver);
				}
			} else if(config->op_q_list) {
				dump_pkg_files(info);
			} else if(config->op_q_orphans) {
				if(alpm_pkg_get_requiredby(info) == NULL
						&& ((long)alpm_pkg_get_reason(info) == PM_PKG_REASON_DEPEND
							|| config->op_q_orphans > 1)) {
					printf("%s %s\n", pkgname, pkgver);
				}
			} else {
				printf("%s %s\n", pkgname, pkgver);
			}
		}
	} else {
		for(i = targets; i; i = alpm_list_next(i)) {
			package = alpm_list_getdata(i);
			info = alpm_db_get_pkg(db_local, package);
			if(info == NULL) {
				fprintf(stderr, _("error: package \"%s\" not found\n"), package);
				ret++;
				continue;
			}

			/* find a target */
			if(config->op_q_info) {
				dump_pkg_full(info, config->op_q_info);
			}
			if(config->op_q_list) {
				dump_pkg_files(info);
			}
			if(!config->op_q_info && !config->op_q_list) {
				printf("%s %s\n", alpm_pkg_get_name(info),
						alpm_pkg_get_version(info));
			}
			if(config->op_q_changelog) {
				char changelog[PATH_MAX];
				/* TODO should be done in the backend- no raw DB stuff up front */
				snprintf(changelog, PATH_MAX, "%s/%s/%s-%s/changelog",
				         alpm_option_get_dbpath(),
				         alpm_db_get_name(db_local),
				         alpm_pkg_get_name(info),
				         alpm_pkg_get_version(info));
				dump_pkg_changelog(changelog, alpm_pkg_get_name(info));
			}
		}
	}

	return(ret);
}

/* vim: set ts=2 sw=2 noet: */
