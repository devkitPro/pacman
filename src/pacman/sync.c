/*
 *  sync.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>

#include <alpm.h>
#include <alpm_list.h>
#include <download.h> /* downloadLastErrString */
/* TODO remove above download.h inclusion once we abstract more, and also
 * remove it from Makefile.am on the pacman side */

/* pacman */
#include "pacman.h"
#include "util.h"
#include "package.h"
#include "callback.h"
#include "conf.h"

extern pmdb_t *db_local;

/* if keep_used != 0, then the dirnames which match an used syncdb
 * will be kept  */
static int sync_cleandb(const char *dbpath, int keep_used) {
	DIR *dir;
	struct dirent *ent;

	dir = opendir(dbpath);
	if(dir == NULL) {
		fprintf(stderr, _("error: could not access database directory\n"));
		return(1);
	}

	rewinddir(dir);
	/* step through the directory one file at a time */
	while((ent = readdir(dir)) != NULL) {
		char path[PATH_MAX];
		alpm_list_t *syncdbs = NULL, *i;
		int found = 0;
		char *dname = ent->d_name;

		if(!strcmp(dname, ".") || !strcmp(dname, "..")) {
			continue;
		}
		/* skip the local and sync directories */
		if(!strcmp(dname, "sync") || !strcmp(dname, "local")) {
			continue;
		}
		if(keep_used) {
			syncdbs = alpm_option_get_syncdbs();
			for(i = syncdbs; i && !found; i = alpm_list_next(i)) {
				pmdb_t *db = alpm_list_getdata(i);
				found = !strcmp(dname, alpm_db_get_name(db));
			}
		}
		/* We have a directory that doesn't match any syncdb.
		 * Ask the user if he wants to remove it. */
		if(!found) {
			/* build the full path */
			snprintf(path, PATH_MAX, "%s%s", dbpath, ent->d_name);

			if(!yesno(_("Do you want to remove %s? [Y/n] "), path)) {
				continue;
			}

			if(rmrf(path)) {
				fprintf(stderr, _("error: could not remove repository directory\n"));
				return(1);
			}
		}

	}
	return(0);
}

static int sync_cleandb_all(void) {
	const char *dbpath = alpm_option_get_dbpath();
	char newdbpath[PATH_MAX];

	printf(_("Database directory: %s\n"), dbpath);
	if(!yesno(_("Do you want to remove unused repositories? [Y/n] "))) {
		return(0);
	}
	/* The sync dbs were previously put in dbpath/, but are now in dbpath/sync/,
	 * so we will clean everything in dbpath/ (except dbpath/local/ and dbpath/sync/,
	 * and only the unused sync dbs in dbpath/sync/ */
	sync_cleandb(dbpath, 0);

	sprintf(newdbpath, "%s%s", dbpath, "sync/");
	sync_cleandb(newdbpath, 1);

	printf(_("Database directory cleaned up\n"));
	return(0);
}

static int sync_cleancache(int level)
{
	/* TODO for now, just mess with the first cache directory */
	alpm_list_t* cachedirs = alpm_option_get_cachedirs();
	const char *cachedir = alpm_list_getdata(cachedirs);

	if(level == 1) {
		/* incomplete cleanup */
		DIR *dir;
		struct dirent *ent;
		/* Let's vastly improve the way this is done. Before, we went by package
		 * name. Instead, let's only keep packages we have installed. Open up each
		 * package and see if it has an entry in the local DB; if not, delete it.
		 */
		printf(_("Cache directory: %s\n"), cachedir);
		if(!yesno(_("Do you want to remove uninstalled packages from cache? [Y/n] "))) {
			return(0);
		}
		printf(_("removing old packages from cache... "));

		dir = opendir(cachedir);
		if(dir == NULL) {
			fprintf(stderr, _("error: could not access cache directory\n"));
			return(1);
		}

		rewinddir(dir);
		/* step through the directory one file at a time */
		while((ent = readdir(dir)) != NULL) {
			char path[PATH_MAX];
			pmpkg_t *localpkg = NULL, *dbpkg = NULL;

			if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
				continue;
			}
			/* build the full filepath */
			snprintf(path, PATH_MAX, "%s/%s", cachedir, ent->d_name);

			/* attempt to load the package, skip file on failures as we may have
			 * files here that aren't valid packages. we also don't need a full
			 * load of the package, just the metadata. */
			if(alpm_pkg_load(path, 0, &localpkg) != 0 || localpkg == NULL) {
				continue;
			}
			/* check if this package is in the local DB */
			dbpkg = alpm_db_get_pkg(db_local, alpm_pkg_get_name(localpkg));
			if(dbpkg == NULL) {
				/* delete package, not present in local DB */
				unlink(path);
			} else if(alpm_pkg_vercmp(alpm_pkg_get_version(localpkg),
							alpm_pkg_get_version(dbpkg)) != 0) {
				/* delete package, it was found but version differs */
				unlink(path);
			}
			/* else version was the same, so keep the package */
			/* free the local file package */
			alpm_pkg_free(localpkg);
		}
		printf(_("done.\n"));
	} else {
		/* full cleanup */
		printf(_("Cache directory: %s\n"), cachedir);
		if(!yesno(_("Do you want to remove ALL packages from cache? [Y/n] "))) {
			return(0);
		}
		printf(_("removing all packages from cache... "));

		if(rmrf(cachedir)) {
			fprintf(stderr, _("error: could not remove cache directory\n"));
			return(1);
		}

		if(makepath(cachedir)) {
			fprintf(stderr, _("error: could not create new cache directory\n"));
			return(1);
		}
		printf(_("done.\n"));
	}

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
				fprintf(stderr, _("error: failed to synchronize %s: %s\n"),
				        alpm_db_get_name(db), downloadLastErrString);
			} else {
				fprintf(stderr, _("error: failed to update %s (%s)\n"),
				        alpm_db_get_name(db), alpm_strerrorlast());
			}
		} else if(ret == 1) {
			printf(_(" %s is up to date\n"), alpm_db_get_name(db));
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

/* search the sync dbs for a matching package */
static int sync_search(alpm_list_t *syncs, alpm_list_t *targets)
{
	alpm_list_t *i, *j, *ret;
	int freelist;
	int found = 0;

	for(i = syncs; i; i = alpm_list_next(i)) {
		pmdb_t *db = alpm_list_getdata(i);
		/* if we have a targets list, search for packages matching it */
		if(targets) {
			ret = alpm_db_search(db, targets);
			freelist = 1;
		} else {
			ret = alpm_db_getpkgcache(db);
			freelist = 0;
		}
		if(ret == NULL) {
			continue;
		} else {
			found = 1;
		}
		for(j = ret; j; j = alpm_list_next(j)) {
			/* print repo/name (group) info about each package in our list */
			char *group = NULL;
			alpm_list_t *grp;
			pmpkg_t *pkg = alpm_list_getdata(j);

			if (!config->quiet) {
				printf("%s/%s %s", alpm_db_get_name(db), alpm_pkg_get_name(pkg),
							 alpm_pkg_get_version(pkg));
			} else {
				printf("%s", alpm_pkg_get_name(pkg));
			}

			/* print the package size with the output if ShowSize option set */
			if(config->showsize) {
				/* Convert byte size to MB */
				double mbsize = alpm_pkg_get_size(pkg) / (1024.0 * 1024.0);

				printf(" [%.2f MB]", mbsize);
			}

			if (!config->quiet) {
				if((grp = alpm_pkg_get_groups(pkg)) != NULL) {
					group = alpm_list_getdata(grp);
					printf(" (%s)", (char *)alpm_list_getdata(grp));
				}

				/* we need a newline and initial indent first */
				printf("\n    ");
				indentprint(alpm_pkg_get_desc(pkg), 4);
			}
			printf("\n");
		}
		/* we only want to free if the list was a search list */
		if(freelist) {
			alpm_list_free(ret);
		}
	}

	return(!found);
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
					printf("%s\n", (char *)alpm_grp_get_name(grp));
					list_display("   ", alpm_grp_get_pkgs(grp));
				}
			}
		}
	} else {
		for(i = syncs; i; i = alpm_list_next(i)) {
			pmdb_t *db = alpm_list_getdata(i);

			for(j = alpm_db_getgrpcache(db); j; j = alpm_list_next(j)) {
				pmgrp_t *grp = alpm_list_getdata(j);

				printf("%s\n", (char *)alpm_grp_get_name(grp));
				if(grp && level > 1) {
					list_display("   ", alpm_grp_get_pkgs(grp));
				}
			}
		}
	}

	return(0);
}

static int sync_info(alpm_list_t *syncs, alpm_list_t *targets)
{
	alpm_list_t *i, *j, *k;
	int ret = 0;

	if(targets) {
		for(i = targets; i; i = alpm_list_next(i)) {
			pmdb_t *db = NULL;
			int foundpkg = 0;

			char target[512]; /* TODO is this enough space? */
			char *repo = NULL, *pkgstr = NULL;

			strncpy(target, i->data, 512);
			pkgstr = strchr(target, '/');
			if(pkgstr) {
				repo = target;
				*pkgstr = '\0';
				++pkgstr;

				for(j = syncs; j; j = alpm_list_next(j)) {
					db = alpm_list_getdata(j);
					if(strcmp(repo, alpm_db_get_name(db)) == 0) {
						break;
					}
					db = NULL;
				}

				if(!db) {
					fprintf(stderr, _("error: repository '%s' does not exist\n"), repo);
					return(1);
				}

				for(k = alpm_db_getpkgcache(db); k; k = alpm_list_next(k)) {
					pmpkg_t *pkg = alpm_list_getdata(k);

					if(strcmp(alpm_pkg_get_name(pkg), pkgstr) == 0) {
						dump_pkg_sync(pkg, alpm_db_get_name(db));
						foundpkg = 1;
						break;
					}
				}

				if(!foundpkg) {
					fprintf(stderr, _("error: package '%s' was not found in repository '%s'\n"), pkgstr, repo);
					ret++;
				}
			} else {
				pkgstr = target;

				for(j = syncs; j; j = alpm_list_next(j)) {
					pmdb_t *db = alpm_list_getdata(j);

					for(k = alpm_db_getpkgcache(db); k; k = alpm_list_next(k)) {
						pmpkg_t *pkg = alpm_list_getdata(k);

						if(strcmp(alpm_pkg_get_name(pkg), pkgstr) == 0) {
							dump_pkg_sync(pkg, alpm_db_get_name(db));
							foundpkg = 1;
							break;
						}
					}
				}
				if(!foundpkg) {
					fprintf(stderr, _("error: package '%s' was not found\n"), pkgstr);
					ret++;
				}
			}
		}
	} else {
		for(i = syncs; i; i = alpm_list_next(i)) {
			pmdb_t *db = alpm_list_getdata(i);

			for(j = alpm_db_getpkgcache(db); j; j = alpm_list_next(j)) {
				dump_pkg_sync(alpm_list_getdata(j), alpm_db_get_name(db));
			}
		}
	}

	return(ret);
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
				fprintf(stderr, _("error: repository \"%s\" was not found.\n"),repo);
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
			if (!config->quiet) {
				printf("%s %s %s\n", alpm_db_get_name(db), alpm_pkg_get_name(pkg),
							 alpm_pkg_get_version(pkg));
			} else {
				printf("%s\n", alpm_pkg_get_name(pkg));
			}
		}
	}

	if(targets) {
		alpm_list_free(ls);
	}

	return(0);
}

static int sync_trans(alpm_list_t *targets, int sync_only)
{
	int retval = 0;
	alpm_list_t *data = NULL;
	alpm_list_t *sync_dbs = alpm_option_get_syncdbs();

	/* Step 1: create a new transaction... */
	if(alpm_trans_init(PM_TRANS_TYPE_SYNC, config->flags, cb_trans_evt,
		 cb_trans_conv, cb_trans_progress) == -1) {
		fprintf(stderr, _("error: failed to init transaction (%s)\n"),
		        alpm_strerrorlast());
		if(pm_errno == PM_ERR_HANDLE_LOCK) {
			printf(_("  if you're sure a package manager is not already\n"
			         "  running, you can remove %s.\n"), alpm_option_get_lockfile());
		}
		return(1);
	}

	if(config->op_s_sync) {
		/* grab a fresh package list */
		printf(_(":: Synchronizing package databases...\n"));
		alpm_logaction("synchronizing package lists\n");
		if(!sync_synctree(config->op_s_sync, sync_dbs)) {
			fprintf(stderr, _("error: failed to synchronize any databases\n"));
			retval = 1;
			goto cleanup;
		}
		if(sync_only) {
			goto cleanup;
		}
	}

	if(config->op_s_upgrade) {
		alpm_list_t *pkgs, *i;

		printf(_(":: Starting full system upgrade...\n"));
		alpm_logaction("starting full system upgrade\n");
		if(alpm_trans_sysupgrade() == -1) {
			fprintf(stderr, _("error: %s\n"), alpm_strerrorlast());
			retval = 1;
			goto cleanup;
		}

		/* check if pacman itself is one of the packages to upgrade.
		 * this can prevent some of the "syntax error" problems users can have
		 * when sysupgrade'ing with an older version of pacman.
		 */
		pkgs = alpm_trans_get_pkgs();
		for(i = pkgs; i; i = alpm_list_next(i)) {
			pmsyncpkg_t *sync = alpm_list_getdata(i);
			pmpkg_t *spkg = alpm_sync_get_pkg(sync);
			/* TODO pacman name should probably not be hardcoded. In addition, we
			 * have problems on an -Syu if pacman has to pull in deps, so recommend
			 * an '-S pacman' operation */
			if(strcmp("pacman", alpm_pkg_get_name(spkg)) == 0) {
				printf("\n");
				printf(_(":: pacman has detected a newer version of itself.\n"
				         ":: It is recommended that you upgrade pacman by itself\n"
				         ":: using 'pacman -S pacman', and then rerun the current\n"
				         ":: operation. If you wish to continue the operation and\n"
				         ":: not upgrade pacman separately, answer no.\n"));
				if(yesno(_(":: Cancel current operation? [Y/n] "))) {
					if(alpm_trans_release() == -1) {
						fprintf(stderr, _("error: failed to release transaction (%s)\n"),
						    alpm_strerrorlast());
						retval = 1;
						goto cleanup;
					}
					if(alpm_trans_init(PM_TRANS_TYPE_SYNC, config->flags,
					   cb_trans_evt, cb_trans_conv, cb_trans_progress) == -1) {
						fprintf(stderr, _("error: failed to init transaction (%s)\n"),
						    alpm_strerrorlast());
						return(1);
					}
					if(alpm_trans_addtarget("pacman") == -1) {
						fprintf(stderr, _("error: pacman: %s\n"), alpm_strerrorlast());
						retval = 1;
						goto cleanup;
					}
					break;
				}
			}
		}
	} else {
		alpm_list_t *i;

		/* process targets */
		for(i = targets; i; i = alpm_list_next(i)) {
			char *targ = alpm_list_getdata(i);
			if(alpm_trans_addtarget(targ) == -1) {
				pmgrp_t *grp = NULL;
				int found=0;
				alpm_list_t *j;

				if(pm_errno == PM_ERR_TRANS_DUP_TARGET) {
					/* just ignore duplicate targets */
					continue;
				}
				if(pm_errno != PM_ERR_PKG_NOT_FOUND) {
					fprintf(stderr, _("error: '%s': %s\n"),
					        (char *)i->data, alpm_strerrorlast());
					retval = 1;
					goto cleanup;
				}
				/* target not found: check if it's a group */

				for(j = sync_dbs; j; j = alpm_list_next(j)) {
					pmdb_t *db = alpm_list_getdata(j);
					grp = alpm_db_readgrp(db, targ);
					if(grp) {
						alpm_list_t *k;

						found++;
						printf(_(":: group %s:\n"), targ);
						/* remove dupe entries in case a package exists in multiple repos */
						const alpm_list_t *grppkgs = alpm_grp_get_pkgs(grp);
						alpm_list_t *pkgs = alpm_list_remove_dupes(grppkgs);
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
					fprintf(stderr, _("error: '%s': not found in sync db\n"), targ);
					retval = 1;
					goto cleanup;
				}
			}
		}
	}

	/* Step 2: "compute" the transaction based on targets and flags */
	if(alpm_trans_prepare(&data) == -1) {
		fprintf(stderr, _("error: failed to prepare transaction (%s)\n"),
		        alpm_strerrorlast());
		switch(pm_errno) {
			alpm_list_t *i;
			case PM_ERR_UNSATISFIED_DEPS:
				for(i = data; i; i = alpm_list_next(i)) {
					pmdepmissing_t *miss = alpm_list_getdata(i);
					pmdepend_t *dep = alpm_miss_get_dep(miss);
					char *depstring = alpm_dep_get_string(dep);
					printf(_(":: %s: requires %s\n"), alpm_miss_get_target(miss),
							depstring);
					free(depstring);
				}
				break;
			case PM_ERR_CONFLICTING_DEPS:
			  for(i = data; i; i = alpm_list_next(i)) {
					pmconflict_t *conflict = alpm_list_getdata(i);
					printf(_(":: %s: conflicts with %s\n"),
							alpm_conflict_get_package1(conflict), alpm_conflict_get_package2(conflict));
				}
				break;
			default:
				break;
		}
		retval = 1;
		goto cleanup;
	}

	alpm_list_t *packages = alpm_trans_get_pkgs();
	if(packages == NULL) {
		/* nothing to do: just exit without complaining */
		printf(_(" local database is up to date\n"));
		goto cleanup;
	}

	if(!(alpm_trans_get_flags() & PM_TRANS_FLAG_PRINTURIS)) {
		int confirm;

		display_targets(packages, db_local);
		printf("\n");

		if(config->op_s_downloadonly) {
			if(config->noconfirm) {
				printf(_("Beginning download...\n"));
				confirm = 1;
			} else {
				confirm = yesno(_("Proceed with download? [Y/n] "));
			}
		} else {
			if(config->noconfirm) {
				printf(_("Beginning upgrade process...\n"));
				confirm = 1;
			} else {
				confirm = yesno(_("Proceed with installation? [Y/n] "));
			}
		}
		if(!confirm) {
			goto cleanup;
		}
	}/* else 'print uris' requested.  We're done at this point */

	/* Step 3: actually perform the installation */
	if(alpm_trans_commit(&data) == -1) {
		fprintf(stderr, _("error: failed to commit transaction (%s)\n"),
		        alpm_strerrorlast());
		switch(pm_errno) {
			alpm_list_t *i;
			case PM_ERR_FILE_CONFLICTS:
				for(i = data; i; i = alpm_list_next(i)) {
					pmfileconflict_t *conflict = alpm_list_getdata(i);
					switch(alpm_fileconflict_get_type(conflict)) {
						case PM_FILECONFLICT_TARGET:
							printf(_("%s exists in both '%s' and '%s'\n"),
									alpm_fileconflict_get_file(conflict),
									alpm_fileconflict_get_target(conflict),
									alpm_fileconflict_get_ctarget(conflict));
							break;
						case PM_FILECONFLICT_FILESYSTEM:
							printf(_("%s: %s exists in filesystem\n"),
									alpm_fileconflict_get_target(conflict),
									alpm_fileconflict_get_file(conflict));
							break;
					}
				}
				break;
			case PM_ERR_PKG_CORRUPTED:
				for(i = data; i; i = alpm_list_next(i)) {
					printf("%s", (char*)alpm_list_getdata(i));
				}
				break;
			default:
				break;
		}
		/* TODO: stderr? */
		printf(_("Errors occurred, no packages were upgraded.\n"));
		retval = 1;
		goto cleanup;
	}

	/* Step 4: release transaction resources */
cleanup:
	if(data) {
		FREELIST(data);
	}
	if(alpm_trans_release() == -1) {
		fprintf(stderr, _("error: failed to release transaction (%s)\n"),
		        alpm_strerrorlast());
		retval = 1;
	}

	return(retval);
}

int pacman_sync(alpm_list_t *targets)
{
	alpm_list_t *sync_dbs = NULL;
	int sync_only = 0;

	/* clean the cache */
	if(config->op_s_clean) {
		int ret = sync_cleancache(config->op_s_clean);
		ret += sync_cleandb_all();
		return(ret);
	}

	/* ensure we have at least one valid sync db set up */
	sync_dbs = alpm_option_get_syncdbs();
	if(sync_dbs == NULL || alpm_list_count(sync_dbs) == 0) {
		pm_printf(PM_LOG_ERROR, _("no usable package repositories configured.\n"));
		return(1);
	}

	if(config->op_s_search || config->group
			|| config->op_s_info || config->op_q_list) {
		sync_only = 1;
	} else if(targets == NULL && !(config->op_s_sync || config->op_s_upgrade)) {
		/* don't proceed here unless we have an operation that doesn't require
		 * a target list */
		pm_printf(PM_LOG_ERROR, _("no targets specified (use -h for help)\n"));
		return(1);
	}

	if(needs_transaction()) {
		if(sync_trans(targets, sync_only) == 1) {
			return(1);
		}
	}

	/* search for a package */
	if(config->op_s_search) {
		return(sync_search(sync_dbs, targets));
	}

	/* look for groups */
	if(config->group) {
		return(sync_group(config->group, sync_dbs, targets));
	}

	/* get package info */
	if(config->op_s_info) {
		return(sync_info(sync_dbs, targets));
	}

	/* get a listing of files in sync DBs */
	if(config->op_q_list) {
		return(sync_list(sync_dbs, targets));
	}

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
