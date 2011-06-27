/*
 *  sync.c
 *
 *  Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
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

/* if keep_used != 0, then the db files which match an used syncdb
 * will be kept  */
static int sync_cleandb(const char *dbpath, int keep_used)
{
	DIR *dir;
	struct dirent *ent;
	alpm_list_t *syncdbs;

	dir = opendir(dbpath);
	if(dir == NULL) {
		pm_fprintf(stderr, PM_LOG_ERROR, _("could not access database directory\n"));
		return 1;
	}

	syncdbs = alpm_option_get_syncdbs(config->handle);

	rewinddir(dir);
	/* step through the directory one file at a time */
	while((ent = readdir(dir)) != NULL) {
		char path[PATH_MAX];
		struct stat buf;
		int found = 0;
		const char *dname = ent->d_name;
		size_t len;

		if(strcmp(dname, ".") == 0 || strcmp(dname, "..") == 0) {
			continue;
		}
		/* skip the local and sync directories */
		if(strcmp(dname, "sync") == 0 || strcmp(dname, "local") == 0) {
			continue;
		}
		/* skip the db.lck file */
		if(strcmp(dname, "db.lck") == 0) {
			continue;
		}

		/* build the full path */
		snprintf(path, PATH_MAX, "%s%s", dbpath, dname);

		/* remove all non-skipped directories and non-database files */
		stat(path, &buf);
		len = strlen(path);
		if(S_ISDIR(buf.st_mode) || strcmp(path + len - 3, ".db") != 0) {
			if(rmrf(path)) {
				pm_fprintf(stderr, PM_LOG_ERROR,
					_("could not remove %s\n"), path);
				closedir(dir);
				return 1;
			}
			continue;
		}

		if(keep_used) {
			alpm_list_t *i;
			len = strlen(dname);
			char *dbname = strndup(dname, len - 3);
			for(i = syncdbs; i && !found; i = alpm_list_next(i)) {
				pmdb_t *db = alpm_list_getdata(i);
				found = !strcmp(dbname, alpm_db_get_name(db));
			}
			free(dbname);
		}
		/* We have a database that doesn't match any syncdb.
		 * Ask the user if he wants to remove it. */
		if(!found) {
			if(!yesno(_("Do you want to remove %s?"), path)) {
				continue;
			}

			if(rmrf(path)) {
				pm_fprintf(stderr, PM_LOG_ERROR,
					_("could not remove %s\n"), path);
				closedir(dir);
				return 1;
			}
		}
	}
	closedir(dir);
	return 0;
}

static int sync_cleandb_all(void)
{
	const char *dbpath;
	char newdbpath[PATH_MAX];
	int ret = 0;

	dbpath = alpm_option_get_dbpath(config->handle);
	printf(_("Database directory: %s\n"), dbpath);
	if(!yesno(_("Do you want to remove unused repositories?"))) {
		return 0;
	}
	/* The sync dbs were previously put in dbpath/ but are now in dbpath/sync/.
	 * We will clean everything in dbpath/ except local/, sync/ and db.lck, and
	 * only the unused sync dbs in dbpath/sync/ */
	ret += sync_cleandb(dbpath, 0);

	sprintf(newdbpath, "%s%s", dbpath, "sync/");
	ret += sync_cleandb(newdbpath, 1);

	printf(_("Database directory cleaned up\n"));
	return ret;
}

static int sync_cleancache(int level)
{
	alpm_list_t *i;
	alpm_list_t *sync_dbs = alpm_option_get_syncdbs(config->handle);
	pmdb_t *db_local = alpm_option_get_localdb(config->handle);
	alpm_list_t *cachedirs = alpm_option_get_cachedirs(config->handle);
	int ret = 0;

	for(i = cachedirs; i; i = alpm_list_next(i)) {
		printf(_("Cache directory: %s\n"), (char *)alpm_list_getdata(i));
	}

	if(!config->cleanmethod) {
		/* default to KeepInstalled if user did not specify */
		config->cleanmethod = PM_CLEAN_KEEPINST;
	}

	if(level == 1) {
		printf(_("Packages to keep:\n"));
		if(config->cleanmethod & PM_CLEAN_KEEPINST) {
			printf(_("  All locally installed packages\n"));
		}
		if(config->cleanmethod & PM_CLEAN_KEEPCUR) {
			printf(_("  All current sync database packages\n"));
		}
		if(!yesno(_("Do you want to remove all other packages from cache?"))) {
			return 0;
		}
		printf(_("removing old packages from cache...\n"));
	} else {
		if(!noyes(_("Do you want to remove ALL files from cache?"))) {
			return 0;
		}
		printf(_("removing all files from cache...\n"));
	}

	for(i = cachedirs; i; i = alpm_list_next(i)) {
		const char *cachedir = alpm_list_getdata(i);
		DIR *dir = opendir(cachedir);
		struct dirent *ent;

		if(dir == NULL) {
			pm_fprintf(stderr, PM_LOG_ERROR,
					_("could not access cache directory %s\n"), cachedir);
			ret++;
			continue;
		}

		rewinddir(dir);
		/* step through the directory one file at a time */
		while((ent = readdir(dir)) != NULL) {
			char path[PATH_MAX];
			size_t pathlen;
			int delete = 1;
			pmpkg_t *localpkg = NULL, *pkg = NULL;
			const char *local_name, *local_version;

			if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
				continue;
			}
			/* build the full filepath */
			snprintf(path, PATH_MAX, "%s%s", cachedir, ent->d_name);

			/* short circuit for removing all packages from cache */
			if(level > 1) {
				unlink(path);
				continue;
			}

			/* we handle .sig files with packages, not separately */
			pathlen = strlen(path);
			if(strcmp(path + pathlen - 4, ".sig") == 0) {
				continue;
			}

			/* attempt to load the package, prompt removal on failures as we may have
			 * files here that aren't valid packages. we also don't need a full
			 * load of the package, just the metadata. */
			if(alpm_pkg_load(config->handle, path, 0, PM_PGP_VERIFY_NEVER, &localpkg) != 0
					|| localpkg == NULL) {
				if(yesno(_("File %s does not seem to be a valid package, remove it?"),
							path)) {
					if(localpkg) {
						alpm_pkg_free(localpkg);
					}
					unlink(path);
				}
				continue;
			}
			local_name = alpm_pkg_get_name(localpkg);
			local_version = alpm_pkg_get_version(localpkg);

			if(config->cleanmethod & PM_CLEAN_KEEPINST) {
				/* check if this package is in the local DB */
				pkg = alpm_db_get_pkg(db_local, local_name);
				if(pkg != NULL && alpm_pkg_vercmp(local_version,
							alpm_pkg_get_version(pkg)) == 0) {
					/* package was found in local DB and version matches, keep it */
					pm_printf(PM_LOG_DEBUG, "pkg %s-%s found in local db\n",
							local_name, local_version);
					delete = 0;
				}
			}
			if(config->cleanmethod & PM_CLEAN_KEEPCUR) {
				alpm_list_t *j;
				/* check if this package is in a sync DB */
				for(j = sync_dbs; j && delete; j = alpm_list_next(j)) {
					pmdb_t *db = alpm_list_getdata(j);
					pkg = alpm_db_get_pkg(db, local_name);
					if(pkg != NULL && alpm_pkg_vercmp(local_version,
								alpm_pkg_get_version(pkg)) == 0) {
						/* package was found in a sync DB and version matches, keep it */
						pm_printf(PM_LOG_DEBUG, "pkg %s-%s found in sync db\n",
								local_name, local_version);
						delete = 0;
					}
				}
			}
			/* free the local file package */
			alpm_pkg_free(localpkg);

			if(delete) {
				unlink(path);
				/* unlink a signature file if present too */
				if(PATH_MAX - 5 >= pathlen) {
					strcpy(path + pathlen, ".sig");
					unlink(path);
				}
			}
		}
		closedir(dir);
	}

	return ret;
}

static int sync_synctree(int level, alpm_list_t *syncs)
{
	alpm_list_t *i;
	int success = 0, ret;

	for(i = syncs; i; i = alpm_list_next(i)) {
		pmdb_t *db = alpm_list_getdata(i);

		ret = alpm_db_update((level < 2 ? 0 : 1), db);
		if(ret < 0) {
			pm_fprintf(stderr, PM_LOG_ERROR, _("failed to update %s (%s)\n"),
					alpm_db_get_name(db), alpm_strerror(alpm_errno(config->handle)));
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
	if(!success) {
		pm_fprintf(stderr, PM_LOG_ERROR, _("failed to synchronize any databases\n"));
	}
	return (success > 0);
}

static void print_installed(pmdb_t *db_local, pmpkg_t *pkg)
{
	const char *pkgname = alpm_pkg_get_name(pkg);
	const char *pkgver = alpm_pkg_get_version(pkg);
	pmpkg_t *lpkg = alpm_db_get_pkg(db_local, pkgname);
	if(lpkg) {
		const char *lpkgver = alpm_pkg_get_version(lpkg);
		if(strcmp(lpkgver,pkgver) == 0) {
			printf(" [%s]", _("installed"));
		} else {
			printf(" [%s: %s]", _("installed"), lpkgver);
		}
	}
}

/* search the sync dbs for a matching package */
static int sync_search(alpm_list_t *syncs, alpm_list_t *targets)
{
	alpm_list_t *i, *j, *ret;
	int freelist;
	int found = 0;
	pmdb_t *db_local = alpm_option_get_localdb(config->handle);

	for(i = syncs; i; i = alpm_list_next(i)) {
		pmdb_t *db = alpm_list_getdata(i);
		/* if we have a targets list, search for packages matching it */
		if(targets) {
			ret = alpm_db_search(db, targets);
			freelist = 1;
		} else {
			ret = alpm_db_get_pkgcache(db);
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

			if(!config->quiet) {
				printf("%s/%s %s", alpm_db_get_name(db), alpm_pkg_get_name(pkg),
							 alpm_pkg_get_version(pkg));
			} else {
				printf("%s", alpm_pkg_get_name(pkg));
			}

			if(!config->quiet) {
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

				print_installed(db_local, pkg);

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

	return !found;
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
					for(k = grp->packages; k; k = alpm_list_next(k)) {
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

			for(j = alpm_db_get_grpcache(db); j; j = alpm_list_next(j)) {
				pmgrp_t *grp = alpm_list_getdata(j);

				if(level > 1) {
					for(k = grp->packages; k; k = alpm_list_next(k)) {
						printf("%s %s\n", grp->name,
								alpm_pkg_get_name(alpm_list_getdata(k)));
					}
				} else {
					/* print grp names only, no package names */
					printf("%s\n", grp->name);
				}
			}
		}
	}

	return 0;
}

static int sync_info(alpm_list_t *syncs, alpm_list_t *targets)
{
	alpm_list_t *i, *j, *k;
	int ret = 0;

	if(targets) {
		for(i = targets; i; i = alpm_list_next(i)) {
			int foundpkg = 0;

			char target[512]; /* TODO is this enough space? */
			char *repo = NULL, *pkgstr = NULL;

			strncpy(target, i->data, 512);
			pkgstr = strchr(target, '/');
			if(pkgstr) {
				pmdb_t *db = NULL;
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
					return 1;
				}

				for(k = alpm_db_get_pkgcache(db); k; k = alpm_list_next(k)) {
					pmpkg_t *pkg = alpm_list_getdata(k);

					if(strcmp(alpm_pkg_get_name(pkg), pkgstr) == 0) {
						dump_pkg_full(pkg, PKG_FROM_SYNCDB, config->op_s_info > 1);
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

					for(k = alpm_db_get_pkgcache(db); k; k = alpm_list_next(k)) {
						pmpkg_t *pkg = alpm_list_getdata(k);

						if(strcmp(alpm_pkg_get_name(pkg), pkgstr) == 0) {
							dump_pkg_full(pkg, PKG_FROM_SYNCDB, config->op_s_info > 1);
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

			for(j = alpm_db_get_pkgcache(db); j; j = alpm_list_next(j)) {
				pmpkg_t *pkg = alpm_list_getdata(j);
				dump_pkg_full(pkg, PKG_FROM_SYNCDB, config->op_s_info > 1);
			}
		}
	}

	return ret;
}

static int sync_list(alpm_list_t *syncs, alpm_list_t *targets)
{
	alpm_list_t *i, *j, *ls = NULL;
	pmdb_t *db_local = alpm_option_get_localdb(config->handle);

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
				return 1;
			}

			ls = alpm_list_add(ls, db);
		}
	} else {
		ls = syncs;
	}

	for(i = ls; i; i = alpm_list_next(i)) {
		pmdb_t *db = alpm_list_getdata(i);

		for(j = alpm_db_get_pkgcache(db); j; j = alpm_list_next(j)) {
			pmpkg_t *pkg = alpm_list_getdata(j);

			if(!config->quiet) {
				printf("%s %s %s", alpm_db_get_name(db), alpm_pkg_get_name(pkg),
						alpm_pkg_get_version(pkg));
				print_installed(db_local, pkg);
				printf("\n");
			} else {
				printf("%s\n", alpm_pkg_get_name(pkg));
			}
		}
	}

	if(targets) {
		alpm_list_free(ls);
	}

	return 0;
}

static alpm_list_t *syncfirst(void) {
	alpm_list_t *i, *res = NULL;
	pmdb_t *db_local = alpm_option_get_localdb(config->handle);
	alpm_list_t *syncdbs = alpm_option_get_syncdbs(config->handle);

	for(i = config->syncfirst; i; i = alpm_list_next(i)) {
		char *pkgname = alpm_list_getdata(i);
		pmpkg_t *pkg = alpm_db_get_pkg(db_local, pkgname);
		if(pkg == NULL) {
			continue;
		}

		if(alpm_sync_newversion(pkg, syncdbs)) {
			res = alpm_list_add(res, strdup(pkgname));
		}
	}

	return res;
}

static pmdb_t *get_db(const char *dbname)
{
	alpm_list_t *i;
	for(i = alpm_option_get_syncdbs(config->handle); i; i = i->next) {
		pmdb_t *db = i->data;
		if(strcmp(alpm_db_get_name(db), dbname) == 0) {
			return db;
		}
	}
	return NULL;
}

static int process_pkg(pmpkg_t *pkg)
{
	int ret = alpm_add_pkg(config->handle, pkg);

	if(ret == -1) {
		enum _pmerrno_t err = alpm_errno(config->handle);
		if(err == PM_ERR_TRANS_DUP_TARGET
				|| err == PM_ERR_PKG_IGNORED) {
			/* just skip duplicate or ignored targets */
			pm_printf(PM_LOG_WARNING, _("skipping target: %s\n"), alpm_pkg_get_name(pkg));
			return 0;
		} else {
			pm_fprintf(stderr, PM_LOG_ERROR, "'%s': %s\n", alpm_pkg_get_name(pkg),
					alpm_strerror(err));
			return 1;
		}
	}
	return 0;
}

static int process_group(alpm_list_t *dbs, char *group)
{
	int ret = 0;
	alpm_list_t *i;
	alpm_list_t *pkgs = alpm_find_grp_pkgs(dbs, group);
	int count = alpm_list_count(pkgs);

	if(!count) {
		pm_fprintf(stderr, PM_LOG_ERROR, _("target not found: %s\n"), group);
		return 1;
	}


	if(config->print == 0) {
		printf(_(":: There are %d members in group %s:\n"), count,
				group);
		select_display(pkgs);
		char *array = malloc(count);
		multiselect_question(array, count);
		int n = 0;
		for(i = pkgs; i; i = alpm_list_next(i)) {
			if(array[n++] == 0)
				continue;
			pmpkg_t *pkg = alpm_list_getdata(i);

			if(process_pkg(pkg) == 1) {
				ret = 1;
				free(array);
				goto cleanup;
			}
		}
	} else {
		for(i = pkgs; i; i = alpm_list_next(i)) {
			pmpkg_t *pkg = alpm_list_getdata(i);

			if(process_pkg(pkg) == 1) {
				ret = 1;
				goto cleanup;
			}
		}
	}
cleanup:
	alpm_list_free(pkgs);
	return ret;
}

static int process_targname(alpm_list_t *dblist, char *targname)
{
	pmpkg_t *pkg = alpm_find_dbs_satisfier(config->handle, dblist, targname);

	/* #FS#23342 - skip ignored packages when user says no */
	if(alpm_errno(config->handle) == PM_ERR_PKG_IGNORED) {
			pm_printf(PM_LOG_WARNING, _("skipping target: %s\n"), targname);
			/* TODO how to do this, we shouldn't be fucking with it from the frontend */
			/* pm_errno = 0; */
			return 0;
	}

	if(pkg) {
		return process_pkg(pkg);
	}
	/* fallback on group */
	return process_group(dblist, targname);
}

static int process_target(char *target)
{
	/* process targets */
	char *targstring = strdup(target);
	char *targname = strchr(targstring, '/');
	char *dbname = NULL;
	int ret = 0;
	alpm_list_t *dblist = NULL;

	if(targname) {
		pmdb_t *db = NULL;

		*targname = '\0';
		targname++;
		dbname = targstring;
		db = get_db(dbname);
		if(!db) {
			pm_fprintf(stderr, PM_LOG_ERROR, _("database not found: %s\n"),
					dbname);
			ret = 1;
			goto cleanup;
		}
		dblist = alpm_list_add(dblist, db);
		ret = process_targname(dblist, targname);
		alpm_list_free(dblist);
	} else {
		targname = targstring;
		dblist = alpm_option_get_syncdbs(config->handle);
		ret = process_targname(dblist, targname);
	}
cleanup:
	free(targstring);
	return ret;
}

static int sync_trans(alpm_list_t *targets)
{
	int retval = 0;
	alpm_list_t *data = NULL;
	alpm_list_t *packages = NULL;
	alpm_list_t *i;

	/* Step 1: create a new transaction... */
	if(trans_init(config->flags) == -1) {
		return 1;
	}

	/* process targets */
	for(i = targets; i; i = alpm_list_next(i)) {
		char *targ = alpm_list_getdata(i);
		if(process_target(targ) == 1) {
			retval = 1;
			goto cleanup;
		}
	}

	if(config->op_s_upgrade) {
		printf(_(":: Starting full system upgrade...\n"));
		alpm_logaction(config->handle, "starting full system upgrade\n");
		if(alpm_sync_sysupgrade(config->handle, config->op_s_upgrade >= 2) == -1) {
			pm_fprintf(stderr, PM_LOG_ERROR, "%s\n", alpm_strerror(alpm_errno(config->handle)));
			retval = 1;
			goto cleanup;
		}
	}

	/* Step 2: "compute" the transaction based on targets and flags */
	if(alpm_trans_prepare(config->handle, &data) == -1) {
		enum _pmerrno_t err = alpm_errno(config->handle);
		pm_fprintf(stderr, PM_LOG_ERROR, _("failed to prepare transaction (%s)\n"),
		        alpm_strerror(err));
		switch(err) {
			case PM_ERR_PKG_INVALID_ARCH:
				for(i = data; i; i = alpm_list_next(i)) {
					char *pkg = alpm_list_getdata(i);
					printf(_(":: package %s does not have a valid architecture\n"), pkg);
				}
				break;
			case PM_ERR_UNSATISFIED_DEPS:
				for(i = data; i; i = alpm_list_next(i)) {
					pmdepmissing_t *miss = alpm_list_getdata(i);
					char *depstring = alpm_dep_compute_string(miss->depend);
					printf(_(":: %s: requires %s\n"), miss->target, depstring);
					free(depstring);
				}
				break;
			case PM_ERR_CONFLICTING_DEPS:
				for(i = data; i; i = alpm_list_next(i)) {
					pmconflict_t *conflict = alpm_list_getdata(i);
					/* only print reason if it contains new information */
					if(strcmp(conflict->package1, conflict->reason) == 0 ||
							strcmp(conflict->package2, conflict->reason) == 0) {
						printf(_(":: %s and %s are in conflict\n"),
								conflict->package1, conflict->package2);
					} else {
						printf(_(":: %s and %s are in conflict (%s)\n"),
								conflict->package1, conflict->package2, conflict->reason);
					}
				}
				break;
			default:
				break;
		}
		retval = 1;
		goto cleanup;
	}

	packages = alpm_trans_get_add(config->handle);
	if(packages == NULL) {
		/* nothing to do: just exit without complaining */
		printf(_(" there is nothing to do\n"));
		goto cleanup;
	}

	/* Step 3: actually perform the operation */
	if(config->print) {
		print_packages(packages);
		goto cleanup;
	}

	display_targets(alpm_trans_get_remove(config->handle), 0);
	display_targets(alpm_trans_get_add(config->handle), 1);
	printf("\n");

	int confirm;
	if(config->op_s_downloadonly) {
		confirm = yesno(_("Proceed with download?"));
	} else {
		confirm = yesno(_("Proceed with installation?"));
	}
	if(!confirm) {
		goto cleanup;
	}

	if(alpm_trans_commit(config->handle, &data) == -1) {
		enum _pmerrno_t err = alpm_errno(config->handle);
		pm_fprintf(stderr, PM_LOG_ERROR, _("failed to commit transaction (%s)\n"),
		        alpm_strerror(err));
		switch(err) {
			case PM_ERR_FILE_CONFLICTS:
				for(i = data; i; i = alpm_list_next(i)) {
					pmfileconflict_t *conflict = alpm_list_getdata(i);
					switch(conflict->type) {
						case PM_FILECONFLICT_TARGET:
							printf(_("%s exists in both '%s' and '%s'\n"),
									conflict->file, conflict->target, conflict->ctarget);
							break;
						case PM_FILECONFLICT_FILESYSTEM:
							printf(_("%s: %s exists in filesystem\n"),
									conflict->target, conflict->file);
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

	return retval;
}

int pacman_sync(alpm_list_t *targets)
{
	alpm_list_t *sync_dbs = NULL;

	/* clean the cache */
	if(config->op_s_clean) {
		int ret = 0;

		if(trans_init(0) == -1) {
			return 1;
		}

		ret += sync_cleancache(config->op_s_clean);
		printf("\n");
		ret += sync_cleandb_all();

		if(trans_release() == -1) {
			ret++;
		}

		return ret;
	}

	/* ensure we have at least one valid sync db set up */
	sync_dbs = alpm_option_get_syncdbs(config->handle);
	if(sync_dbs == NULL) {
		pm_printf(PM_LOG_ERROR, _("no usable package repositories configured.\n"));
		return 1;
	}

	if(config->op_s_sync) {
		/* grab a fresh package list */
		printf(_(":: Synchronizing package databases...\n"));
		alpm_logaction(config->handle, "synchronizing package lists\n");
		if(!sync_synctree(config->op_s_sync, sync_dbs)) {
			return 1;
		}
	}

	/* search for a package */
	if(config->op_s_search) {
		return sync_search(sync_dbs, targets);
	}

	/* look for groups */
	if(config->group) {
		return sync_group(config->group, sync_dbs, targets);
	}

	/* get package info */
	if(config->op_s_info) {
		return sync_info(sync_dbs, targets);
	}

	/* get a listing of files in sync DBs */
	if(config->op_q_list) {
		return sync_list(sync_dbs, targets);
	}

	if(targets == NULL) {
		if(config->op_s_upgrade) {
			/* proceed */
		} else if(config->op_s_sync) {
			return 0;
		} else {
			/* don't proceed here unless we have an operation that doesn't require a
			 * target list */
			pm_printf(PM_LOG_ERROR, _("no targets specified (use -h for help)\n"));
			return 1;
		}
	}

	alpm_list_t *targs = alpm_list_strdup(targets);
	if(!(config->flags & PM_TRANS_FLAG_DOWNLOADONLY) && !config->print) {
		/* check for newer versions of packages to be upgraded first */
		alpm_list_t *packages = syncfirst();
		if(packages) {
			/* Do not ask user if all the -S targets are SyncFirst packages, see FS#15810 */
			alpm_list_t *tmp = NULL;
			if(config->op_s_upgrade || (tmp = alpm_list_diff(targets, packages, (alpm_list_fn_cmp)strcmp))) {
				alpm_list_free(tmp);
				printf(_(":: The following packages should be upgraded first :\n"));
				list_display("   ", packages);
				if(yesno(_(":: Do you want to cancel the current operation\n"
								":: and upgrade these packages now?"))) {
					FREELIST(targs);
					targs = packages;
					config->flags = 0;
					config->op_s_upgrade = 0;
				} else {
					FREELIST(packages);
				}
				printf("\n");
			} else {
				pm_printf(PM_LOG_DEBUG, "skipping SyncFirst dialog\n");
				FREELIST(packages);
			}
		}
	}

	int ret = sync_trans(targs);
	FREELIST(targs);

	return ret;
}

/* vim: set ts=2 sw=2 noet: */
