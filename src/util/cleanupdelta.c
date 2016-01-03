/*
 *  cleanupdelta.c : return list of unused delta in a given sync database
 *
 *  Copyright (c) 2011-2016 Pacman Development Team <pacman-dev@archlinux.org>
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

#include <alpm.h>
#include <alpm_list.h>

alpm_handle_t *handle = NULL;

static void cleanup(int signum)
{
	if(handle && alpm_release(handle) == -1) {
		fprintf(stderr, "error releasing alpm\n");
	}

	exit(signum);
}

__attribute__((format(printf, 2, 0)))
static void output_cb(alpm_loglevel_t level, const char *fmt, va_list args)
{
	if(strlen(fmt)) {
		switch(level) {
			case ALPM_LOG_ERROR: printf("error: "); break;
			case ALPM_LOG_WARNING: printf("warning: "); break;
			/* case ALPM_LOG_DEBUG: printf("debug: "); break; */
			default: return;
		}
		vprintf(fmt, args);
	}
}


static void checkpkgs(alpm_list_t *pkglist)
{
	alpm_list_t *i, *j;
	for(i = pkglist; i; i = alpm_list_next(i)) {
		alpm_pkg_t *pkg = i->data;
		alpm_list_t *unused = alpm_pkg_unused_deltas(pkg);
		for(j = unused; j; j = alpm_list_next(j)) {
			const char *delta = j->data;
			printf("%s\n", delta);
		}
		alpm_list_free(unused);
	}
}

static void checkdbs(alpm_list_t *dbnames)
{
	alpm_db_t *db = NULL;
	alpm_list_t *i;
	const alpm_siglevel_t level = ALPM_SIG_DATABASE | ALPM_SIG_DATABASE_OPTIONAL;

	for(i = dbnames; i; i = alpm_list_next(i)) {
		const char *dbname = i->data;
		db = alpm_register_syncdb(handle, dbname, level);
		if(db == NULL) {
			fprintf(stderr, "error: could not register sync database '%s' (%s)\n",
					dbname, alpm_strerror(alpm_errno(handle)));
			continue;
		}
		checkpkgs(alpm_db_get_pkgcache(db));
	}

}

static void usage(void)
{
	fprintf(stderr, "cleanupdelta (pacman) v" PACKAGE_VERSION "\n\n"
			"Returns a list of unused delta in a given sync database.\n\n"
			"Usage: cleanupdelta [options]\n\n"
			"  -b <pacman db>       core extra ... : check the listed sync databases\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	const char *dbpath = DBPATH;
	alpm_errno_t err;
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

	handle = alpm_initialize(ROOTDIR, dbpath, &err);
	if(!handle) {
		fprintf(stderr, "cannot initialize alpm: %s\n", alpm_strerror(err));
		return 1;
	}

	/* let us get log messages from libalpm */
	alpm_option_set_logcb(handle, output_cb);

	checkdbs(dbnames);
	alpm_list_free(dbnames);

	cleanup(0);
}

/* vim: set noet: */
