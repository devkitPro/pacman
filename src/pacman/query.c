/*
 *  query.c
 * 
 *  Copyright (c) 2002-2005 by Judd Vinet <jvinet@zeroflux.org>
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

#include <alpm.h>
/* pacman */
#include "list.h"
#include "package.h"
#include "db.h"
#include "query.h"
#include "log.h"

extern unsigned short pmo_q_isfile;
extern unsigned short pmo_q_info;
extern unsigned short pmo_q_list;
extern unsigned short pmo_q_orphans;
extern unsigned short pmo_q_owns;
extern unsigned short pmo_q_search;
extern unsigned short pmo_group;
extern PM_DB *db_local;

static int query_fileowner(PM_DB *db, char *filename)
{
	struct stat buf;
	int gotcha = 0;
	char rpath[PATH_MAX];
	PM_LIST *lp;
	char *root;

	if(db == NULL) {
		return(0);
	}
	if(filename == NULL || strlen(filename) == 0) {
		fprintf(stderr, "error: no file was specified for --owns\n");
		return(1);
	}

	if(stat(filename, &buf) == -1 || S_ISDIR(buf.st_mode) || realpath(filename, rpath) == NULL) {
		fprintf(stderr, "error: %s is not a file.\n", filename);
		return(1);
	}

	alpm_get_option(PM_OPT_ROOT, (long *)&root);

	for(lp = alpm_db_getpkgcache(db); lp && !gotcha; lp = alpm_list_next(lp)) {
		PM_PKG *info;
		char *pkgname;
		PM_LIST *i;

		pkgname = alpm_pkg_getinfo(alpm_list_getdata(lp), PM_PKG_NAME);

		info = alpm_db_readpkg(db, pkgname);
		if(info == NULL) {
			fprintf(stderr, "error: package %s not found\n", pkgname);
			return(1);
		}

		for(i = alpm_pkg_getinfo(info, PM_PKG_FILES); i && !gotcha; i = alpm_list_next(i)) {
			char path[PATH_MAX];

			snprintf(path, PATH_MAX, "%s%s", root, (char *)alpm_list_getdata(i));
			if(!strcmp(path, rpath)) {
				printf("%s is owned by %s %s\n", filename, pkgname,
				       (char *)alpm_pkg_getinfo(info, PM_PKG_VERSION));
				gotcha = 1;
				break;
			}
		}
	}
	if(!gotcha) {
		fprintf(stderr, "No package owns %s\n", filename);
		return(1);
	}

	return(0);
}

int pacman_query(list_t *targets)
{
	PM_PKG *info = NULL;
	list_t *targ;
	char *package = NULL;
	int done = 0;

	if(pmo_q_search) {
		for(targ = targets; targ; targ = targ->next) {
			db_search(db_local, "local", targ->data);
		}
		return(0);
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
		if(pmo_group) {
			PM_LIST *lp;
			if(targets == NULL) {
				for(lp = alpm_db_getgrpcache(db_local); lp; lp = alpm_list_next(lp)) {
					PM_GRP *grp = alpm_list_getdata(lp);
					PM_LIST *i, *pkgnames;
					char *grpname;

					grpname = alpm_grp_getinfo(grp, PM_GRP_NAME);
					pkgnames = alpm_grp_getinfo(grp, PM_GRP_PKGNAMES);

					for(i = pkgnames; i; i = alpm_list_next(i)) {
						MSG(NL, "%s %s\n", grpname, (char *)alpm_list_getdata(i));
					}
				}
			} else {
				PM_GRP *grp = alpm_db_readgrp(db_local, package);
				if(grp) {
					PM_LIST *i, *pkgnames = alpm_grp_getinfo(grp, PM_GRP_PKGNAMES);
					for(i = pkgnames; i; i = alpm_list_next(i)) {
						MSG(NL, "%s %s\n", package, (char *)alpm_list_getdata(i));
					}
				} else {
					ERR(NL, "group \"%s\" was not found\n", package);
					return(2);
				}
			}
			continue;
		}

		/* output info for a .tar.gz package */
		if(pmo_q_isfile) {
			if(package == NULL) {
				ERR(NL, "no package file was specified for --file\n");
				return(1);
			}
			if(alpm_pkg_load(package, &info) == -1) {
				ERR(NL, "failed to load package '%s' (%s)\n", package, alpm_strerror(pm_errno));
				return(1);
			}
			if(pmo_q_info) {
				dump_pkg_full(info, 0);
				MSG(NL, "\n");
			}
			if(pmo_q_list) {
				dump_pkg_files(info);
			}
			if(!pmo_q_info && !pmo_q_list) {
				MSG(NL, "%s %s\n", (char *)alpm_pkg_getinfo(info, PM_PKG_NAME),
				                   (char *)alpm_pkg_getinfo(info, PM_PKG_VERSION));
			}
			FREEPKG(info);
			continue;
		}

		/* determine the owner of a file */
		if(pmo_q_owns) {
			return(query_fileowner(db_local, package));
		}

		/* find packages in the db */
		if(package == NULL) {
			PM_LIST *lp;
			/* no target */
			for(lp = alpm_db_getpkgcache(db_local); lp; lp = alpm_list_next(lp)) {
				PM_PKG *tmpp = alpm_list_getdata(lp);
				char *pkgname, *pkgver;

				pkgname = alpm_pkg_getinfo(tmpp, PM_PKG_NAME);
				pkgver = alpm_pkg_getinfo(tmpp, PM_PKG_VERSION);

				if(pmo_q_list || pmo_q_orphans) {
					info = alpm_db_readpkg(db_local, pkgname);
					if(info == NULL) {
						/* something weird happened */
						ERR(NL, "package \"%s\" not found\n", pkgname);
						return(1);
					}
					if(pmo_q_list) {
						dump_pkg_files(info);
					}
					if(pmo_q_orphans) {
						if(alpm_pkg_getinfo(info, PM_PKG_REQUIREDBY) == NULL
						   && (int)alpm_pkg_getinfo(info, PM_PKG_REASON) == PM_PKG_REASON_EXPLICIT) {
							MSG(NL, "%s %s\n", pkgname, pkgver);
						}
					} 
				} else {
					MSG(NL, "%s %s\n", pkgname, pkgver);
				}
			}
		} else {
			char *pkgname, *pkgver;

			info = alpm_db_readpkg(db_local, package);
			if(info == NULL) {
				ERR(NL, "package \"%s\" not found\n", package);
				return(2);
			}

			/* find a target */
			if(pmo_q_info || pmo_q_list) {
				if(pmo_q_info) {
					dump_pkg_full(info, pmo_q_info);
				}
				if(pmo_q_list) {
					dump_pkg_files(info);
				}
			} else if(pmo_q_orphans) {
					if(alpm_pkg_getinfo(info, PM_PKG_REQUIREDBY) == NULL) {
						MSG(NL, "%s %s\n", pkgname, pkgver);
					}
			} else {
				pkgname = alpm_pkg_getinfo(info, PM_PKG_NAME);
				pkgver = alpm_pkg_getinfo(info, PM_PKG_VERSION);
				MSG(NL, "%s %s\n", pkgname, pkgver);
			}
		}
	}

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
