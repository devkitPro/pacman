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
#include <fetch.h> /* fetchLastErrString */
/* pacman */
#include "util.h"
#include "log.h"
#include "download.h"
#include "list.h"
#include "package.h"
#include "trans.h"
#include "sync.h"
#include "conf.h"

extern config_t *config;

extern list_t *pmc_syncs;

static int sync_cleancache(int level)
{
	long lroot, lcachedir;
	char *root, *cachedir;
	char dirpath[PATH_MAX];

	alpm_get_option(PM_OPT_ROOT, &lroot);
	root = (void *)&lroot;
	alpm_get_option(PM_OPT_CACHEDIR, &lcachedir);
	cachedir = (void *)&lcachedir;

	snprintf(dirpath, PATH_MAX, "%s%s", root, cachedir);

	if(level == 1) {
		/* incomplete cleanup: we keep latest packages and partial downloads */
		DIR *dir;
		struct dirent *ent;
		list_t *cache = NULL;
		list_t *clean = NULL;
		list_t *i, *j;

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
			cache = list_add(cache, strdup(ent->d_name));
		}
		closedir(dir);

		for(i = cache; i; i = i->next) {
			char *str = i->data;
			char name[256], version[64];

			if(strstr(str, PM_EXT_PKG) == NULL) {
				clean = list_add(clean, strdup(str));
				continue;
			}
			/* we keep partially downloaded files */
			if(strstr(str, PM_EXT_PKG ".part")) {
				continue;
			}
			if(split_pkgname(str, name, version) != 0) {
				clean = list_add(clean, strdup(str));
				continue;
			}
			for(j = i->next; j; j = j->next) {
				char *s = j->data;
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
				if(!strcmp(name, n)) {
					char *ptr = (alpm_pkg_vercmp(version, v) < 0) ? str : s;
					if(!list_is_strin(ptr, clean)) {
						clean = list_add(clean, strdup(ptr));
					}
				}
			}
		}
		FREELIST(cache);

		for(i = clean; i; i = i->next) {
			char path[PATH_MAX];

			snprintf(path, PATH_MAX, "%s/%s", dirpath, (char *)i->data);
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

static int sync_synctree(int level, list_t *syncs)
{
	list_t *i;
	int success = 0, ret;

	for(i = syncs; i; i = i->next) {
		sync_t *sync = (sync_t *)i->data;

		ret = alpm_db_update((level < 2 ? 0 : 1), sync->db);
		if(ret < 0) {
			if(pm_errno == PM_ERR_DB_SYNC) {
				/* use libfetch error */
				ERR(NL, _("failed to synchronize %s: %s\n"), sync->treename, fetchLastErrString);
			} else {
				ERR(NL, _("failed to update %s (%s)\n"), sync->treename, alpm_strerror(pm_errno));
			}
		} else if(ret == 1) {
			MSG(NL, _(" %s is up to date\n"), sync->treename);
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

static int sync_search(list_t *syncs, list_t *targets)
{
	list_t *i;
	PM_LIST *ret;

	for(i = targets; i; i = i->next) {
		alpm_set_option(PM_OPT_NEEDLES, (long)i->data);
	}
	for(i = syncs; i; i = i->next) {
		sync_t *sync = i->data;
		if(targets) {
			PM_LIST *lp;
			ret = alpm_db_search(sync->db);
			if(ret == NULL) {
				continue;
			}
			for(lp = ret; lp; lp = alpm_list_next(lp)) {
				PM_PKG *pkg = alpm_list_getdata(lp);

				char *group = (char *)alpm_list_getdata(alpm_pkg_getinfo(pkg,PM_PKG_GROUPS));
				printf("%s/%s %s %s%s%s\n    ",
							 (char *)alpm_db_getinfo(sync->db, PM_DB_TREENAME),
						   (char *)alpm_pkg_getinfo(pkg, PM_PKG_NAME),
						   (char *)alpm_pkg_getinfo(pkg, PM_PKG_VERSION),
						   (group ? " (" : ""), (group ? group : ""), (group ? ") " : ""));
				indentprint((char *)alpm_pkg_getinfo(pkg, PM_PKG_DESC), 4);
				printf("\n");
			}
		} else {
			PM_LIST *lp;

			for(lp = alpm_db_getpkgcache(sync->db); lp; lp = alpm_list_next(lp)) {
				PM_PKG *pkg = alpm_list_getdata(lp);

				MSG(NL, "%s/%s %s\n    ", sync->treename, (char *)alpm_pkg_getinfo(pkg, PM_PKG_NAME), (char *)alpm_pkg_getinfo(pkg, PM_PKG_VERSION));
				indentprint(alpm_pkg_getinfo(pkg, PM_PKG_DESC), 4);
				MSG(NL, "\n");
			}
		}
	}

	return(0);
}

static int sync_group(int level, list_t *syncs, list_t *targets)
{
	list_t *i, *j;

	if(targets) {
		for(i = targets; i; i = i->next) {
			for(j = syncs; j; j = j->next) {
				sync_t *sync = j->data;
				PM_GRP *grp = alpm_db_readgrp(sync->db, i->data);

				if(grp) {
					MSG(NL, "%s\n", (char *)alpm_grp_getinfo(grp, PM_GRP_NAME));
					PM_LIST_display("   ", alpm_grp_getinfo(grp, PM_GRP_PKGNAMES));
				}
			}
		}
	} else {
		for(j = syncs; j; j = j->next) {
			sync_t *sync = j->data;
			PM_LIST *lp;

			for(lp = alpm_db_getgrpcache(sync->db); lp; lp = alpm_list_next(lp)) {
				PM_GRP *grp = alpm_list_getdata(lp);

				MSG(NL, "%s\n", (char *)alpm_grp_getinfo(grp, PM_GRP_NAME));
				if(grp && level > 1) {
					PM_LIST_display("   ", alpm_grp_getinfo(grp, PM_GRP_PKGNAMES));
				}
			}
		}
	}

	return(0);
}

static int sync_info(list_t *syncs, list_t *targets)
{
	list_t *i, *j;

	if(targets) {
		for(i = targets; i; i = i->next) {
			int found = 0;

			for(j = syncs; j && !found; j = j->next) {
				sync_t *sync = j->data;
				PM_LIST *lp;

				for(lp = alpm_db_getpkgcache(sync->db); !found && lp; lp = alpm_list_next(lp)) {
					PM_PKG *pkg = alpm_list_getdata(lp);

					if(!strcmp(alpm_pkg_getinfo(pkg, PM_PKG_NAME), i->data)) {
						dump_pkg_sync(pkg, sync->treename);
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
		for(j = syncs; j; j = j->next) {
			sync_t *sync = j->data;
			PM_LIST *lp;
			
			for(lp = alpm_db_getpkgcache(sync->db); lp; lp = alpm_list_next(lp)) {
				dump_pkg_sync(alpm_list_getdata(lp), sync->treename);
				MSG(NL, "\n");
			}
		}
	}

	return(0);
}

static int sync_list(list_t *syncs, list_t *targets)
{
	list_t *i;
	list_t *ls = NULL;

	if(targets) {
		for(i = targets; i; i = i->next) {
			list_t *j;
			sync_t *sync = NULL;

			for(j = syncs; j && !sync; j = j->next) {
				sync_t *s = j->data;

				if(strcmp(i->data, s->treename) == 0) {
					sync = s;
				}
			}

			if(sync == NULL) {
				ERR(NL, _("repository \"%s\" was not found.\n"), (char *)i->data);
				FREELISTPTR(ls);
				return(1);
			}

			ls = list_add(ls, sync);
		}
	} else {
		ls = syncs;
	}

	for(i = ls; i; i = i->next) {
		PM_LIST *lp;
		sync_t *sync = i->data;

		for(lp = alpm_db_getpkgcache(sync->db); lp; lp = alpm_list_next(lp)) {
			PM_PKG *pkg = alpm_list_getdata(lp);

			MSG(NL, "%s %s %s\n", (char *)sync->treename, (char *)alpm_pkg_getinfo(pkg, PM_PKG_NAME), (char *)alpm_pkg_getinfo(pkg, PM_PKG_VERSION));
		}
	}

	if(targets) {
		FREELISTPTR(ls);
	}

	return(0);
}

int pacman_sync(list_t *targets)
{
	int confirm = 0;
	int retval = 0;
	list_t *i = NULL;
	PM_LIST *packages, *data, *lp;

	if(pmc_syncs == NULL || !list_count(pmc_syncs)) {
		ERR(NL, _("no usable package repositories configured.\n"));
		return(1);
	}

	if(config->op_s_clean) {
		return(sync_cleancache(config->op_s_clean));
	}

	if(config->op_s_sync) {
		/* grab a fresh package list */
		MSG(NL, _(":: Synchronizing package databases...\n"));
		alpm_logaction(_("synchronizing package lists"));
		if(!sync_synctree(config->op_s_sync, pmc_syncs)) {
			ERR(NL, _("failed to synchronize any databases"));
			return(1);
		}
	}

	if(config->op_s_search) {
		return(sync_search(pmc_syncs, targets));
	}

	if(config->group) {
		return(sync_group(config->group, pmc_syncs, targets));
	}

	if(config->op_s_info) {
		return(sync_info(pmc_syncs, targets));
	}

	if(config->op_q_list) {
		return(sync_list(pmc_syncs, targets));
	}

	/* Step 1: create a new transaction...
	 */
	if(alpm_trans_init(PM_TRANS_TYPE_SYNC, config->flags, cb_trans_evt, cb_trans_conv, cb_trans_progress) == -1) {
		ERR(NL, _("failed to init transaction (%s)\n"), alpm_strerror(pm_errno));
		if(pm_errno == PM_ERR_HANDLE_LOCK) {
			MSG(NL, _("       if you're sure a package manager is not already running,\n"
			        "       you can remove %s%s\n"), config->root, PM_LOCK);
		}
		return(1);
	}

	if(config->op_s_upgrade) {
		MSG(NL, _(":: Starting local database upgrade...\n"));
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
		data = alpm_trans_getinfo(PM_TRANS_PACKAGES);
		for(lp = alpm_list_first(data); lp; lp = alpm_list_next(lp)) {
			PM_SYNCPKG *sync = alpm_list_getdata(lp);
			PM_PKG *spkg = alpm_sync_getinfo(sync, PM_SYNC_PKG);
			if(!strcmp("pacman", alpm_pkg_getinfo(spkg, PM_PKG_NAME)) && alpm_list_count(data) > 1) {
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
			        				"       you can remove %s%s\n"), config->root, PM_LOCK);
						}
						return(1);
					}
					if(alpm_trans_addtarget("pacman") == -1) {
						ERR(NL, _("could not add target '%s': %s\n"), (char *)i->data, alpm_strerror(pm_errno));
						retval = 1;
						goto cleanup;
					}
					break;
				}
			}
		}
	} else {
		/* process targets */
		for(i = targets; i; i = i->next) {
			char *targ = i->data;
			if(alpm_trans_addtarget(targ) == -1) {
				PM_GRP *grp = NULL;
				list_t *j;
				int found=0;
				if(pm_errno == PM_ERR_TRANS_DUP_TARGET) {
					/* just ignore duplicate targets */
					continue;
				}
				if(pm_errno != PM_ERR_PKG_NOT_FOUND) {
					ERR(NL, _("could not add target '%s': %s\n"), (char *)i->data, alpm_strerror(pm_errno));
					retval = 1;
					goto cleanup;
				}
				/* target not found: check if it's a group */
				for(j = pmc_syncs; j; j = j->next) {
					sync_t *sync = j->data;
					grp = alpm_db_readgrp(sync->db, targ);
					if(grp) {
						PM_LIST *pmpkgs;
						list_t *k, *pkgs;
						found++;
						MSG(NL, _(":: group %s:\n"), targ);
						pmpkgs = alpm_grp_getinfo(grp, PM_GRP_PKGNAMES);
						/* remove dupe entries in case a package exists in multiple repos */
						/*   (the dupe function takes a PM_LIST* and returns a list_t*) */
						pkgs = PM_LIST_remove_dupes(pmpkgs);
						list_display("   ", pkgs);
						if(yesno(_(":: Install whole content? [Y/n] "))) {
							for(k = pkgs; k; k = k->next) {
								targets = list_add(targets, strdup(k->data));
							}
						} else {
							for(k = pkgs; k; k = k->next) {
								char *pkgname = k->data;
								if(yesno(_(":: Install %s from group %s? [Y/n] "), pkgname, targ)) {
									targets = list_add(targets, strdup(pkgname));
								}
							}
						}
						FREELIST(pkgs);
					}
				}
				if(!found) {
					/* targ not found in sync db, searching for providers... */
					PM_LIST *k = NULL;
					PM_PKG *pkg;
					char *pname = NULL;
					for(j = pmc_syncs; j && !k; j = j->next) {
						sync_t *sync = j->data;
						k = alpm_db_whatprovides(sync->db, targ);
						pkg = (PM_PKG*)alpm_list_getdata(alpm_list_first(k));
						pname = (char*)alpm_pkg_getinfo(pkg, PM_PKG_NAME);
					}
					if(pname != NULL) {
						/* targ is provided by pname */
						targets = list_add(targets, strdup(pname));
					} else {
						ERR(NL, _("could not add target '%s': not found in sync db\n"), targ);
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
				for(lp = alpm_list_first(data); lp; lp = alpm_list_next(lp)) {
					PM_DEPMISS *miss = alpm_list_getdata(lp);
					MSG(NL, ":: %s: %s %s", alpm_dep_getinfo(miss, PM_DEP_TARGET),
					    (long)alpm_dep_getinfo(miss, PM_DEP_TYPE) == PM_DEP_TYPE_DEPEND ? _("requires") : _("is required by"),
					    alpm_dep_getinfo(miss, PM_DEP_NAME));
					switch((long)alpm_dep_getinfo(miss, PM_DEP_MOD)) {
						case PM_DEP_MOD_EQ: MSG(CL, "=%s", alpm_dep_getinfo(miss, PM_DEP_VERSION)); break;
						case PM_DEP_MOD_GE: MSG(CL, ">=%s", alpm_dep_getinfo(miss, PM_DEP_VERSION)); break;
						case PM_DEP_MOD_LE: MSG(CL, "<=%s", alpm_dep_getinfo(miss, PM_DEP_VERSION)); break;
					}
					MSG(CL, "\n");
				}
				alpm_list_free(data);
			break;
			case PM_ERR_CONFLICTING_DEPS:
				for(lp = alpm_list_first(data); lp; lp = alpm_list_next(lp)) {
					PM_DEPMISS *miss = alpm_list_getdata(lp);

					MSG(NL, _(":: %s: conflicts with %s"),
						alpm_dep_getinfo(miss, PM_DEP_TARGET), alpm_dep_getinfo(miss, PM_DEP_NAME));
				}
				alpm_list_free(data);
			break;
			case PM_ERR_DISK_FULL:
				lp = alpm_list_first(data);
				pkgsize = alpm_list_getdata(lp);
				lp = alpm_list_next(lp);
				freespace = alpm_list_getdata(lp);
					MSG(NL, _(":: %.1f MB required, have %.1f MB"),
						(double)(*pkgsize / 1048576.0), (double)(*freespace / 1048576.0));
				alpm_list_free(data);
			break;
			default:
			break;
		}
		retval = 1;
		goto cleanup;
	}

	packages = alpm_trans_getinfo(PM_TRANS_PACKAGES);
	if(packages == NULL) {
		/* nothing to do: just exit without complaining */
		MSG(NL," local database is up to date");
		goto cleanup;
	}

	/* list targets and get confirmation */
	if(!((unsigned long)alpm_trans_getinfo(PM_TRANS_FLAGS) & PM_TRANS_FLAG_PRINTURIS)) {
		list_t *list_install = NULL;
		list_t *list_remove = NULL;
		char *str;
		unsigned long totalsize = 0;
		unsigned long totalusize = 0;
		double mb, umb;

		for(lp = alpm_list_first(packages); lp; lp = alpm_list_next(lp)) {
			PM_SYNCPKG *sync = alpm_list_getdata(lp);
			PM_PKG *pkg = alpm_sync_getinfo(sync, PM_SYNC_PKG);
			char *pkgname, *pkgver;

			if((long)alpm_sync_getinfo(sync, PM_SYNC_TYPE) == PM_SYNC_TYPE_REPLACE) {
				PM_LIST *j, *data;
				data = alpm_sync_getinfo(sync, PM_SYNC_DATA);
				for(j = alpm_list_first(data); j; j = alpm_list_next(j)) {
					PM_PKG *p = alpm_list_getdata(j);
					char *pkgname = alpm_pkg_getinfo(p, PM_PKG_NAME);
					if(!list_is_strin(pkgname, list_remove)) {
						list_remove = list_add(list_remove, strdup(pkgname));
					}
				}
			}

			pkgname = alpm_pkg_getinfo(pkg, PM_PKG_NAME);
			pkgver = alpm_pkg_getinfo(pkg, PM_PKG_VERSION);
			totalsize += (long)alpm_pkg_getinfo(pkg, PM_PKG_SIZE);
			totalusize += (long)alpm_pkg_getinfo(pkg, PM_PKG_USIZE);

			asprintf(&str, "%s-%s", pkgname, pkgver);
			list_install = list_add(list_install, str);
		}
		if(list_remove) {
			MSG(NL, _("\nRemove:  "));
			str = buildstring(list_remove);
			indentprint(str, 9);
			MSG(CL, "\n");
			FREELIST(list_remove);
			FREE(str);
		}
		mb = (double)(totalsize / 1048576.0);
		umb = (double)(totalusize / 1048576.0);
		/* round up to 0.1 */
		if(mb < 0.1) {
			mb = 0.1;
		}
		if(umb > 0 && umb < 0.1) {
			umb = 0.1;
		}
		MSG(NL, _("\nTargets: "));
		str = buildstring(list_install);
		indentprint(str, 9);
		MSG(NL, _("\nTotal Package Size:   %.1f MB\n"), mb);
		if(umb > 0) {
		  MSG(NL, _("\nTotal Uncompressed Package Size:   %.1f MB\n"), umb);
		}
		FREELIST(list_install);
		FREE(str);

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
					confirm = yesno(_("Proceed with upgrade? [Y/n] "));
				}
			}
		}
		if(!confirm) {
			goto cleanup;
		}
	}

	/* Step 3: actually perform the installation
	 */
	if(alpm_trans_commit(&data) == -1) {
		ERR(NL, _("failed to commit transaction (%s)\n"), alpm_strerror(pm_errno));
		switch(pm_errno) {
			case PM_ERR_FILE_CONFLICTS:
				for(lp = alpm_list_first(data); lp; lp = alpm_list_next(lp)) {
					PM_CONFLICT *conflict = alpm_list_getdata(lp);
					switch((long)alpm_conflict_getinfo(conflict, PM_CONFLICT_TYPE)) {
						case PM_CONFLICT_TYPE_TARGET:
							MSG(NL, _("%s%s exists in \"%s\" (target) and \"%s\" (target)"),
											config->root,
							        (char *)alpm_conflict_getinfo(conflict, PM_CONFLICT_FILE),
							        (char *)alpm_conflict_getinfo(conflict, PM_CONFLICT_TARGET),
							        (char *)alpm_conflict_getinfo(conflict, PM_CONFLICT_CTARGET));
						break;
						case PM_CONFLICT_TYPE_FILE:
							MSG(NL, _("%s: %s%s exists in filesystem"),
							        (char *)alpm_conflict_getinfo(conflict, PM_CONFLICT_TARGET),
											config->root,
							        (char *)alpm_conflict_getinfo(conflict, PM_CONFLICT_FILE));
						break;
					}
				}
				alpm_list_free(data);
				MSG(NL, _("\nerrors occurred, no packages were upgraded.\n"));
			break;
			case PM_ERR_PKG_CORRUPTED:
				for(lp = alpm_list_first(data); lp; lp = alpm_list_next(lp)) {
					MSG(NL, "%s", (char*)alpm_list_getdata(lp));
				}
				alpm_list_free(data);
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
	if(alpm_trans_release() == -1) {
		ERR(NL, _("failed to release transaction (%s)\n"), alpm_strerror(pm_errno));
		retval = 1;
	}

	return(retval);
}

/* vim: set ts=2 sw=2 noet: */
