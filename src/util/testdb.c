/*
 *  testdb.c : Test a pacman local database for validity
 *
 *  Copyright (c) 2007 by Aaron Griffin <aaronmgriffin@gmail.com>
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <dirent.h>

#include <alpm.h>
#include <alpm_list.h>

#define BASENAME "testdb"

int str_cmp(const void *s1, const void *s2)
{
	return(strcmp(s1, s2));
}

static void cleanup(int signum) {
	if(alpm_release() == -1) {
		fprintf(stderr, "error releasing alpm: %s\n", alpm_strerrorlast());
	}

	exit(signum);
}

void output_cb(pmloglevel_t level, char *fmt, va_list args)
{
	if(strlen(fmt)) {
		switch(level) {
			case PM_LOG_ERROR: printf("error: "); break;
			case PM_LOG_WARNING: printf("warning: "); break;
			default: return;
		}
		vprintf(fmt, args);
	}
}

static int db_test(char *dbpath, int local)
{
	struct dirent *ent;
	char path[PATH_MAX];
	int ret = 0;

	DIR *dir;

	if(!(dir = opendir(dbpath))) {
		fprintf(stderr, "error : %s : %s\n", dbpath, strerror(errno));
		return(1);
	}

	while ((ent = readdir(dir)) != NULL) {
		if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0
				|| ent->d_name[0] == '.') {
			continue;
		}
		/* check for desc, depends, and files */
		snprintf(path, PATH_MAX, "%s/%s/desc", dbpath, ent->d_name);
		if(access(path, F_OK)) {
			printf("%s: description file is missing\n", ent->d_name);
			ret++;
		}
		snprintf(path, PATH_MAX, "%s/%s/depends", dbpath, ent->d_name);
		if(access(path, F_OK)) {
			printf("%s: dependency file is missing\n", ent->d_name);
			ret++;
		}
		if(local) {
			snprintf(path, PATH_MAX, "%s/%s/files", dbpath, ent->d_name);
			if(access(path, F_OK)) {
				printf("%s: file list is missing\n", ent->d_name);
				ret++;
			}
		}
	}
	if(closedir(dir)) {
		fprintf(stderr, "error closing dbpath : %s\n", strerror(errno));
		return(1);
	}

	return(ret);
}

int checkdeps(alpm_list_t *pkglist)
{
	alpm_list_t *data, *i;
	int ret = 0;
	/* check dependencies */
	data = alpm_checkdeps(pkglist, 0, NULL, pkglist);
	for(i = data; i; i = alpm_list_next(i)) {
		pmdepmissing_t *miss = alpm_list_getdata(i);
		pmdepend_t *dep = alpm_miss_get_dep(miss);
		char *depstring = alpm_dep_compute_string(dep);
		printf("missing dependency for %s : %s\n", alpm_miss_get_target(miss),
				depstring);
		free(depstring);
		ret++;
	}
	FREELIST(data);
	return(ret);
}

int checkconflicts(alpm_list_t *pkglist)
{
	alpm_list_t *data, *i;
	int ret = 0;
	/* check conflicts */
	data = alpm_checkconflicts(pkglist);
	for(i = data; i; i = i->next) {
		pmconflict_t *conflict = alpm_list_getdata(i);
		printf("%s conflicts with %s\n", alpm_conflict_get_package1(conflict),
				alpm_conflict_get_package2(conflict));
		ret++;
	}
	FREELIST(data);
	return(ret);
}

int check_localdb(char *dbpath) {
	char localdbpath[PATH_MAX];
	int ret = 0;
	pmdb_t *db = NULL;
	alpm_list_t *pkglist;

	snprintf(localdbpath, PATH_MAX, "%s/local", dbpath);
	ret = db_test(localdbpath, 1);
	if(ret) {
		return(ret);
	}

	db = alpm_db_register_local();
	if(db == NULL) {
		fprintf(stderr, "error: could not register 'local' database (%s)\n",
				alpm_strerrorlast());
		cleanup(EXIT_FAILURE);
	}
	pkglist = alpm_db_get_pkgcache(db);
	ret += checkdeps(pkglist);
	ret += checkconflicts(pkglist);
	return(ret);
}

int check_syncdbs(char *dbpath, alpm_list_t *dbnames) {
	char syncdbpath[PATH_MAX];
	int ret = 0;
	pmdb_t *db = NULL;
	alpm_list_t *i, *pkglist, *syncpkglist = NULL;

	for(i = dbnames; i; i = alpm_list_next(i)) {
		char *dbname = alpm_list_getdata(i);
		snprintf(syncdbpath, PATH_MAX, "%s/sync/%s", dbpath, dbname);
		ret = db_test(syncdbpath, 0);
		if(ret) {
			ret = 1;
			goto cleanup;
		}
		db = alpm_db_register_sync(dbname);
		if(db == NULL) {
			fprintf(stderr, "error: could not register sync database (%s)\n",
					alpm_strerrorlast());
			ret = 1;
			goto cleanup;
		}
		pkglist = alpm_db_get_pkgcache(db);
		syncpkglist = alpm_list_join(syncpkglist, alpm_list_copy(pkglist));
	}
	ret += checkdeps(syncpkglist);

cleanup:
	alpm_list_free(syncpkglist);
	return(ret);
}

void usage() {
	fprintf(stderr, "usage:\n");
	fprintf(stderr,
			"\t%s [-b <pacman db>]                : check the local database\n", BASENAME);
	fprintf(stderr,
			"\t%s [-b <pacman db>] core extra ... : check the listed sync databases\n", BASENAME);
	exit(1);
}

int main(int argc, char **argv)
{
	int ret = 0;
	char *dbpath = DBPATH;
	int a = 1;
	alpm_list_t *dbnames = NULL;

	while(a < argc) {
		if(strcmp(argv[a], "-b") == 0) {
			if(++a < argc) {
				dbpath = argv[a];
			} else {
				usage();
			}
		}	else if(strcmp(argv[a], "-h") == 0 ||
				strcmp(argv[a], "--help") == 0 ) {
			usage();
		} else {
			dbnames = alpm_list_add(dbnames, argv[a]);
		}
		a++;
	}

	if(alpm_initialize() == -1) {
		fprintf(stderr, "cannot initialize alpm: %s\n", alpm_strerrorlast());
		return(1);
	}

	/* let us get log messages from libalpm */
	alpm_option_set_logcb(output_cb);

	alpm_option_set_dbpath(dbpath);

	if(!dbnames) {
		ret = check_localdb(dbpath);
	} else {
		ret = check_syncdbs(dbpath,dbnames);
		alpm_list_free(dbnames);
	}

	cleanup(ret);
}

/* vim: set ts=2 sw=2 noet: */
