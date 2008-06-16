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
#include <libgen.h>

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

static int db_test(char *dbpath)
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
		if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
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
		snprintf(path, PATH_MAX, "%s/%s/files", dbpath, ent->d_name);
		if(access(path, F_OK)) {
			printf("%s: file list is missing\n", ent->d_name);
			ret++;
		}
	}
	return(ret);
}

int main(int argc, char **argv)
{
	int retval = 0; /* default = false */
	pmdb_t *db = NULL;
	char *dbpath;
	char localdbpath[PATH_MAX];
	alpm_list_t *i;

	if(argc == 1) {
		dbpath = DBPATH;
	} else if(argc == 3 && strcmp(argv[1], "-b") == 0) {
		dbpath = argv[2];
	} else {
		fprintf(stderr, "usage: %s -b <pacman db>\n", BASENAME);
		return(1);
	}

	snprintf(localdbpath, PATH_MAX, "%s/local", dbpath);
	retval = db_test(localdbpath);
	if(retval) {
		return(retval);
	}

	if(alpm_initialize() == -1) {
		fprintf(stderr, "cannot initialize alpm: %s\n", alpm_strerrorlast());
		return(1);
	}

	/* let us get log messages from libalpm */
	alpm_option_set_logcb(output_cb);

	alpm_option_set_dbpath(dbpath);

	db = alpm_db_register_local();
	if(db == NULL) {
		fprintf(stderr, "error: could not register 'local' database (%s)\n",
				alpm_strerrorlast());
		cleanup(EXIT_FAILURE);
	}

	/* check dependencies */
	alpm_list_t *data;
	data = alpm_checkdeps(db, 0, NULL, alpm_db_getpkgcache(db));
	for(i = data; i; i = alpm_list_next(i)) {
		pmdepmissing_t *miss = alpm_list_getdata(i);
		pmdepend_t *dep = alpm_miss_get_dep(miss);
		char *depstring = alpm_dep_get_string(dep);
		printf("missing dependency for %s : %s\n", alpm_miss_get_target(miss),
				depstring);
		free(depstring);
	}

	/* check conflicts */
	data = alpm_checkdbconflicts(db);
	for(i = data; i; i = i->next) {
		pmconflict_t *conflict = alpm_list_getdata(i);
		printf("%s conflicts with %s\n", alpm_conflict_get_package1(conflict),
				alpm_conflict_get_package2(conflict));
	}

	cleanup(retval);
}

/* vim: set ts=2 sw=2 noet: */
