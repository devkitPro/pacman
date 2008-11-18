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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "pacman.h"
#include "util.h"
#include "package.h"
#include "conf.h"

extern pmdb_t *db_local;

/* if keep_used != 0, then the dirnames which match an used syncdb
 * will be kept  */
static int sync_cleandb(const char *dbpath, int keep_used) {
	DIR *dir;
	struct dirent *ent;

	dir = opendir(dbpath);
	if(dir == NULL) {
		pm_fprintf(stderr, PM_LOG_ERROR, _("could not access database directory\n"));
		return(1);
	}

	rewinddir(dir);
	/* step through the directory one file at a time */
	while((ent = readdir(dir)) != NULL) {
		char path[PATH_MAX];
		struct stat buf;
		alpm_list_t *syncdbs = NULL, *i;
		int found = 0;
		const char *dname = ent->d_name;

		if(!strcmp(dname, ".") || !strcmp(dname, "..")) {
			continue;
		}
		/* skip the local and sync directories */
		if(!strcmp(dname, "sync") || !strcmp(dname, "local")) {
			continue;
		}

		/* build the full path */
		snprintf(path, PATH_MAX, "%s%s", dbpath, dname);
		/* skip entries that are not dirs (lock file, etc.) */
		stat(path, &buf);
		if(!S_ISDIR(buf.st_mode)) {
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
			if(!yesno(1, _("Do you want to remove %s?"), path)) {
				continue;
			}

			if(rmrf(path)) {
				pm_fprintf(stderr, PM_LOG_ERROR,
					_("could not remove repository directory\n"));
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
	if(!yesno(1, _("Do you want to remove unused repositories?"))) {
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
		/* Open up each package and see if it should be deleted,
		 * depending on the clean method used */
		printf(_("Cache directory: %s\n"), cachedir);
		switch(config->cleanmethod) {
			case PM_CLEAN_KEEPINST:
				if(!yesno(1, _("Do you want to remove uninstalled packages from cache?"))) {
					return(0);
				}
				break;
			case PM_CLEAN_KEEPCUR:
				if(!yesno(1, _("Do you want to remove outdated packages from cache?"))) {
					return(0);
				}
				break;
			default:
				/* this should not happen : the config parsing doesn't set any other value */
				return(1);
		}
		printf(_("removing old packages from cache... "));

		dir = opendir(cachedir);
		if(dir == NULL) {
			pm_fprintf(stderr, PM_LOG_ERROR, _("could not access cache directory\n"));
			return(1);
		}

		rewinddir(dir);
		/* step through the directory one file at a time */
		while((ent = readdir(dir)) != NULL) {
			char path[PATH_MAX];
			int delete = 1;
			pmpkg_t *localpkg = NULL, *pkg = NULL;
			alpm_list_t *sync_dbs = alpm_option_get_syncdbs();
			alpm_list_t *j;

			if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
				continue;
			}
			/* build the full filepath */
			snprintf(path, PATH_MAX, "%s%s", cachedir, ent->d_name);

			/* attempt to load the package, skip file on failures as we may have
			 * files here that aren't valid packages. we also don't need a full
			 * load of the package, just the metadata. */
			if(alpm_pkg_load(path, 0, &localpkg) != 0 || localpkg == NULL) {
				continue;
			}
			switch(config->cleanmethod) {
				case PM_CLEAN_KEEPINST:
					/* check if this package is in the local DB */
					pkg = alpm_db_get_pkg(db_local, alpm_pkg_get_name(localpkg));
					if(pkg != NULL && alpm_pkg_vercmp(alpm_pkg_get_version(localpkg),
								alpm_pkg_get_version(pkg)) == 0) {
						/* package was found in local DB and version matches, keep it */
						delete = 0;
					}
					break;
				case PM_CLEAN_KEEPCUR:
					/* check if this package is in a sync DB */
					for(j = sync_dbs; j && delete; j = alpm_list_next(j)) {
						pmdb_t *db = alpm_list_getdata(j);
						pkg = alpm_db_get_pkg(db, alpm_pkg_get_name(localpkg));
						if(pkg != NULL && alpm_pkg_vercmp(alpm_pkg_get_version(localpkg),
									alpm_pkg_get_version(pkg)) == 0) {
							/* package was found in a sync DB and version matches, keep it */
							delete = 0;
						}
					}
					break;
				default:
					/* this should not happen : the config parsing doesn't set any other value */
					delete = 0;
					break;
			}
			/* free the local file package */
			alpm_pkg_free(localpkg);

			if(delete) {
				unlink(path);
			}
		}
		printf(_("done.\n"));
	} else {
		/* full cleanup */
		printf(_("Cache directory: %s\n"), cachedir);
		if(!yesno(0, _("Do you want to remove ALL packages from cache?"))) {
			return(0);
		}
		printf(_("removing all packages from cache... "));

		if(rmrf(cachedir)) {
			pm_fprintf(stderr, PM_LOG_ERROR, _("could not remove cache directory\n"));
			return(1);
		}

		if(makepath(cachedir)) {
			pm_fprintf(stderr, PM_LOG_ERROR, _("could not create new cache directory\n"));
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

	if(trans_init(PM_TRANS_TYPE_SYNC, 0) == -1) {
		return(0);
	}

	for(i = syncs; i; i = alpm_list_next(i)) {
		pmdb_t *db = alpm_list_getdata(i);

		ret = alpm_db_update((level < 2 ? 0 : 1), db);
		if(ret < 0) {
			pm_fprintf(stderr, PM_LOG_ERROR, _("failed to update %s (%s)\n"),
					alpm_db_get_name(db), alpm_strerrorlast());
		} else if(ret == 1) {
			printf(_(" %s is up to date\n"), alpm_db_get_name(db));
			success++;
		} else {
			success++;
		}
	}

	if(trans_release() == -1) {
		return(0);
	}
	/* We should always succeed if at least one DB was upgraded - we may possibly
	 * fail later with unresolved deps, but that should be rare, and would be
	 * expected
	 */
	if(!success) {
		pm_fprintf(stderr, PM_LOG_ERROR, _("failed to synchronize any databases\n"));
	}
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
			alpm_list_free(ret);
		}
	}

	return(!found);
}

static int sync_group(int level, alpm_list_t *syncs, alpm_list_t *targets)
{
	alpm_list_t *i, *j, *k;

	if(targets) {
		for(i = targets; i; i = alpm_list_next(i)) {
			const char *grpname = alpm_list_getdata(i);
			for(j = syncs; j; j = alpm_list_next(j)) {
				pmdb_t *db = alpm_list_getdata(j);
				pmgrp_t *grp = alpm_db_readgrp(db, grpname);

				if(grp) {
					/* get names of packages in group */
					for(k = alpm_grp_get_pkgs(grp); k; k = alpm_list_next(k)) {
						if(!config->quiet) {
							printf("%s %s\n", grpname,
									alpm_pkg_get_name(alpm_list_getdata(k)));
						} else {
							printf("%s\n", alpm_pkg_get_name(alpm_list_getdata(k)));
						}
					}
				}
			}
		}
	} else {
		for(i = syncs; i; i = alpm_list_next(i)) {
			pmdb_t *db = alpm_list_getdata(i);

			for(j = alpm_db_getgrpcache(db); j; j = alpm_list_next(j)) {
				pmgrp_t *grp = alpm_list_getdata(j);
				const char *grpname = alpm_grp_get_name(grp);

				if(level > 1) {
					for(k = alpm_grp_get_pkgs(grp); k; k = alpm_list_next(k)) {
						printf("%s %s\n", grpname,
								alpm_pkg_get_name(alpm_list_getdata(k)));
					}
				} else {
					/* print grp names only, no package names */
					printf("%s\n", grpname);
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
					pm_fprintf(stderr, PM_LOG_ERROR,
						_("repository '%s' does not exist\n"), repo);
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
					pm_fprintf(stderr, PM_LOG_ERROR,
						_("package '%s' was not found in repository '%s'\n"), pkgstr, repo);
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
					pm_fprintf(stderr, PM_LOG_ERROR,
						_("package '%s' was not found\n"), pkgstr);
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
				pm_fprintf(stderr, PM_LOG_ERROR,
					_("repository \"%s\" was not found.\n"),repo);
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

static alpm_list_t *syncfirst() {
	alpm_list_t *i, *res = NULL;

	for(i = config->syncfirst; i; i = alpm_list_next(i)) {
		char *pkgname = alpm_list_getdata(i);
		pmpkg_t *pkg = alpm_db_get_pkg(alpm_option_get_localdb(), pkgname);
		if(pkg == NULL) {
			continue;
		}

		if(alpm_sync_newversion(pkg, alpm_option_get_syncdbs())) {
			res = alpm_list_add(res, strdup(pkgname));
		}
	}

	return(res);
}

static int sync_trans(alpm_list_t *targets)
{
	int retval = 0;
	alpm_list_t *data = NULL;
	alpm_list_t *sync_dbs = alpm_option_get_syncdbs();
	alpm_list_t *packages = NULL;

	/* Step 1: create a new transaction... */
	if(trans_init(PM_TRANS_TYPE_SYNC, config->flags) == -1) {
		return(1);
	}

	if(config->op_s_upgrade) {
		printf(_(":: Starting full system upgrade...\n"));
		alpm_logaction("starting full system upgrade\n");
		if(alpm_trans_sysupgrade() == -1) {
			pm_fprintf(stderr, PM_LOG_ERROR, "%s\n", alpm_strerrorlast());
			retval = 1;
			goto cleanup;
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
					pm_fprintf(stderr, PM_LOG_ERROR, "'%s': %s\n",
							targ, alpm_strerrorlast());
					retval = 1;
					goto cleanup;
				}
				/* target not found: check if it's a group */
				printf(_("%s package not found, searching for group...\n"), targ);
				for(j = sync_dbs; j; j = alpm_list_next(j)) {
					pmdb_t *db = alpm_list_getdata(j);
					grp = alpm_db_readgrp(db, targ);
					if(grp) {
						alpm_list_t *k, *pkgnames = NULL;

						found++;
						printf(_(":: group %s (including ignored packages):\n"), targ);
						/* remove dupe entries in case a package exists in multiple repos */
						alpm_list_t *grppkgs = alpm_grp_get_pkgs(grp);
						alpm_list_t *pkgs = alpm_list_remove_dupes(grppkgs);
						for(k = pkgs; k; k = alpm_list_next(k)) {
							pkgnames = alpm_list_add(pkgnames,
									(char*)alpm_pkg_get_name(k->data));
						}
						list_display("   ", pkgnames);
						if(yesno(1, _(":: Install whole content?"))) {
							for(k = pkgnames; k; k = alpm_list_next(k)) {
								targets = alpm_list_add(targets, strdup(alpm_list_getdata(k)));
							}
						} else {
							for(k = pkgnames; k; k = alpm_list_next(k)) {
								char *pkgname = alpm_list_getdata(k);
								if(yesno(1, _(":: Install %s from group %s?"), pkgname, targ)) {
									targets = alpm_list_add(targets, strdup(pkgname));
								}
							}
						}
						alpm_list_free(pkgnames);
						alpm_list_free(pkgs);
					}
				}
				if(!found) {
					pm_fprintf(stderr, PM_LOG_ERROR, _("'%s': not found in sync db\n"), targ);
					retval = 1;
					goto cleanup;
				}
			}
		}
	}

	/* Step 2: "compute" the transaction based on targets and flags */
	if(alpm_trans_prepare(&data) == -1) {
		pm_fprintf(stderr, PM_LOG_ERROR, _("failed to prepare transaction (%s)\n"),
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

	packages = alpm_trans_get_pkgs();
	if(packages == NULL) {
		/* nothing to do: just exit without complaining */
		printf(_(" local database is up to date\n"));
		goto cleanup;
	}

	if(!(alpm_trans_get_flags() & PM_TRANS_FLAG_PRINTURIS)) {
		int confirm;

		display_synctargets(packages);
		printf("\n");

		if(config->op_s_downloadonly) {
			confirm = yesno(1, _("Proceed with download?"));
		} else {
			confirm = yesno(1, _("Proceed with installation?"));
		}
		if(!confirm) {
			goto cleanup;
		}
	}/* else 'print uris' requested.  We're done at this point */

	/* Step 3: actually perform the installation */
	if(alpm_trans_commit(&data) == -1) {
		pm_fprintf(stderr, PM_LOG_ERROR, _("failed to commit transaction (%s)\n"),
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
			case PM_ERR_PKG_INVALID:
			case PM_ERR_DLT_INVALID:
				for(i = data; i; i = alpm_list_next(i)) {
					char *filename = alpm_list_getdata(i);
					printf(_("%s is invalid or corrupted\n"), filename);
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
	if(trans_release() == -1) {
		retval = 1;
	}

	return(retval);
}

int pacman_sync(alpm_list_t *targets)
{
	alpm_list_t *sync_dbs = NULL;

	/* Display only errors with -Sp and -Sw operations */
	if(config->flags & (PM_TRANS_FLAG_DOWNLOADONLY | PM_TRANS_FLAG_PRINTURIS)) {
		config->logmask &= ~PM_LOG_WARNING;
	}

	/* clean the cache */
	if(config->op_s_clean) {
		int ret = 0;

		if(trans_init(PM_TRANS_TYPE_SYNC, 0) == -1) {
			return(1);
		}

		ret += sync_cleancache(config->op_s_clean);
		ret += sync_cleandb_all();

		if(trans_release() == -1) {
			ret++;
		}

		return(ret);
	}

	/* ensure we have at least one valid sync db set up */
	sync_dbs = alpm_option_get_syncdbs();
	if(sync_dbs == NULL || alpm_list_count(sync_dbs) == 0) {
		pm_printf(PM_LOG_ERROR, _("no usable package repositories configured.\n"));
		return(1);
	}

	if(config->op_s_sync) {
		/* grab a fresh package list */
		printf(_(":: Synchronizing package databases...\n"));
		alpm_logaction("synchronizing package lists\n");
		if(!sync_synctree(config->op_s_sync, sync_dbs)) {
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

	if(targets == NULL) {
		if(config->op_s_upgrade) {
			/* proceed */
		} else if(config->op_s_sync) {
			return(0);
		} else {
			/* don't proceed here unless we have an operation that doesn't require a
			 * target list */
			pm_printf(PM_LOG_ERROR, _("no targets specified (use -h for help)\n"));
			return(1);
		}
	}

	alpm_list_t *targs = alpm_list_strdup(targets);
	if(!(config->flags & (PM_TRANS_FLAG_DOWNLOADONLY | PM_TRANS_FLAG_PRINTURIS))) {
		/* check for newer versions of packages to be upgraded first */
		alpm_list_t *packages = syncfirst();
		if(packages) {
			printf(_(":: The following packages should be upgraded first :\n"));
			list_display("   ", packages);
			if(yesno(1, _(":: Do you want to cancel the current operation\n"
							":: and upgrade these packages now?"))) {
				FREELIST(targs);
				targs = packages;
				config->flags = 0;
				config->op_s_upgrade = 0;
			} else {
				FREELIST(packages);
			}
			printf("\n");
		}
	}

	int ret = sync_trans(targs);
	FREELIST(targs);

	return(ret);
}

/* vim: set ts=2 sw=2 noet: */
