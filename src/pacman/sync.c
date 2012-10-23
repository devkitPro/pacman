/*
 *  sync.c
 *
 *  Copyright (c) 2006-2013 Pacman Development Team <pacman-dev@archlinux.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fnmatch.h>

#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "pacman.h"
#include "util.h"
#include "package.h"
#include "conf.h"

static int unlink_verbose(const char *pathname, int ignore_missing)
{
	int ret = unlink(pathname);
	if(ret) {
		if(ignore_missing && errno == ENOENT) {
			ret = 0;
		} else {
			pm_printf(ALPM_LOG_ERROR, _("could not remove %s: %s\n"),
					pathname, strerror(errno));
		}
	}
	return ret;
}

/* if keep_used != 0, then the db files which match an used syncdb
 * will be kept  */
static int sync_cleandb(const char *dbpath, int keep_used)
{
	DIR *dir;
	struct dirent *ent;
	alpm_list_t *syncdbs;
	int ret = 0;

	dir = opendir(dbpath);
	if(dir == NULL) {
		pm_printf(ALPM_LOG_ERROR, _("could not access database directory\n"));
		return 1;
	}

	syncdbs = alpm_get_syncdbs(config->handle);

	rewinddir(dir);
	/* step through the directory one file at a time */
	while((ent = readdir(dir)) != NULL) {
		char path[PATH_MAX];
		struct stat buf;
		int found = 0;
		const char *dname = ent->d_name;
		char *dbname;
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
		if(S_ISDIR(buf.st_mode)) {
			if(rmrf(path)) {
				pm_printf(ALPM_LOG_ERROR, _("could not remove %s: %s\n"),
						path, strerror(errno));
			}
			continue;
		}

		len = strlen(dname);
		if(len > 3 && strcmp(dname + len - 3, ".db") == 0) {
			dbname = strndup(dname, len - 3);
		} else if(len > 7 && strcmp(dname + len - 7, ".db.sig") == 0) {
			dbname = strndup(dname, len - 7);
		} else {
			ret += unlink_verbose(path, 0);
			continue;
		}

		if(keep_used) {
			alpm_list_t *i;
			for(i = syncdbs; i && !found; i = alpm_list_next(i)) {
				alpm_db_t *db = i->data;
				found = !strcmp(dbname, alpm_db_get_name(db));
			}
		}

		/* We have a database that doesn't match any syncdb. */
		if(!found) {
			/* ENOENT check is because the signature and database could come in any
			 * order in our readdir() call, so either file may already be gone. */
			snprintf(path, PATH_MAX, "%s%s.db", dbpath, dbname);
			ret += unlink_verbose(path, 1);
			/* unlink a signature file if present too */
			snprintf(path, PATH_MAX, "%s%s.db.sig", dbpath, dbname);
			ret += unlink_verbose(path, 1);
		}
		free(dbname);
	}
	closedir(dir);
	return ret;
}

static int sync_cleandb_all(void)
{
	const char *dbpath;
	char *newdbpath;
	int ret = 0;

	dbpath = alpm_option_get_dbpath(config->handle);
	printf(_("Database directory: %s\n"), dbpath);
	if(!yesno(_("Do you want to remove unused repositories?"))) {
		return 0;
	}
	printf(_("removing unused sync repositories...\n"));
	/* The sync dbs were previously put in dbpath/ but are now in dbpath/sync/.
	 * We will clean everything in dbpath/ except local/, sync/ and db.lck, and
	 * only the unused sync dbs in dbpath/sync/ */
	ret += sync_cleandb(dbpath, 0);

	if(asprintf(&newdbpath, "%s%s", dbpath, "sync/") < 0) {
		ret += 1;
		return ret;
	}
	ret += sync_cleandb(newdbpath, 1);
	free(newdbpath);

	return ret;
}

static int sync_cleancache(int level)
{
	alpm_list_t *i;
	alpm_list_t *sync_dbs = alpm_get_syncdbs(config->handle);
	alpm_db_t *db_local = alpm_get_localdb(config->handle);
	alpm_list_t *cachedirs = alpm_option_get_cachedirs(config->handle);
	int ret = 0;

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
	}
	printf("\n");

	for(i = cachedirs; i; i = alpm_list_next(i)) {
		const char *cachedir = i->data;
		DIR *dir;
		struct dirent *ent;

		printf(_("Cache directory: %s\n"), (const char *)i->data);

		if(level == 1) {
			if(!yesno(_("Do you want to remove all other packages from cache?"))) {
				printf("\n");
				continue;
			}
			printf(_("removing old packages from cache...\n"));
		} else {
			if(!noyes(_("Do you want to remove ALL files from cache?"))) {
				printf("\n");
				continue;
			}
			printf(_("removing all files from cache...\n"));
		}

		dir = opendir(cachedir);
		if(dir == NULL) {
			pm_printf(ALPM_LOG_ERROR,
					_("could not access cache directory %s\n"), cachedir);
			ret++;
			continue;
		}

		rewinddir(dir);
		/* step through the directory one file at a time */
		while((ent = readdir(dir)) != NULL) {
			char path[PATH_MAX];
			int delete = 1;
			alpm_pkg_t *localpkg = NULL, *pkg = NULL;
			const char *local_name, *local_version;

			if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
				continue;
			}

			if(level <= 1) {
				static const char * const glob_skips[] = {
					/* skip signature files - they are removed with their package file */
					"*.sig",
					/* skip package database within the cache directory */
					"*.db*",
					/* skip source packages within the cache directory */
					"*.src.tar.*",
					/* skip package deltas, we aren't smart enough to clean these yet */
					"*.delta",
					/* skip any partial downloads */
					"*.part"
				};
				size_t j;

				for(j = 0; j < sizeof(glob_skips) / sizeof(glob_skips[0]); j++) {
					if(fnmatch(glob_skips[j], ent->d_name, 0) == 0) {
						delete = 0;
						break;
					}
				}
				if(delete == 0) {
					continue;
				}
			}

			/* build the full filepath */
			snprintf(path, PATH_MAX, "%s%s", cachedir, ent->d_name);

			/* short circuit for removing all files from cache */
			if(level > 1) {
				ret += unlink_verbose(path, 0);
				continue;
			}

			/* attempt to load the file as a package. if we cannot load the file,
			 * simply skip it and move on. we don't need a full load of the package,
			 * just the metadata. */
			if(alpm_pkg_load(config->handle, path, 0, 0, &localpkg) != 0) {
				pm_printf(ALPM_LOG_DEBUG, "skipping %s, could not load as package\n",
						path);
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
					pm_printf(ALPM_LOG_DEBUG, "package %s-%s found in local db\n",
							local_name, local_version);
					delete = 0;
				}
			}
			if(config->cleanmethod & PM_CLEAN_KEEPCUR) {
				alpm_list_t *j;
				/* check if this package is in a sync DB */
				for(j = sync_dbs; j && delete; j = alpm_list_next(j)) {
					alpm_db_t *db = j->data;
					pkg = alpm_db_get_pkg(db, local_name);
					if(pkg != NULL && alpm_pkg_vercmp(local_version,
								alpm_pkg_get_version(pkg)) == 0) {
						/* package was found in a sync DB and version matches, keep it */
						pm_printf(ALPM_LOG_DEBUG, "package %s-%s found in sync db\n",
								local_name, local_version);
						delete = 0;
					}
				}
			}
			/* free the local file package */
			alpm_pkg_free(localpkg);

			if(delete) {
				size_t pathlen = strlen(path);
				ret += unlink_verbose(path, 0);
				/* unlink a signature file if present too */
				if(PATH_MAX - 5 >= pathlen) {
					strcpy(path + pathlen, ".sig");
					ret += unlink_verbose(path, 1);
				}
			}
		}
		closedir(dir);
		printf("\n");
	}

	return ret;
}

static int sync_synctree(int level, alpm_list_t *syncs)
{
	alpm_list_t *i;
	unsigned int success = 0;

	for(i = syncs; i; i = alpm_list_next(i)) {
		alpm_db_t *db = i->data;

		int ret = alpm_db_update((level < 2 ? 0 : 1), db);
		if(ret < 0) {
			pm_printf(ALPM_LOG_ERROR, _("failed to update %s (%s)\n"),
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
		pm_printf(ALPM_LOG_ERROR, _("failed to synchronize any databases\n"));
		trans_init_error();
	}
	return (success > 0);
}

static void print_installed(alpm_db_t *db_local, alpm_pkg_t *pkg)
{
	const char *pkgname = alpm_pkg_get_name(pkg);
	const char *pkgver = alpm_pkg_get_version(pkg);
	alpm_pkg_t *lpkg = alpm_db_get_pkg(db_local, pkgname);
	if(lpkg) {
		const char *lpkgver = alpm_pkg_get_version(lpkg);
		if(strcmp(lpkgver, pkgver) == 0) {
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
	alpm_db_t *db_local = alpm_get_localdb(config->handle);

	for(i = syncs; i; i = alpm_list_next(i)) {
		alpm_db_t *db = i->data;
		unsigned short cols;
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
		cols = getcols(fileno(stdout));
		for(j = ret; j; j = alpm_list_next(j)) {
			alpm_list_t *grp;
			alpm_pkg_t *pkg = j->data;

			if(!config->quiet) {
				printf("%s/%s %s", alpm_db_get_name(db), alpm_pkg_get_name(pkg),
							 alpm_pkg_get_version(pkg));
			} else {
				fputs(alpm_pkg_get_name(pkg), stdout);
			}

			if(!config->quiet) {
				if((grp = alpm_pkg_get_groups(pkg)) != NULL) {
					alpm_list_t *k;
					fputs(" (", stdout);
					for(k = grp; k; k = alpm_list_next(k)) {
						const char *group = k->data;
						fputs(group, stdout);
						if(alpm_list_next(k)) {
							/* only print a spacer if there are more groups */
							putchar(' ');
						}
					}
					putchar(')');
				}

				print_installed(db_local, pkg);

				/* we need a newline and initial indent first */
				fputs("\n    ", stdout);
				indentprint(alpm_pkg_get_desc(pkg), 4, cols);
			}
			fputc('\n', stdout);
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
	alpm_list_t *i, *j, *k, *s = NULL;

	if(targets) {
		for(i = targets; i; i = alpm_list_next(i)) {
			const char *grpname = i->data;
			for(j = syncs; j; j = alpm_list_next(j)) {
				alpm_db_t *db = j->data;
				alpm_group_t *grp = alpm_db_get_group(db, grpname);

				if(grp) {
					/* get names of packages in group */
					for(k = grp->packages; k; k = alpm_list_next(k)) {
						if(!config->quiet) {
							printf("%s %s\n", grpname,
									alpm_pkg_get_name(k->data));
						} else {
							printf("%s\n", alpm_pkg_get_name(k->data));
						}
					}
				}
			}
		}
	} else {
		for(i = syncs; i; i = alpm_list_next(i)) {
			alpm_db_t *db = i->data;

			for(j = alpm_db_get_groupcache(db); j; j = alpm_list_next(j)) {
				alpm_group_t *grp = j->data;

				if(level > 1) {
					for(k = grp->packages; k; k = alpm_list_next(k)) {
						printf("%s %s\n", grp->name,
								alpm_pkg_get_name(k->data));
					}
				} else {
					/* print grp names only, no package names */
					if(!alpm_list_find_str (s, grp->name)) {
						s = alpm_list_add (s, grp->name);
						printf("%s\n", grp->name);
					}
				}
			}
		}
		alpm_list_free(s);
	}

	return 0;
}

static int sync_info(alpm_list_t *syncs, alpm_list_t *targets)
{
	alpm_list_t *i, *j, *k;
	int ret = 0;

	if(targets) {
		for(i = targets; i; i = alpm_list_next(i)) {
			const char *target = i->data;
			char *name = strdup(target);
			char *repo, *pkgstr;
			int foundpkg = 0, founddb = 0;

			pkgstr = strchr(name, '/');
			if(pkgstr) {
				repo = name;
				*pkgstr = '\0';
				++pkgstr;
			} else {
				repo = NULL;
				pkgstr = name;
			}

			for(j = syncs; j; j = alpm_list_next(j)) {
				alpm_db_t *db = j->data;
				if(repo && strcmp(repo, alpm_db_get_name(db)) != 0) {
					continue;
				}
				founddb = 1;

				for(k = alpm_db_get_pkgcache(db); k; k = alpm_list_next(k)) {
					alpm_pkg_t *pkg = k->data;

					if(strcmp(alpm_pkg_get_name(pkg), pkgstr) == 0) {
						dump_pkg_full(pkg, config->op_s_info > 1);
						foundpkg = 1;
						break;
					}
				}
			}

			if(!founddb) {
				pm_printf(ALPM_LOG_ERROR,
						_("repository '%s' does not exist\n"), repo);
				ret++;
			}
			if(!foundpkg) {
				pm_printf(ALPM_LOG_ERROR,
						_("package '%s' was not found\n"), target);
				ret++;
			}
			free(name);
		}
	} else {
		for(i = syncs; i; i = alpm_list_next(i)) {
			alpm_db_t *db = i->data;

			for(j = alpm_db_get_pkgcache(db); j; j = alpm_list_next(j)) {
				alpm_pkg_t *pkg = j->data;
				dump_pkg_full(pkg, config->op_s_info > 1);
			}
		}
	}

	return ret;
}

static int sync_list(alpm_list_t *syncs, alpm_list_t *targets)
{
	alpm_list_t *i, *j, *ls = NULL;
	alpm_db_t *db_local = alpm_get_localdb(config->handle);

	if(targets) {
		for(i = targets; i; i = alpm_list_next(i)) {
			const char *repo = i->data;
			alpm_db_t *db = NULL;

			for(j = syncs; j; j = alpm_list_next(j)) {
				alpm_db_t *d = j->data;

				if(strcmp(repo, alpm_db_get_name(d)) == 0) {
					db = d;
					break;
				}
			}

			if(db == NULL) {
				pm_printf(ALPM_LOG_ERROR,
					_("repository \"%s\" was not found.\n"), repo);
				alpm_list_free(ls);
				return 1;
			}

			ls = alpm_list_add(ls, db);
		}
	} else {
		ls = syncs;
	}

	for(i = ls; i; i = alpm_list_next(i)) {
		alpm_db_t *db = i->data;

		for(j = alpm_db_get_pkgcache(db); j; j = alpm_list_next(j)) {
			alpm_pkg_t *pkg = j->data;

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

static alpm_db_t *get_db(const char *dbname)
{
	alpm_list_t *i;
	for(i = alpm_get_syncdbs(config->handle); i; i = i->next) {
		alpm_db_t *db = i->data;
		if(strcmp(alpm_db_get_name(db), dbname) == 0) {
			return db;
		}
	}
	return NULL;
}

static int process_pkg(alpm_pkg_t *pkg)
{
	int ret = alpm_add_pkg(config->handle, pkg);

	if(ret == -1) {
		alpm_errno_t err = alpm_errno(config->handle);
		if(err == ALPM_ERR_TRANS_DUP_TARGET
				|| err == ALPM_ERR_PKG_IGNORED) {
			/* just skip duplicate or ignored targets */
			pm_printf(ALPM_LOG_WARNING, _("skipping target: %s\n"), alpm_pkg_get_name(pkg));
			return 0;
		} else {
			pm_printf(ALPM_LOG_ERROR, "'%s': %s\n", alpm_pkg_get_name(pkg),
					alpm_strerror(err));
			return 1;
		}
	}
	config->explicit_adds = alpm_list_add(config->explicit_adds, pkg);
	return 0;
}

static int process_group(alpm_list_t *dbs, const char *group, int error)
{
	int ret = 0;
	alpm_list_t *i;
	alpm_list_t *pkgs = alpm_find_group_pkgs(dbs, group);
	int count = alpm_list_count(pkgs);

	if(!count) {
		pm_printf(ALPM_LOG_ERROR, _("target not found: %s\n"), group);
		return 1;
	}

	if(error) {
		/* we already know another target errored. there is no reason to prompt the
		 * user here; we already validated the group name so just move on since we
		 * won't actually be installing anything anyway. */
		goto cleanup;
	}

	if(config->print == 0) {
		printf(_(":: There are %d members in group %s:\n"), count,
				group);
		select_display(pkgs);
		char *array = malloc(count);
		if(!array) {
			ret = 1;
			goto cleanup;
		}
		if(multiselect_question(array, count)) {
			ret = 1;
			free(array);
			goto cleanup;
		}
		int n = 0;
		for(i = pkgs; i; i = alpm_list_next(i)) {
			if(array[n++] == 0)
				continue;
			alpm_pkg_t *pkg = i->data;

			if(process_pkg(pkg) == 1) {
				ret = 1;
				free(array);
				goto cleanup;
			}
		}
		free(array);
	} else {
		for(i = pkgs; i; i = alpm_list_next(i)) {
			alpm_pkg_t *pkg = i->data;

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

static int process_targname(alpm_list_t *dblist, const char *targname,
		int error)
{
	alpm_pkg_t *pkg = alpm_find_dbs_satisfier(config->handle, dblist, targname);

	/* #FS#23342 - skip ignored packages when user says no */
	if(alpm_errno(config->handle) == ALPM_ERR_PKG_IGNORED) {
			pm_printf(ALPM_LOG_WARNING, _("skipping target: %s\n"), targname);
			return 0;
	}

	if(pkg) {
		return process_pkg(pkg);
	}
	/* fallback on group */
	return process_group(dblist, targname, error);
}

static int process_target(const char *target, int error)
{
	/* process targets */
	char *targstring = strdup(target);
	char *targname = strchr(targstring, '/');
	int ret = 0;
	alpm_list_t *dblist;

	if(targname && targname != targstring) {
		alpm_db_t *db;
		const char *dbname;

		*targname = '\0';
		targname++;
		dbname = targstring;
		db = get_db(dbname);
		if(!db) {
			pm_printf(ALPM_LOG_ERROR, _("database not found: %s\n"),
					dbname);
			ret = 1;
			goto cleanup;
		}
		dblist = alpm_list_add(NULL, db);
		ret = process_targname(dblist, targname, error);
		alpm_list_free(dblist);
	} else {
		targname = targstring;
		dblist = alpm_get_syncdbs(config->handle);
		ret = process_targname(dblist, targname, error);
	}

cleanup:
	free(targstring);
	if(ret && access(target, R_OK) == 0) {
		pm_printf(ALPM_LOG_WARNING,
				_("'%s' is a file, did you mean %s instead of %s?\n"),
				target, "-U/--upgrade", "-S/--sync");
	}
	return ret;
}

static int sync_trans(alpm_list_t *targets)
{
	int retval = 0;
	alpm_list_t *i;

	/* Step 1: create a new transaction... */
	if(trans_init(config->flags, 1) == -1) {
		return 1;
	}

	/* process targets */
	for(i = targets; i; i = alpm_list_next(i)) {
		const char *targ = i->data;
		if(process_target(targ, retval) == 1) {
			retval = 1;
		}
	}

	if(retval) {
		trans_release();
		return retval;
	}

	if(config->op_s_upgrade) {
		printf(_(":: Starting full system upgrade...\n"));
		alpm_logaction(config->handle, PACMAN_CALLER_PREFIX,
				"starting full system upgrade\n");
		if(alpm_sync_sysupgrade(config->handle, config->op_s_upgrade >= 2) == -1) {
			pm_printf(ALPM_LOG_ERROR, "%s\n", alpm_strerror(alpm_errno(config->handle)));
			trans_release();
			return 1;
		}
	}

	return sync_prepare_execute();
}

int sync_prepare_execute(void)
{
	alpm_list_t *i, *packages, *data = NULL;
	int retval = 0;

	/* Step 2: "compute" the transaction based on targets and flags */
	if(alpm_trans_prepare(config->handle, &data) == -1) {
		alpm_errno_t err = alpm_errno(config->handle);
		pm_printf(ALPM_LOG_ERROR, _("failed to prepare transaction (%s)\n"),
		        alpm_strerror(err));
		switch(err) {
			case ALPM_ERR_PKG_INVALID_ARCH:
				for(i = data; i; i = alpm_list_next(i)) {
					const char *pkg = i->data;
					printf(_(":: package %s does not have a valid architecture\n"), pkg);
				}
				break;
			case ALPM_ERR_UNSATISFIED_DEPS:
				for(i = data; i; i = alpm_list_next(i)) {
					alpm_depmissing_t *miss = i->data;
					char *depstring = alpm_dep_compute_string(miss->depend);
					printf(_(":: %s: requires %s\n"), miss->target, depstring);
					free(depstring);
				}
				break;
			case ALPM_ERR_CONFLICTING_DEPS:
				for(i = data; i; i = alpm_list_next(i)) {
					alpm_conflict_t *conflict = i->data;
					/* only print reason if it contains new information */
					if(conflict->reason->mod == ALPM_DEP_MOD_ANY) {
						printf(_(":: %s and %s are in conflict\n"),
								conflict->package1, conflict->package2);
					} else {
						char *reason = alpm_dep_compute_string(conflict->reason);
						printf(_(":: %s and %s are in conflict (%s)\n"),
								conflict->package1, conflict->package2, reason);
						free(reason);
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
		if(!config->print) {
			printf(_(" there is nothing to do\n"));
		}
		goto cleanup;
	}

	/* Step 3: actually perform the operation */
	if(config->print) {
		print_packages(packages);
		goto cleanup;
	}

	display_targets();
	printf("\n");

	int confirm;
	if(config->op_s_downloadonly) {
		confirm = yesno(_("Proceed with download?"));
	} else {
		confirm = yesno(_("Proceed with installation?"));
	}
	if(!confirm) {
		retval = 1;
		goto cleanup;
	}

	if(alpm_trans_commit(config->handle, &data) == -1) {
		alpm_errno_t err = alpm_errno(config->handle);
		pm_printf(ALPM_LOG_ERROR, _("failed to commit transaction (%s)\n"),
		        alpm_strerror(err));
		switch(err) {
			case ALPM_ERR_FILE_CONFLICTS:
				for(i = data; i; i = alpm_list_next(i)) {
					alpm_fileconflict_t *conflict = i->data;
					switch(conflict->type) {
						case ALPM_FILECONFLICT_TARGET:
							printf(_("%s exists in both '%s' and '%s'\n"),
									conflict->file, conflict->target, conflict->ctarget);
							break;
						case ALPM_FILECONFLICT_FILESYSTEM:
							printf(_("%s: %s exists in filesystem\n"),
									conflict->target, conflict->file);
							break;
					}
				}
				break;
			case ALPM_ERR_PKG_INVALID:
			case ALPM_ERR_PKG_INVALID_CHECKSUM:
			case ALPM_ERR_PKG_INVALID_SIG:
			case ALPM_ERR_DLT_INVALID:
				for(i = data; i; i = alpm_list_next(i)) {
					const char *filename = i->data;
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

		if(trans_init(0, 0) == -1) {
			return 1;
		}

		ret += sync_cleancache(config->op_s_clean);
		ret += sync_cleandb_all();

		if(trans_release() == -1) {
			ret++;
		}

		return ret;
	}

	if(check_syncdbs(1, 0)) {
		return 1;
	}

	sync_dbs = alpm_get_syncdbs(config->handle);

	if(config->op_s_sync) {
		/* grab a fresh package list */
		printf(_(":: Synchronizing package databases...\n"));
		alpm_logaction(config->handle, PACMAN_CALLER_PREFIX,
				"synchronizing package lists\n");
		if(!sync_synctree(config->op_s_sync, sync_dbs)) {
			return 1;
		}
	}

	if(check_syncdbs(1, 1)) {
		return 1;
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
			pm_printf(ALPM_LOG_ERROR, _("no targets specified (use -h for help)\n"));
			return 1;
		}
	}

	return sync_trans(targets);
}

/* vim: set ts=2 sw=2 noet: */
