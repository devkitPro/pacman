/*
 *  query.c
 *
 *  Copyright (c) 2002-2008 by Judd Vinet <jvinet@zeroflux.org>
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

extern pmdb_t *db_local;

static char *resolve_path(const char* file)
{
	char *str = NULL;

	str = calloc(PATH_MAX+1, sizeof(char));
	if(!str) {
		return(NULL);
	}

	if(!realpath(file, str)) {
		free(str);
		return(NULL);
	}

	return(str);
}

static int query_fileowner(alpm_list_t *targets)
{
	int ret = 0;
	alpm_list_t *t;

	/* This code is here for safety only */
	if(targets == NULL) {
		pm_fprintf(stderr, PM_LOG_ERROR, _("no file was specified for --owns\n"));
		return(1);
	}

	for(t = targets; t; t = alpm_list_next(t)) {
		int found = 0;
		char *filename = alpm_list_getdata(t);
		char *bname, *dname, *rpath;
		const char *root;
		struct stat buf;
		alpm_list_t *i, *j;

		if(stat(filename, &buf) == -1) {
			pm_fprintf(stderr, PM_LOG_ERROR, _("failed to read file '%s': %s\n"),
					filename, strerror(errno));
			ret++;
			continue;
		}

		if(S_ISDIR(buf.st_mode)) {
			pm_fprintf(stderr, PM_LOG_ERROR,
				_("cannot determine ownership of a directory\n"));
			ret++;
			continue;
		}

		bname = mbasename(filename);
		dname = mdirname(filename);
		rpath = resolve_path(dname);
		free(dname);

		if(!rpath) {
			pm_fprintf(stderr, PM_LOG_ERROR, _("cannot determine real path for '%s': %s\n"),
					filename, strerror(errno));
			free(rpath);
			ret++;
			continue;
		}

		root = alpm_option_get_root();

		for(i = alpm_db_getpkgcache(db_local); i && !found; i = alpm_list_next(i)) {
			pmpkg_t *info = alpm_list_getdata(i);

			for(j = alpm_pkg_get_files(info); j && !found; j = alpm_list_next(j)) {
				char path[PATH_MAX], *ppath, *pdname;
				snprintf(path, PATH_MAX, "%s%s",
						root, (const char *)alpm_list_getdata(j));

				/* avoid the costly resolve_path usage if the basenames don't match */
				if(strcmp(mbasename(path), bname) != 0) {
					continue;
				}

				pdname = mdirname(path);
				ppath = resolve_path(pdname);
				free(pdname);

				if(ppath && strcmp(ppath, rpath) == 0) {
					if (!config->quiet) {
						printf(_("%s is owned by %s %s\n"), filename,
								alpm_pkg_get_name(info), alpm_pkg_get_version(info));
					} else {
						printf("%s %s\n", alpm_pkg_get_name(info),
								alpm_pkg_get_version(info));
					}
					found = 1;
				}
				free(ppath);
			}
		}
		if(!found) {
			pm_fprintf(stderr, PM_LOG_ERROR, _("No package owns %s\n"), filename);
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
		alpm_list_t *grp;
		pmpkg_t *pkg = alpm_list_getdata(i);

		if (!config->quiet) {
			printf("local/%s %s", alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
		} else {
			printf("%s", alpm_pkg_get_name(pkg));
		}

		/* print the package size with the output if ShowSize option set */
		if(config->showsize) {
			/* Convert byte size to MB */
			double mbsize = (double)alpm_pkg_get_size(pkg) / (1024.0 * 1024.0);

			printf(" [%.2f MB]", mbsize);
		}


		if (!config->quiet) {
			if((grp = alpm_pkg_get_groups(pkg)) != NULL) {
				alpm_list_t *k;
				printf(" (");
				for(k = grp; k; k = alpm_list_next(k)) {
					const char *group = alpm_list_getdata(k);
					printf("%s", group);
					if(alpm_list_next(k)) {
						/* only print a spacer if there are more groups */
						printf(" ");
					}
				}
				printf(")");
			}

			/* we need a newline and initial indent first */
			printf("\n    ");
			indentprint(alpm_pkg_get_desc(pkg), 4);
		}
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
	char *grpname = NULL;
	int ret = 0;
	if(targets == NULL) {
		for(j = alpm_db_getgrpcache(db_local); j; j = alpm_list_next(j)) {
			pmgrp_t *grp = alpm_list_getdata(j);
			const alpm_list_t *p, *packages;
			const char *grpname;

			grpname = alpm_grp_get_name(grp);
			packages = alpm_grp_get_pkgs(grp);

			for(p = packages; p; p = alpm_list_next(p)) {
				printf("%s %s\n", grpname, alpm_pkg_get_name(alpm_list_getdata(p)));
			}
		}
	} else {
		for(i = targets; i; i = alpm_list_next(i)) {
			pmgrp_t *grp;
			grpname = alpm_list_getdata(i);
			grp = alpm_db_readgrp(db_local, grpname);
			if(grp) {
				const alpm_list_t *p, *packages = alpm_grp_get_pkgs(grp);
				for(p = packages; p; p = alpm_list_next(p)) {
					if(!config->quiet) {
						printf("%s %s\n", grpname,
								alpm_pkg_get_name(alpm_list_getdata(p)));
					} else {
						printf("%s\n", alpm_pkg_get_name(alpm_list_getdata(p)));
					}
				}
			} else {
				pm_fprintf(stderr, PM_LOG_ERROR, _("group \"%s\" was not found\n"), grpname);
				ret++;
			}
		}
	}
	return ret;
}

static int query_upgrades(void)
{
	alpm_list_t *syncpkgs = NULL;
	printf(_("Checking for package upgrades... \n"));

	alpm_list_t *syncdbs = alpm_option_get_syncdbs();
	if(alpm_sync_sysupgrade(db_local, syncdbs, &syncpkgs) == -1) {
		return(-1);
	}
	if(syncpkgs) {
		display_synctargets(syncpkgs);
		return(0);
	}

	printf(_("no upgrades found.\n"));
	return(1);
}

static int is_foreign(pmpkg_t *pkg)
{
	const char *pkgname = alpm_pkg_get_name(pkg);
	alpm_list_t *j;
	alpm_list_t *sync_dbs = alpm_option_get_syncdbs();

	int match = 0;
	for(j = sync_dbs; j; j = alpm_list_next(j)) {
		pmdb_t *db = alpm_list_getdata(j);
		pmpkg_t *findpkg = alpm_db_get_pkg(db, pkgname);
		if(findpkg) {
			match = 1;
			break;
		}
	}
	if(match == 0) {
		return(1);
	}
	return(0);
}

static int is_unrequired(pmpkg_t *pkg)
{
	alpm_list_t *requiredby = alpm_pkg_compute_requiredby(pkg);
	if(requiredby == NULL) {
		return(1);
	}
	FREELIST(requiredby);
	return(0);
}

static int filter(pmpkg_t *pkg)
{
	/* check if this package was explicitly installed */
	if(config->op_q_explicit &&
			alpm_pkg_get_reason(pkg) != PM_PKG_REASON_EXPLICIT) {
		return(0);
	}
	/* check if this package was installed as a dependency */
	if(config->op_q_deps &&
			alpm_pkg_get_reason(pkg) != PM_PKG_REASON_DEPEND) {
		return(0);
	}
	/* check if this pkg isn't in a sync DB */
	if(config->op_q_foreign && !is_foreign(pkg)) {
		return(0);
	}
	/* check if this pkg is unrequired */
	if(config->op_q_unrequired && !is_unrequired(pkg)) {
		return(0);
	}
	return(1);
}

static void display(pmpkg_t *pkg)
{
	if(config->op_q_info) {
		if(config->op_q_isfile) {
			/* omit info that isn't applicable for a file package */
			dump_pkg_full(pkg, 0);
		} else {
			dump_pkg_full(pkg, config->op_q_info);
		}
	}
	if(config->op_q_list) {
		dump_pkg_files(pkg);
	}
	if(config->op_q_changelog) {
		dump_pkg_changelog(pkg);
	}
	if(!config->op_q_info && !config->op_q_list && !config->op_q_changelog) {
		if (!config->quiet) {
			printf("%s %s\n", alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
		} else {
			printf("%s\n", alpm_pkg_get_name(pkg));
		}
	}
}

int pacman_query(alpm_list_t *targets)
{
	int ret = 0;
	alpm_list_t *i;

	/* First: operations that do not require targets */

	/* search for a package */
	if(config->op_q_search) {
		ret = query_search(targets);
		return(ret);
	}

	/* check for package upgrades */
	if(config->op_q_upgrade) {
		ret = query_upgrades();
		return(ret);
	}

	/* looking for groups */
	if(config->group) {
		ret = query_group(targets);
		return(ret);
	}

	if(config->op_q_foreign) {
		/* ensure we have at least one valid sync db set up */
		alpm_list_t *sync_dbs = alpm_option_get_syncdbs();
		if(sync_dbs == NULL || alpm_list_count(sync_dbs) == 0) {
			pm_printf(PM_LOG_ERROR, _("no usable package repositories configured.\n"));
			return(-1);
		}
	}

	/* operations on all packages in the local DB
	 * valid: no-op (plain -Q), list, info
	 * invalid: isfile, owns */
	if(targets == NULL) {
		if(config->op_q_isfile || config->op_q_owns) {
			pm_printf(PM_LOG_ERROR, _("no targets specified (use -h for help)\n"));
			return(1);
		}

		for(i = alpm_db_getpkgcache(db_local); i; i = alpm_list_next(i)) {
			pmpkg_t *pkg = alpm_list_getdata(i);
			if(filter(pkg)) {
				display(pkg);
			}
		}
		return(0);
	}

	/* Second: operations that require target(s) */

	/* determine the owner of a file */
	if(config->op_q_owns) {
		ret = query_fileowner(targets);
		return(ret);
	}

	/* operations on named packages in the local DB
	 * valid: no-op (plain -Q), list, info */
	for(i = targets; i; i = alpm_list_next(i)) {
		char *strname = alpm_list_getdata(i);
		pmpkg_t *pkg = NULL;

		if(config->op_q_isfile) {
			alpm_pkg_load(strname, 1, &pkg);
		} else {
			pkg = alpm_db_get_pkg(db_local, strname);
		}

		if(pkg == NULL) {
			pm_fprintf(stderr, PM_LOG_ERROR, _("package \"%s\" not found\n"), strname);
			ret++;
			continue;
		}

		if(filter(pkg)) {
			display(pkg);
		}

		if(config->op_q_isfile) {
			alpm_pkg_free(pkg);
			pkg = NULL;
		}
	}

	return(ret);
}

/* vim: set ts=2 sw=2 noet: */
