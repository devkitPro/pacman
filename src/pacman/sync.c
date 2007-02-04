/*
 *  sync.c
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

#if defined(__APPLE__) || defined(__OpenBSD__)
#include <sys/syslimits.h>
#include <sys/stat.h>
#endif

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <libintl.h>
#ifdef CYGWIN
#include <limits.h> /* PATH_MAX */
#endif

#include <alpm.h>
#include <alpm_list.h>
#include <download.h> /* downloadLastErrString */
/* pacman */
#include "util.h"
#include "log.h"
#include "downloadprog.h"
#include "package.h"
#include "trans.h"
#include "sync.h"
#include "conf.h"

extern config_t *config;

/* splits package name into its respective parts */
static int split_pkgname(char *target, char *name, char *version)
{
	char tmp[512];
	char *p, *q;

	if(target == NULL) {
		return(-1);
	}

	/* trim path name (if any) */
	if((p = strrchr(target, '/')) == NULL) {
		p = target;
	} else {
		p++;
	}
	strncpy(tmp, p, 512);
	/* trim file extension (if any) */
	if((p = strstr(tmp, PM_EXT_PKG))) {
		*p = '\0';
	}
	/* trim architecture */
	if((p = alpm_pkg_name_hasarch(tmp))) {
		*p = '\0';
	}

	p = tmp + strlen(tmp);

	for(q = --p; *q && *q != '-'; q--);
	if(*q != '-' || q == tmp) {
		return(-1);
	}
	for(p = --q; *p && *p != '-'; p--);
	if(*p != '-' || p == tmp) {
		return(-1);
	}
	strncpy(version, p+1, 64);
	*p = '\0';

	strncpy(name, tmp, 256);

	return(0);
}

static int sync_cleancache(int level)
{
	const char *root, *cachedir;
	char dirpath[PATH_MAX];

	root = alpm_option_get_root();
	cachedir = alpm_option_get_cachedir();

	snprintf(dirpath, PATH_MAX, "%s%s", root, cachedir);

	if(level == 1) {
		/* incomplete cleanup: we keep latest packages and partial downloads */
		DIR *dir;
		struct dirent *ent;
		alpm_list_t *cache = NULL, *clean = NULL, *i, *j;

		if(!yesno(_("Do you want to remove old packages from cache? [Y/n] ")))
			return(0);
		MSG(NL, _("removing old packages from cache... "));
		dir = opendir(dirpath);
		if(dir == NULL) {
			ERR(NL, _("could not access cache directory\n"));
			return(1);
		}
		rewinddir(dir);
		while((ent = readdir(dir)) != NULL) {
			if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
				continue;
			}
			cache = alpm_list_add(cache, strdup(ent->d_name));
		}
		closedir(dir);

		for(i = cache; i; i = alpm_list_next(i)) {
			char *str = alpm_list_getdata(i);
			char name[256], version[64];

			if(strstr(str, PM_EXT_PKG) == NULL) {
				clean = alpm_list_add(clean, strdup(str));
				continue;
			}
			/* we keep partially downloaded files */
			if(strstr(str, PM_EXT_PKG ".part")) {
				continue;
			}
			if(split_pkgname(str, name, version) != 0) {
				clean = alpm_list_add(clean, strdup(str));
				continue;
			}
			for(j = alpm_list_next(i); j; j = alpm_list_next(j)) {
				char *s = alpm_list_getdata(j);
				char n[256], v[64];

				if(strstr(s, PM_EXT_PKG) == NULL) {
					continue;
				}
				if(strstr(s, PM_EXT_PKG ".part")) {
					continue;
				}
				if(split_pkgname(s, n, v) != 0) {
					continue;
				}
				/* TODO Do not remove the currently installed version EITHER */
				if(!strcmp(name, n)) {
					char *ptr = (alpm_pkg_vercmp(version, v) < 0) ? str : s;
					if(!alpm_list_find_str(clean, ptr)) {
						clean = alpm_list_add(clean, strdup(ptr));
					}
				}
			}
		}
		FREELIST(cache);

		for(i = clean; i; i = alpm_list_next(i)) {
			char path[PATH_MAX];

			snprintf(path, PATH_MAX, "%s/%s", dirpath, (char *)alpm_list_getdata(i));
			unlink(path);
		}
		FREELIST(clean);
	} else {
		/* full cleanup */
		if(!yesno(_("Do you want to remove all packages from cache? [Y/n] ")))
			return(0);
		MSG(NL, _("removing all packages from cache... "));

		if(rmrf(dirpath)) {
			ERR(NL, _("could not remove cache directory\n"));
			return(1);
		}

		if(makepath(dirpath)) {
			ERR(NL, _("could not create new cache directory\n"));
			return(1);
		}
	}

	MSG(CL, _("done.\n"));
	return(0);
}

static int sync_synctree(int level, alpm_list_t *syncs)
{
	alpm_list_t *i;
	int success = 0, ret;

	for(i = syncs; i; i = alpm_list_next(i)) {
		pmdb_t *db = alpm_list_getdata(i);

		ret = alpm_db_update((level < 2 ? 0 : 1), db);
		if(ret < 0) {
			if(pm_errno == PM_ERR_DB_SYNC) {
				/* use libdownload error */
				/* TODO breaking abstraction barrier here?
				 *			pacman -> libalpm -> libdownload
				 *
				 * Yes.  This will be here until we add a nice pacman "pm_errstr" or
				 * something, OR add all libdownload error codes into the pm_error enum
				 */
				ERR(NL, _("failed to synchronize %s: %s\n"), alpm_db_get_name(db), downloadLastErrString);
			} else {
				ERR(NL, _("failed to update %s (%s)\n"), alpm_db_get_name(db), alpm_strerror(pm_errno));
			}
		} else if(ret == 1) {
			MSG(NL, _(" %s is up to date\n"), alpm_db_get_name(db));
			success++;
		} else {
			success++;
		}
	}

	/* We should always succeed if at least one DB was upgraded - we may possibly
	 * fail later with unresolved deps, but that should be rare, and would be
	 * expected
	 */
	return(success > 0);
}

static int sync_search(alpm_list_t *syncs, alpm_list_t *targets)
{
	alpm_list_t *i, *j, *ret;

	for(i = targets; i; i = alpm_list_next(i)) {
		alpm_option_add_needle(alpm_list_getdata(i));
	}

	for(i = syncs; i; i = alpm_list_next(i)) {
		pmdb_t *db = (pmdb_t *)alpm_list_getdata(i);
		if(targets) {
			ret = alpm_db_search(db);
			if(ret == NULL) {
				continue;
			}
			for(j = ret; j; j = alpm_list_next(j)) {
				char *group = NULL;
				alpm_list_t *grp;
				pmpkg_t *pkg = alpm_list_getdata(j);

				printf("%s/%s %s", alpm_db_get_name(db), alpm_pkg_get_name(pkg),
						   alpm_pkg_get_version(pkg));

				if((grp = alpm_pkg_get_groups(pkg)) != NULL) {
						group = alpm_list_getdata(grp);
						printf(" (%s)\n    ", (char *)alpm_list_getdata(grp));
				} else {
					printf("\n    ");
				}
				
				indentprint(alpm_pkg_get_desc(pkg), 4);
				printf("\n");
			}
			alpm_list_free(ret);
		} else {
			for(j = alpm_db_getpkgcache(db); j; j = alpm_list_next(j)) {
				pmpkg_t *pkg = alpm_list_getdata(j);

				MSG(NL, "%s/%s %s\n    ", alpm_db_get_name(db), alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
				indentprint(alpm_pkg_get_desc(pkg), 4);
				MSG(NL, "\n");
			}
		}
	}

	return(0);
}

static int sync_group(int level, alpm_list_t *syncs, alpm_list_t *targets)
{
	alpm_list_t *i, *j;

	if(targets) {
		for(i = targets; i; i = alpm_list_next(i)) {
			char *grpname = alpm_list_getdata(i);
			for(j = syncs; j; j = alpm_list_next(j)) {
				pmdb_t *db = alpm_list_getdata(j);
				pmgrp_t *grp = alpm_db_readgrp(db, grpname);

				if(grp) {
					/* TODO this should be a lot cleaner, why two outputs? */
					MSG(NL, "%s\n", (char *)alpm_grp_get_name(grp));
					list_display("   ", alpm_grp_get_packages(grp));
				}
			}
		}
	} else {
		for(i = syncs; i; i = alpm_list_next(i)) {
			pmdb_t *db = alpm_list_getdata(i);

			for(j = alpm_db_getgrpcache(db); j; j = alpm_list_next(j)) {
				pmgrp_t *grp = alpm_list_getdata(j);

				MSG(NL, "%s\n", (char *)alpm_grp_get_name(grp));
				if(grp && level > 1) {
					list_display("   ", alpm_grp_get_packages(grp));
				}
			}
		}
	}

	return(0);
}

static int sync_info(alpm_list_t *syncs, alpm_list_t *targets)
{
	alpm_list_t *i, *j, *k;

	if(targets) {
		for(i = targets; i; i = alpm_list_next(i)) {
			int found = 0;

			for(j = syncs; j && !found; j = alpm_list_next(j)) {
				pmdb_t *db = alpm_list_getdata(j);

				for(k = alpm_db_getpkgcache(db); !found && k; k = alpm_list_next(k)) {
					pmpkg_t *pkg = alpm_list_getdata(k);

					if(!strcmp(alpm_pkg_get_name(pkg), alpm_list_getdata(i))) {
						dump_pkg_sync(pkg, alpm_db_get_name(db));
						MSG(NL, "\n");
						found = 1;
					}
				}
			}
			if(!found) {
				ERR(NL, _("package \"%s\" was not found.\n"), (char *)i->data);
				break;
			}
		}
	} else {
		for(i = syncs; i; i = alpm_list_next(i)) {
			pmdb_t *db = alpm_list_getdata(i);
			
			for(j = alpm_db_getpkgcache(db); j; j = alpm_list_next(j)) {
				dump_pkg_sync(alpm_list_getdata(j), alpm_db_get_name(db));
				MSG(NL, "\n");
			}
		}
	}

	return(0);
}

static int sync_list(alpm_list_t *syncs, alpm_list_t *targets)
{
	alpm_list_t *i, *j, *ls = NULL;

	if(targets) {
		for(i = targets; i; i = alpm_list_next(i)) {
			const char *repo = alpm_list_getdata(i);
			pmdb_t *db = NULL;

			for(j = syncs; j; j = alpm_list_next(j)) {
				pmdb_t *d = alpm_list_getdata(j);

				if(strcmp(repo, alpm_db_get_name(d)) == 0) {
					db = d;
					break;
				}
			}

			if(db == NULL) {
				ERR(NL, _("repository \"%s\" was not found.\n"),repo);
				alpm_list_free(ls);
				return(1);
			}

			ls = alpm_list_add(ls, db);
		}
	} else {
		ls = syncs;
	}

	for(i = ls; i; i = alpm_list_next(i)) {
		pmdb_t *db = alpm_list_getdata(i);

		for(j = alpm_db_getpkgcache(db); j; j = alpm_list_next(j)) {
			pmpkg_t *pkg = alpm_list_getdata(j);
			MSG(NL, "%s %s %s\n", alpm_db_get_name(db), alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
		}
	}

	if(targets) {
		alpm_list_free(ls);
	}

	return(0);
}

int pacman_sync(alpm_list_t *targets)
{
	int confirm = 0;
	int retval = 0;
	alpm_list_t *packages, *data = NULL, *i, *j, *k, *sync_dbs;

	sync_dbs = alpm_option_get_syncdbs();
	if(sync_dbs == NULL || alpm_list_count(sync_dbs) == 0) {
		ERR(NL, _("no usable package repositories configured.\n"));
		return(1);
	}

	if(config->op_s_clean) {
		return(sync_cleancache(config->op_s_clean));
	}

	if(config->op_s_search) {
		return(sync_search(sync_dbs, targets));
	}

	if(config->group) {
		return(sync_group(config->group, sync_dbs, targets));
	}

	if(config->op_s_info) {
		return(sync_info(sync_dbs, targets));
	}

	if(config->op_q_list) {
		return(sync_list(sync_dbs, targets));
	}

	/* Step 1: create a new transaction...
	 */
	if(alpm_trans_init(PM_TRANS_TYPE_SYNC, config->flags, cb_trans_evt, cb_trans_conv, cb_trans_progress) == -1) {
		ERR(NL, _("failed to init transaction (%s)\n"), alpm_strerror(pm_errno));
		if(pm_errno == PM_ERR_HANDLE_LOCK) {
			MSG(NL, _("       if you're sure a package manager is not already running,\n"
			        "       you can remove %s%s\n"), alpm_option_get_root(), PM_LOCK);
		}
		return(1);
	}

	if(config->op_s_sync) {
		/* grab a fresh package list */
		MSG(NL, _(":: Synchronizing package databases...\n"));
		alpm_logaction(_("synchronizing package lists"));
		if(!sync_synctree(config->op_s_sync, sync_dbs)) {
			ERR(NL, _("failed to synchronize any databases"));
			return(1);
		}
	}

	if(config->op_s_upgrade) {
		MSG(NL, _(":: Starting full system upgrade...\n"));
		alpm_logaction(_("starting full system upgrade"));
		if(alpm_trans_sysupgrade() == -1) {
			ERR(NL, "%s\n", alpm_strerror(pm_errno));
			retval = 1;
			goto cleanup;
		}

		/* check if pacman itself is one of the packages to upgrade.  If so, we
		 * we should upgrade ourselves first and then re-exec as the new version.
		 *
		 * this can prevent some of the "syntax error" problems users can have
		 * when sysupgrade'ing with an older version of pacman.
		 */
		data = alpm_trans_get_packages();
		for(i = alpm_list_first(data); i; i = alpm_list_next(i)) {
			pmsyncpkg_t *sync = alpm_list_getdata(i);
			pmpkg_t *spkg = alpm_sync_get_package(sync);
			if(strcmp("pacman", alpm_pkg_get_name(spkg)) == 0 && alpm_list_count(data) > 1) {
				MSG(NL, _("\n:: pacman has detected a newer version of the \"pacman\" package.\n"));
				MSG(NL, _(":: It is recommended that you allow pacman to upgrade itself\n"));
				MSG(NL, _(":: first, then you can re-run the operation with the newer version.\n"));
				MSG(NL, "::\n");
				if(yesno(_(":: Upgrade pacman first? [Y/n] "))) {
					if(alpm_trans_release() == -1) {
						ERR(NL, _("failed to release transaction (%s)\n"), alpm_strerror(pm_errno));
						retval = 1;
						goto cleanup;
					}
					if(alpm_trans_init(PM_TRANS_TYPE_SYNC, config->flags, cb_trans_evt, cb_trans_conv, cb_trans_progress) == -1) {
						ERR(NL, _("failed to init transaction (%s)\n"), alpm_strerror(pm_errno));
						if(pm_errno == PM_ERR_HANDLE_LOCK) {
							MSG(NL, _("       if you're sure a package manager is not already running,\n"
			        				"       you can remove %s%s\n"), alpm_option_get_root(), PM_LOCK);
						}
						return(1);
					}
					if(alpm_trans_addtarget("pacman") == -1) {
						ERR(NL, _("pacman: %s\n"), alpm_strerror(pm_errno));
						retval = 1;
						goto cleanup;
					}
					break;
				}
			}
		}
	} else {
		/* process targets */
		for(i = targets; i; i = alpm_list_next(i)) {
			char *targ = alpm_list_getdata(i);
			if(alpm_trans_addtarget(targ) == -1) {
				pmgrp_t *grp = NULL;
				int found=0;
				if(pm_errno == PM_ERR_TRANS_DUP_TARGET) {
					/* just ignore duplicate targets */
					continue;
				}
				if(pm_errno != PM_ERR_PKG_NOT_FOUND) {
					ERR(NL, _("'%s': %s\n"), (char *)i->data, alpm_strerror(pm_errno));
					retval = 1;
					goto cleanup;
				}
				/* target not found: check if it's a group */
				
				for(j = alpm_option_get_syncdbs(); j; j = alpm_list_next(j)) {
					pmdb_t *db = alpm_list_getdata(j);
					grp = alpm_db_readgrp(db, targ);
					if(grp) {
						found++;
						MSG(NL, _(":: group %s:\n"), targ);
						/* remove dupe entries in case a package exists in multiple repos */
						alpm_list_t *pkgs = alpm_list_remove_dupes(alpm_grp_get_packages(grp));
						list_display("   ", pkgs);
						if(yesno(_(":: Install whole content? [Y/n] "))) {
							for(k = pkgs; k; k = alpm_list_next(k)) {
								targets = alpm_list_add(targets, strdup(alpm_list_getdata(k)));
							}
						} else {
							for(k = pkgs; k; k = alpm_list_next(k)) {
								char *pkgname = alpm_list_getdata(k);
								if(yesno(_(":: Install %s from group %s? [Y/n] "), pkgname, targ)) {
									targets = alpm_list_add(targets, strdup(pkgname));
								}
							}
						}
						alpm_list_free(pkgs);
					}
				}
				if(!found) {
					/* targ not found in sync db, searching for providers... */
					const char *pname = NULL;
					for(j = alpm_option_get_syncdbs(); j; j = alpm_list_next(j)) {
						pmdb_t *db = alpm_list_getdata(j);
						alpm_list_t *prov = alpm_db_whatprovides(db, targ);
						if(prov) {
							pmpkg_t *pkg = alpm_list_getdata(prov);
							pname = alpm_pkg_get_name(pkg);
							break;
						}
					}
					if(pname != NULL) {
						/* targ is provided by pname */
						targets = alpm_list_add(targets, strdup(pname));
					} else {
						ERR(NL, _("'%s': not found in sync db\n"), targ);
						retval = 1;
						goto cleanup;
					}
				}
			}
		}
	}

	/* Step 2: "compute" the transaction based on targets and flags
	 */
	if(alpm_trans_prepare(&data) == -1) {
		long long *pkgsize, *freespace;
		ERR(NL, _("failed to prepare transaction (%s)\n"), alpm_strerror(pm_errno));
		switch(pm_errno) {
			case PM_ERR_UNSATISFIED_DEPS:
				for(i = data; i; i = alpm_list_next(i)) {
					pmdepmissing_t *miss = alpm_list_getdata(i);
					MSG(NL, ":: %s %s %s", alpm_dep_get_target(miss),
					    alpm_dep_get_type(miss) == PM_DEP_TYPE_DEPEND ?
									_("requires") : _("is required by"),
									alpm_dep_get_name(miss));
					switch(alpm_dep_get_mod(miss)) {
						case PM_DEP_MOD_ANY:
							break;
						case PM_DEP_MOD_EQ:
							MSG(CL, "=%s", alpm_dep_get_version(miss));
							break;
						case PM_DEP_MOD_GE:
							MSG(CL, ">=%s", alpm_dep_get_version(miss));
							break;
						case PM_DEP_MOD_LE:
							MSG(CL, "<=%s", alpm_dep_get_version(miss));
							break;
					}
					MSG(CL, "\n");
				}
			break;
			case PM_ERR_CONFLICTING_DEPS:
			  for(i = data; i; i = alpm_list_next(i)) {
					pmdepmissing_t *miss = alpm_list_getdata(i);

					MSG(NL, _(":: %s: conflicts with %s"),
							alpm_dep_get_target(miss), alpm_dep_get_name(miss));
				}
			break;
			case PM_ERR_DISK_FULL:
				pkgsize = alpm_list_getdata(data);
				freespace = alpm_list_getdata(alpm_list_next(data));
				MSG(NL, _(":: %.1f MB required, have %.1f MB"),
							(double)(*pkgsize / 1048576.0), (double)(*freespace / 1048576.0));
			break;
			default:
			break;
		}
		retval = 1;
		goto cleanup;
	}

	packages = alpm_trans_get_packages();
	if(packages == NULL) {
		/* nothing to do: just exit without complaining */
		MSG(NL, _("local database is up to date\n"));
		goto cleanup;
	}

	if(!(alpm_trans_get_flags() & PM_TRANS_FLAG_PRINTURIS)) {
		display_targets(packages);

		if(config->op_s_downloadonly) {
			if(config->noconfirm) {
				MSG(NL, _("\nBeginning download...\n"));
				confirm = 1;
			} else {
				MSG(NL, "\n");
				confirm = yesno(_("Proceed with download? [Y/n] "));
			}
		} else {
			/* don't get any confirmation if we're called from makepkg */
			if(config->op_d_resolve) {
				confirm = 1;
			} else {
				if(config->noconfirm) {
					MSG(NL, _("\nBeginning upgrade process...\n"));
					confirm = 1;
				} else {
					MSG(NL, "\n");
					confirm = yesno(_("Proceed with installation? [Y/n] "));
				}
			}
		}
		if(!confirm) {
			goto cleanup;
		}
	}/* else 'print uris' requested.  We're done at this point */

	/* Step 3: actually perform the installation
	*/
	if(alpm_trans_commit(&data) == -1) {
		ERR(NL, _("failed to commit transaction (%s)\n"), alpm_strerror(pm_errno));
		switch(pm_errno) {
		case PM_ERR_FILE_CONFLICTS:
			for(i = data; i; i = alpm_list_next(i)) {
				pmconflict_t *conflict = alpm_list_getdata(i);
				switch(alpm_conflict_get_type(conflict)) {
				case PM_CONFLICT_TYPE_TARGET:
					MSG(NL, _("%s%s exists in \"%s\" (target) and \"%s\" (target)\n"),
							alpm_option_get_root(),
							alpm_conflict_get_file(conflict),
							alpm_conflict_get_target(conflict),
							alpm_conflict_get_ctarget(conflict));
					break;
				case PM_CONFLICT_TYPE_FILE:
					MSG(NL, _("%s: %s%s exists in filesystem\n"),
							alpm_conflict_get_target(conflict),
							alpm_option_get_root(),
							alpm_conflict_get_file(conflict));
					break;
				}
			}
			MSG(NL, _("\nerrors occurred, no packages were upgraded.\n"));
			break;
		case PM_ERR_PKG_CORRUPTED:
			for(i = data; i; i = alpm_list_next(i)) {
				MSG(NL, "%s", (char*)alpm_list_getdata(i));
			}
			MSG(NL, _("\nerrors occurred, no packages were upgraded.\n"));
			break;
		default:
			break;
		}
		retval = 1;
		goto cleanup;
	}

	/* Step 4: release transaction resources
	 */
cleanup:
	if(data) {
		alpm_list_free(data);
	}
	if(alpm_trans_release() == -1) {
		ERR(NL, _("failed to release transaction (%s)\n"), alpm_strerror(pm_errno));
		retval = 1;
	}

	return(retval);
}

/* vim: set ts=2 sw=2 noet: */
