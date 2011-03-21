/*
 *  cleanupdelta.c : return list of unused delta in a given sync database
 *
 *  Copyright (c) 2011 Pacman Development Team <pacman-dev@archlinux.org>
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
#include <limits.h> /* PATH_MAX */

#include <alpm.h>
#include <alpm_list.h>

#define BASENAME "cleanupdelta"

static void cleanup(int signum) {
	if(alpm_release() == -1) {
		fprintf(stderr, "error releasing alpm: %s\n", alpm_strerrorlast());
	}

	exit(signum);
}

static void output_cb(pmloglevel_t level, const char *fmt, va_list args)
{
	if(strlen(fmt)) {
		switch(level) {
			case PM_LOG_ERROR: printf("error: "); break;
			case PM_LOG_WARNING: printf("warning: "); break;
			//case PM_LOG_DEBUG: printf("debug: "); break;
			default: return;
		}
		vprintf(fmt, args);
	}
}


static void checkpkgs(alpm_list_t *pkglist)
{
	alpm_list_t *i, *j;
	for(i = pkglist; i; i = alpm_list_next(i)) {
		pmpkg_t *pkg = alpm_list_getdata(i);
		alpm_list_t *unused = alpm_pkg_unused_deltas(pkg);
		for(j = unused; j; j = alpm_list_next(j)) {
			char *delta = alpm_list_getdata(j);
			printf("%s\n", delta);
		}
		alpm_list_free(unused);
	}
}

static void checkdbs(char *dbpath, alpm_list_t *dbnames) {
	char syncdbpath[PATH_MAX];
	pmdb_t *db = NULL;
	alpm_list_t *i;

	for(i = dbnames; i; i = alpm_list_next(i)) {
		char *dbname = alpm_list_getdata(i);
		snprintf(syncdbpath, PATH_MAX, "%s/sync/%s", dbpath, dbname);
		db = alpm_db_register_sync(dbname);
		if(db == NULL) {
			fprintf(stderr, "error: could not register sync database (%s)\n",
					alpm_strerrorlast());
			return;
		}
		checkpkgs(alpm_db_get_pkgcache(db));
	}

}

static void usage(void) {
	fprintf(stderr, "usage:\n");
	fprintf(stderr,
			"\t%s [-b <pacman db>] core extra ... : check the listed sync databases\n", BASENAME);
	exit(1);
}

int main(int argc, char *argv[])
{
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

	if(!dbnames) {
		usage();
	}

	if(alpm_initialize() == -1) {
		fprintf(stderr, "cannot initialize alpm: %s\n", alpm_strerrorlast());
		return 1;
	}

	/* let us get log messages from libalpm */
	alpm_option_set_logcb(output_cb);

	alpm_option_set_dbpath(dbpath);

	checkdbs(dbpath,dbnames);
	alpm_list_free(dbnames);

	cleanup(0);
}

/* vim: set ts=2 sw=2 noet: */
