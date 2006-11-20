/*
 *  query.c
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
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <libintl.h>

#include <alpm.h>
/* pacman */
#include "list.h"
#include "package.h"
#include "query.h"
#include "log.h"
#include "conf.h"
#include "sync.h"
#include "util.h"

extern config_t *config;
extern pmdb_t *db_local;
extern list_t *pmc_syncs;

static int query_fileowner(pmdb_t *db, char *filename)
{
	struct stat buf;
	int gotcha = 0;
	char rpath[PATH_MAX];
	pmlist_t *lp;
	const char *root;

	if(db == NULL) {
		return(0);
	}
	if(filename == NULL || strlen(filename) == 0) {
		ERR(NL, _("no file was specified for --owns\n"));
		return(1);
	}

	if(stat(filename, &buf) == -1 || S_ISDIR(buf.st_mode) || realpath(filename, rpath) == NULL) {
		ERR(NL, _("%s is not a file.\n"), filename);
		return(1);
	}

	root = alpm_option_get_root();

	for(lp = alpm_db_getpkgcache(db); lp && !gotcha; lp = alpm_list_next(lp)) {
		pmpkg_t *info;
		pmlist_t *i;

		info = alpm_list_getdata(lp);

		for(i = alpm_pkg_get_files(info); i && !gotcha; i = alpm_list_next(i)) {
			char path[PATH_MAX];

			char *filename = (char *)alpm_list_getdata(i);
			snprintf(path, PATH_MAX, "%s%s", root, filename);
			if(!strcmp(path, rpath)) {
				printf(_("%s is owned by %s %s\n"), path, (char *)alpm_pkg_get_name(info),
				       (char *)alpm_pkg_get_version(info));
				gotcha = 1;
				break;
			}
		}
	}
	if(!gotcha) {
		ERR(NL, _("No package owns %s\n"), filename);
		return(1);
	}

	return(0);
}

int pacman_query(list_t *targets)
{
	pmpkg_t *info = NULL;
	list_t *targ;
	list_t *i;
	pmlist_t *j, *ret;
	char *package = NULL;
	int done = 0;

	if(config->op_q_search) {
		for(i = targets; i; i = i->next) {
			alpm_option_add_needle(i->data);
		}
		ret = alpm_db_search(db_local);
		if(ret == NULL) {
			return(1);
		}
		for(j = ret; j; j = alpm_list_next(j)) {
			pmpkg_t *pkg = alpm_list_getdata(j);

			printf("local/%s/%s %s\n    ",
					(char *)alpm_list_getdata(alpm_pkg_get_groups(pkg)),
					alpm_pkg_get_name(pkg),
					alpm_pkg_get_version(pkg));
			indentprint(alpm_pkg_get_desc(pkg), 4);
			printf("\n");
		}
		alpm_list_free_outer(ret);
		return(0);
	}

	if(config->op_q_foreign) {
		if(pmc_syncs == NULL || !list_count(pmc_syncs)) {
			ERR(NL, _("no usable package repositories configured.\n"));
			return(1);
		}
	}

	for(targ = targets; !done; targ = (targ ? targ->next : NULL)) {
		if(targets == NULL) {
			done = 1;
		} else {
			if(targ->next == NULL) {
				done = 1;
			}
			package = targ->data;
		}

		/* looking for groups */
		if(config->group) {
			pmlist_t *lp;
			if(targets == NULL) {
				for(lp = alpm_db_getgrpcache(db_local); lp; lp = alpm_list_next(lp)) {
					pmgrp_t *grp = alpm_list_getdata(lp);
					pmlist_t *i, *pkgnames;
					const char *grpname;

					grpname = alpm_grp_get_name(grp);
					pkgnames = alpm_grp_get_packages(grp);

					for(i = pkgnames; i; i = alpm_list_next(i)) {
						MSG(NL, "%s %s\n", grpname, (char *)alpm_list_getdata(i));
					}
				}
			} else {
				pmgrp_t *grp = alpm_db_readgrp(db_local, package);
				if(grp) {
					pmlist_t *i, *pkgnames = alpm_grp_get_packages(grp);
					for(i = pkgnames; i; i = alpm_list_next(i)) {
						MSG(NL, "%s %s\n", package, (char *)alpm_list_getdata(i));
					}
				} else {
					ERR(NL, _("group \"%s\" was not found\n"), package);
					return(2);
				}
			}
			continue;
		}

		/* output info for a .tar.gz package */
		if(config->op_q_isfile) {
			if(package == NULL) {
				ERR(NL, _("no package file was specified for --file\n"));
				return(1);
			}
			if(alpm_pkg_load(package, &info) == -1) {
				ERR(NL, _("failed to load package '%s' (%s)\n"), package, alpm_strerror(pm_errno));
				return(1);
			}
			if(config->op_q_info) {
				dump_pkg_full(info, 0);
				MSG(NL, "\n");
			}
			if(config->op_q_list) {
				dump_pkg_files(info);
			}
			if(!config->op_q_info && !config->op_q_list) {
				MSG(NL, "%s %s\n", alpm_pkg_get_name(info),
				                   alpm_pkg_get_version(info));
			}
			FREEPKG(info);
			continue;
		}

		/* determine the owner of a file */
		if(config->op_q_owns) {
			return(query_fileowner(db_local, package));
		}

		/* find packages in the db */
		if(package == NULL) {
			pmlist_t *lp;
			/* no target */
			for(lp = alpm_db_getpkgcache(db_local); lp; lp = alpm_list_next(lp)) {
				pmpkg_t *tmpp = alpm_list_getdata(lp);
				const char *pkgname, *pkgver;

				pkgname = alpm_pkg_get_name(tmpp);
				pkgver = alpm_pkg_get_version(tmpp);

				if(config->op_q_list || config->op_q_orphans || config->op_q_foreign) {
					info = alpm_db_readpkg(db_local, (char *)pkgname);
					if(info == NULL) {
						/* something weird happened */
						ERR(NL, _("package \"%s\" not found\n"), pkgname);
						return(1);
					}
					if(config->op_q_foreign) {
						int match = 0;
						for(i = pmc_syncs; i; i = i->next) {
							sync_t *sync = (sync_t *)i->data;
							for(j = alpm_db_getpkgcache(sync->db); j; j = alpm_list_next(j)) {
								pmpkg_t *pkg = alpm_list_getdata(j);
								char *haystack;
								char *needle;
								haystack = strdup(alpm_pkg_get_name(pkg));
								needle = strdup(alpm_pkg_get_name(info));
								if(!strcmp(haystack, needle)) {
									match = 1;
								}
								FREE(haystack);
								FREE(needle);
							}
						}
						if(match==0) {
							MSG(NL, "%s %s\n", pkgname, pkgver);
						}
					}
					if(config->op_q_list) {
						dump_pkg_files(info);
					}
					if(config->op_q_orphans) {
						if(alpm_pkg_get_requiredby(info) == NULL
						   && (long)alpm_pkg_get_reason(info) == PM_PKG_REASON_DEPEND) {
							MSG(NL, "%s %s\n", pkgname, pkgver);
						}
					} 
				} else {
					MSG(NL, "%s %s\n", pkgname, pkgver);
				}
			}
		} else {
			const char *pkgname = NULL, *pkgver = NULL;
		  char changelog[PATH_MAX];

			info = alpm_db_readpkg(db_local, package);
			if(info == NULL) {
				ERR(NL, _("package \"%s\" not found\n"), package);
				return(2);
			}

			/* find a target */
			if(config->op_q_changelog || config->op_q_info || config->op_q_list) {
				if(config->op_q_changelog) {
					const char *dbpath;
					dbpath = alpm_option_get_dbpath();
					snprintf(changelog, PATH_MAX, "%s%s/%s/%s-%s/changelog",
						config->root, dbpath,
						alpm_db_get_name(db_local),
						alpm_pkg_get_name(info),
						alpm_pkg_get_version(info));
					dump_pkg_changelog(changelog, alpm_pkg_get_name(info));
				}
				if(config->op_q_info) {
					dump_pkg_full(info, config->op_q_info);
				}
				if(config->op_q_list) {
					dump_pkg_files(info);
				}
			} else if(config->op_q_orphans) {
					if(alpm_pkg_get_requiredby(info) == NULL) {
						MSG(NL, "%s %s\n", pkgname, pkgver);
					}
			} else {
				pkgname = alpm_pkg_get_name(info);
				pkgver = alpm_pkg_get_version(info);
				MSG(NL, "%s %s\n", pkgname, pkgver);
			}
		}
	}

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
