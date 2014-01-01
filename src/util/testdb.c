/*
 *  testdb.c : Test a pacman local database for validity
 *
 *  Copyright (c) 2007-2014 Pacman Development Team <pacman-dev@archlinux.org>
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
#include <string.h>
#include <dirent.h>

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
			default: return;
		}
		vprintf(fmt, args);
	}
}

static int check_localdb_files(void)
{
	struct dirent *ent;
	const char *dbpath;
	char path[4096];
	int ret = 0;
	DIR *dir;

	dbpath = alpm_option_get_dbpath(handle);
	snprintf(path, sizeof(path), "%slocal", dbpath);
	if(!(dir = opendir(path))) {
		fprintf(stderr, "error : '%s' : %s\n", path, strerror(errno));
		return 1;
	}

	while((ent = readdir(dir)) != NULL) {
		if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0
				|| ent->d_name[0] == '.') {
			continue;
		}
		/* check for known db files in local database */
		snprintf(path, sizeof(path), "%slocal/%s/desc", dbpath, ent->d_name);
		if(access(path, F_OK)) {
			printf("'%s': description file is missing\n", ent->d_name);
			ret++;
		}
		snprintf(path, sizeof(path), "%slocal/%s/files", dbpath, ent->d_name);
		if(access(path, F_OK)) {
			printf("'%s': file list is missing\n", ent->d_name);
			ret++;
		}
	}
	if(closedir(dir)) {
		fprintf(stderr, "error closing dbpath : %s\n", strerror(errno));
		return 1;
	}

	return ret;
}

static int check_deps(alpm_list_t *pkglist)
{
	alpm_list_t *data, *i;
	int ret = 0;
	/* check dependencies */
	data = alpm_checkdeps(handle, NULL, NULL, pkglist, 0);
	for(i = data; i; i = alpm_list_next(i)) {
		alpm_depmissing_t *miss = i->data;
		char *depstring = alpm_dep_compute_string(miss->depend);
		printf("missing '%s' dependency for '%s'\n", depstring, miss->target);
		free(depstring);
		ret++;
	}
	FREELIST(data);
	return ret;
}

static int check_conflicts(alpm_list_t *pkglist)
{
	alpm_list_t *data, *i;
	int ret = 0;
	/* check conflicts */
	data = alpm_checkconflicts(handle, pkglist);
	for(i = data; i; i = i->next) {
		alpm_conflict_t *conflict = i->data;
		printf("'%s' conflicts with '%s'\n",
				conflict->package1, conflict->package2);
		ret++;
	}
	FREELIST(data);
	return ret;
}

struct fileitem {
	alpm_file_t *file;
	alpm_pkg_t *pkg;
};

static int fileitem_cmp(const void *p1, const void *p2)
{
	const struct fileitem * fi1 = p1;
	const struct fileitem * fi2 = p2;
	return strcmp(fi1->file->name, fi2->file->name);
}

static int check_filelists(alpm_list_t *pkglist)
{
	alpm_list_t *i;
	int ret = 0;
	size_t list_size = 4096;
	size_t offset = 0, j;
	struct fileitem *all_files;
	struct fileitem *prev_fileitem = NULL;

	all_files = malloc(list_size * sizeof(struct fileitem));

	for(i = pkglist; i; i = i->next) {
		alpm_pkg_t *pkg = i->data;
		alpm_filelist_t *filelist = alpm_pkg_get_files(pkg);
		for(j = 0; j < filelist->count; j++) {
			alpm_file_t *file = filelist->files + j;
			/* only add files, not directories, to our big list */
			if(file->name[strlen(file->name) - 1] == '/') {
				continue;
			}

			/* do we need to reallocate and grow our array? */
			if(offset >= list_size) {
				struct fileitem *new_files;
				new_files = realloc(all_files, list_size * 2 * sizeof(struct fileitem));
				if(!new_files) {
					free(all_files);
					return 1;
				}
				all_files = new_files;
				list_size *= 2;
			}

			/* we can finally add it to the list */
			all_files[offset].file = file;
			all_files[offset].pkg = pkg;
			offset++;
		}
	}

	/* now sort the list so we can find duplicates */
	qsort(all_files, offset, sizeof(struct fileitem), fileitem_cmp);

	/* do a 'uniq' style check on the list */
	for(j = 0; j < offset; j++) {
		struct fileitem *fileitem = all_files + j;
		if(prev_fileitem && fileitem_cmp(prev_fileitem, fileitem) == 0) {
			printf("file owned by '%s' and '%s': '%s'\n",
					alpm_pkg_get_name(prev_fileitem->pkg),
					alpm_pkg_get_name(fileitem->pkg),
					fileitem->file->name);
		}
		prev_fileitem = fileitem;
	}

	free(all_files);
	return ret;
}

static int check_localdb(void)
{
	int ret = 0;
	alpm_db_t *db = NULL;
	alpm_list_t *pkglist;

	ret = check_localdb_files();
	if(ret) {
		return ret;
	}

	db = alpm_get_localdb(handle);
	pkglist = alpm_db_get_pkgcache(db);
	ret += check_deps(pkglist);
	ret += check_conflicts(pkglist);
	ret += check_filelists(pkglist);
	return ret;
}

static int check_syncdbs(alpm_list_t *dbnames)
{
	int ret = 0;
	alpm_db_t *db = NULL;
	alpm_list_t *i, *pkglist, *syncpkglist = NULL;
	const alpm_siglevel_t level = ALPM_SIG_DATABASE | ALPM_SIG_DATABASE_OPTIONAL;

	for(i = dbnames; i; i = alpm_list_next(i)) {
		const char *dbname = i->data;
		db = alpm_register_syncdb(handle, dbname, level);
		if(db == NULL) {
			fprintf(stderr, "error: could not register sync database (%s)\n",
					alpm_strerror(alpm_errno(handle)));
			ret = 1;
			goto cleanup;
		}
		pkglist = alpm_db_get_pkgcache(db);
		syncpkglist = alpm_list_join(syncpkglist, alpm_list_copy(pkglist));
	}
	ret += check_deps(syncpkglist);

cleanup:
	alpm_list_free(syncpkglist);
	return ret;
}

static void usage(void)
{
	fprintf(stderr, "testdb (pacman) v" PACKAGE_VERSION "\n\n"
			"Test a pacman local database for validity.\n\n"
			"Usage: testdb [options]\n\n"
			"  -b <pacman db>                : check the local database\n"
			"  -b <pacman db> core extra ... : check the listed sync databases\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	int errors = 0;
	alpm_errno_t err;
	const char *dbpath = DBPATH;
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

	handle = alpm_initialize(ROOTDIR, dbpath, &err);
	if(!handle) {
		fprintf(stderr, "cannot initialize alpm: %s\n", alpm_strerror(err));
		return EXIT_FAILURE;
	}

	/* let us get log messages from libalpm */
	alpm_option_set_logcb(handle, output_cb);

	if(!dbnames) {
		errors = check_localdb();
	} else {
		errors = check_syncdbs(dbnames);
		alpm_list_free(dbnames);
	}

	cleanup(errors > 0);
}

/* vim: set ts=2 sw=2 noet: */
