/*
 *  query.c
 *
 *  Copyright (c) 2006-2012 Pacman Development Team <pacman-dev@archlinux.org>
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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "pacman.h"
#include "package.h"
#include "check.h"
#include "conf.h"
#include "util.h"

#define LOCAL_PREFIX "local/"

/* check if filename exists in PATH */
static int search_path(char **filename, struct stat *bufptr)
{
	char *envpath, *envpathsplit, *path, *fullname;
	size_t flen;

	if((envpath = getenv("PATH")) == NULL) {
		return -1;
	}
	if((envpath = envpathsplit = strdup(envpath)) == NULL) {
		return -1;
	}

	flen = strlen(*filename);

	while((path = strsep(&envpathsplit, ":")) != NULL) {
		size_t plen = strlen(path);

		/* strip the trailing slash if one exists */
		while(path[plen - 1] == '/') {
			path[--plen] = '\0';
		}

		fullname = malloc(plen + flen + 2);
		if(!fullname) {
			free(envpath);
			return -1;
		}
		sprintf(fullname, "%s/%s", path, *filename);

		if(lstat(fullname, bufptr) == 0) {
			free(*filename);
			*filename = fullname;
			free(envpath);
			return 0;
		}
		free(fullname);
	}
	free(envpath);
	return -1;
}

static void print_query_fileowner(const char *filename, alpm_pkg_t *info)
{
	if(!config->quiet) {
		printf(_("%s is owned by %s %s\n"), filename,
				alpm_pkg_get_name(info), alpm_pkg_get_version(info));
	} else {
		printf("%s\n", alpm_pkg_get_name(info));
	}
}

static int query_fileowner(alpm_list_t *targets)
{
	int ret = 0;
	char path[PATH_MAX];
	size_t rootlen;
	alpm_list_t *t;
	alpm_db_t *db_local;

	/* This code is here for safety only */
	if(targets == NULL) {
		pm_printf(ALPM_LOG_ERROR, _("no file was specified for --owns\n"));
		return 1;
	}

	/* Set up our root path buffer. We only need to copy the location of root in
	 * once, then we can just overwrite whatever file was there on the previous
	 * iteration. */

	/* resolve root now so any symlinks in it will only have to be resolved once */
	if(!realpath(alpm_option_get_root(config->handle), path)) {
		pm_printf(ALPM_LOG_ERROR, _("cannot determine real path for '%s': %s\n"),
				path, strerror(errno));
		return 1;
	}

	/* make sure there's enough room to append the package file to path */
	rootlen = strlen(path);
	if(rootlen + 2 > PATH_MAX) {
		pm_printf(ALPM_LOG_ERROR, _("path too long: %s%s\n"), path, "");
		return 1;
	}

	/* append trailing '/' removed by realpath */
	path[rootlen++] = '/';
	path[rootlen] = '\0';

	db_local = alpm_get_localdb(config->handle);

	for(t = targets; t; t = alpm_list_next(t)) {
		char *filename = NULL, *dname = NULL, *rpath = NULL;
		const char *bname;
		struct stat buf;
		alpm_list_t *i;
		size_t len;
		unsigned int found = 0;

		if((filename = strdup(t->data)) == NULL) {
			goto targcleanup;
		}

		/* trailing '/' causes lstat to dereference directory symlinks */
		len = strlen(filename) - 1;
		while(len > 0 && filename[len] == '/') {
			filename[len--] = '\0';
		}

		if(lstat(filename, &buf) == -1) {
			/*  if it is not a path but a program name, then check in PATH */
			if(strchr(filename, '/') == NULL) {
				if(search_path(&filename, &buf) == -1) {
					pm_printf(ALPM_LOG_ERROR, _("failed to find '%s' in PATH: %s\n"),
							filename, strerror(errno));
					goto targcleanup;
				}
			} else {
				pm_printf(ALPM_LOG_ERROR, _("failed to read file '%s': %s\n"),
						filename, strerror(errno));
				goto targcleanup;
			}
		}

		if(S_ISDIR(buf.st_mode)) {
			pm_printf(ALPM_LOG_ERROR,
				_("cannot determine ownership of directory '%s'\n"), filename);
			goto targcleanup;
		}

		bname = mbasename(filename);
		dname = mdirname(filename);
		rpath = realpath(dname, NULL);

		if(!dname || !rpath) {
			pm_printf(ALPM_LOG_ERROR, _("cannot determine real path for '%s': %s\n"),
					filename, strerror(errno));
			goto targcleanup;
		}

		for(i = alpm_db_get_pkgcache(db_local); i && !found; i = alpm_list_next(i)) {
			alpm_pkg_t *info = i->data;
			alpm_filelist_t *filelist = alpm_pkg_get_files(info);
			size_t j;

			for(j = 0; j < filelist->count; j++) {
				const alpm_file_t *file = filelist->files + j;
				char *ppath, *pdname;
				const char *pkgfile = file->name;

				/* avoid the costly realpath usage if the basenames don't match */
				if(strcmp(mbasename(pkgfile), bname) != 0) {
					continue;
				}

				/* concatenate our file and the root path */
				if(rootlen + 1 + strlen(pkgfile) > PATH_MAX) {
					path[rootlen] = '\0'; /* reset path for error message */
					pm_printf(ALPM_LOG_ERROR, _("path too long: %s%s\n"), path, pkgfile);
					continue;
				}
				strcpy(path + rootlen, pkgfile);

				pdname = mdirname(path);
				ppath = realpath(pdname, NULL);
				free(pdname);

				if(!ppath) {
					pm_printf(ALPM_LOG_ERROR, _("cannot determine real path for '%s': %s\n"),
							path, strerror(errno));
					continue;
				}

				if(strcmp(ppath, rpath) == 0) {
					print_query_fileowner(filename, info);
					found = 1;
					free(ppath);
					break;
				}
				free(ppath);
			}
		}
		if(!found) {
			pm_printf(ALPM_LOG_ERROR, _("No package owns %s\n"), filename);
		}

targcleanup:
		if(!found) {
			ret++;
		}
		free(filename);
		free(rpath);
		free(dname);
	}

	return ret;
}

/* search the local database for a matching package */
static int query_search(alpm_list_t *targets)
{
	alpm_list_t *i, *searchlist;
	int freelist;
	alpm_db_t *db_local = alpm_get_localdb(config->handle);
	unsigned short cols;

	/* if we have a targets list, search for packages matching it */
	if(targets) {
		searchlist = alpm_db_search(db_local, targets);
		freelist = 1;
	} else {
		searchlist = alpm_db_get_pkgcache(db_local);
		freelist = 0;
	}
	if(searchlist == NULL) {
		return 1;
	}

	cols = getcols(fileno(stdout));
	for(i = searchlist; i; i = alpm_list_next(i)) {
		alpm_list_t *grp;
		alpm_pkg_t *pkg = i->data;

		if(!config->quiet) {
			printf(LOCAL_PREFIX "%s %s", alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
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

			/* we need a newline and initial indent first */
			fputs("\n    ", stdout);
			indentprint(alpm_pkg_get_desc(pkg), 4, cols);
		}
		fputc('\n', stdout);
	}

	/* we only want to free if the list was a search list */
	if(freelist) {
		alpm_list_free(searchlist);
	}
	return 0;
}

static int query_group(alpm_list_t *targets)
{
	alpm_list_t *i, *j;
	const char *grpname = NULL;
	int ret = 0;
	alpm_db_t *db_local = alpm_get_localdb(config->handle);

	if(targets == NULL) {
		for(j = alpm_db_get_groupcache(db_local); j; j = alpm_list_next(j)) {
			alpm_group_t *grp = j->data;
			const alpm_list_t *p;

			for(p = grp->packages; p; p = alpm_list_next(p)) {
				alpm_pkg_t *pkg = p->data;
				printf("%s %s\n", grp->name, alpm_pkg_get_name(pkg));
			}
		}
	} else {
		for(i = targets; i; i = alpm_list_next(i)) {
			alpm_group_t *grp;
			grpname = i->data;
			grp = alpm_db_get_group(db_local, grpname);
			if(grp) {
				const alpm_list_t *p;
				for(p = grp->packages; p; p = alpm_list_next(p)) {
					if(!config->quiet) {
						printf("%s %s\n", grpname,
								alpm_pkg_get_name(p->data));
					} else {
						printf("%s\n", alpm_pkg_get_name(p->data));
					}
				}
			} else {
				pm_printf(ALPM_LOG_ERROR, _("group '%s' was not found\n"), grpname);
				ret++;
			}
		}
	}
	return ret;
}

static int is_foreign(alpm_pkg_t *pkg)
{
	const char *pkgname = alpm_pkg_get_name(pkg);
	alpm_list_t *j;
	alpm_list_t *sync_dbs = alpm_get_syncdbs(config->handle);

	for(j = sync_dbs; j; j = alpm_list_next(j)) {
		if(alpm_db_get_pkg(j->data, pkgname)) {
			return 0;
		}
	}
	return 1;
}

static int is_unrequired(alpm_pkg_t *pkg)
{
	alpm_list_t *requiredby = alpm_pkg_compute_requiredby(pkg);
	if(requiredby == NULL) {
		return 1;
	}
	FREELIST(requiredby);
	return 0;
}

static int filter(alpm_pkg_t *pkg)
{
	/* check if this package was explicitly installed */
	if(config->op_q_explicit &&
			alpm_pkg_get_reason(pkg) != ALPM_PKG_REASON_EXPLICIT) {
		return 0;
	}
	/* check if this package was installed as a dependency */
	if(config->op_q_deps &&
			alpm_pkg_get_reason(pkg) != ALPM_PKG_REASON_DEPEND) {
		return 0;
	}
	/* check if this pkg is in a sync DB */
	if(config->op_q_native && is_foreign(pkg)) {
		return 0;
	}
	/* check if this pkg isn't in a sync DB */
	if(config->op_q_foreign && !is_foreign(pkg)) {
		return 0;
	}
	/* check if this pkg is unrequired */
	if(config->op_q_unrequired && !is_unrequired(pkg)) {
		return 0;
	}
	/* check if this pkg is outdated */
	if(config->op_q_upgrade && (alpm_sync_newversion(pkg,
					alpm_get_syncdbs(config->handle)) == NULL)) {
		return 0;
	}
	return 1;
}

static int display(alpm_pkg_t *pkg)
{
	int ret = 0;

	if(config->op_q_info) {
		if(config->op_q_isfile) {
			dump_pkg_full(pkg, 0);
		} else {
			dump_pkg_full(pkg, config->op_q_info > 1);
		}
	}
	if(config->op_q_list) {
		dump_pkg_files(pkg, config->quiet);
	}
	if(config->op_q_changelog) {
		dump_pkg_changelog(pkg);
	}
	if(config->op_q_check) {
		if(config->op_q_check == 1) {
			ret = check_pkg_fast(pkg);
		} else {
			ret = check_pkg_full(pkg);
		}
	}
	if(!config->op_q_info && !config->op_q_list
			&& !config->op_q_changelog && !config->op_q_check) {
		if(!config->quiet) {
			printf("%s %s\n", alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
		} else {
			printf("%s\n", alpm_pkg_get_name(pkg));
		}
	}
	return ret;
}

int pacman_query(alpm_list_t *targets)
{
	int ret = 0;
	int match = 0;
	alpm_list_t *i;
	alpm_pkg_t *pkg = NULL;
	alpm_db_t *db_local;

	/* First: operations that do not require targets */

	/* search for a package */
	if(config->op_q_search) {
		ret = query_search(targets);
		return ret;
	}

	/* looking for groups */
	if(config->group) {
		ret = query_group(targets);
		return ret;
	}

	if(config->op_q_foreign || config->op_q_upgrade) {
		if(check_syncdbs(1, 1)) {
			return 1;
		}
	}

	db_local = alpm_get_localdb(config->handle);

	/* operations on all packages in the local DB
	 * valid: no-op (plain -Q), list, info, check
	 * invalid: isfile, owns */
	if(targets == NULL) {
		if(config->op_q_isfile || config->op_q_owns) {
			pm_printf(ALPM_LOG_ERROR, _("no targets specified (use -h for help)\n"));
			return 1;
		}

		for(i = alpm_db_get_pkgcache(db_local); i; i = alpm_list_next(i)) {
			pkg = i->data;
			if(filter(pkg)) {
				int value = display(pkg);
				if(value != 0) {
					ret = 1;
				}
				match = 1;
			}
		}
		if(!match) {
			ret = 1;
		}
		return ret;
	}

	/* Second: operations that require target(s) */

	/* determine the owner of a file */
	if(config->op_q_owns) {
		ret = query_fileowner(targets);
		return ret;
	}

	/* operations on named packages in the local DB
	 * valid: no-op (plain -Q), list, info, check */
	for(i = targets; i; i = alpm_list_next(i)) {
		const char *strname = i->data;

		/* strip leading part of "local/pkgname" */
		if(strncmp(strname, LOCAL_PREFIX, strlen(LOCAL_PREFIX)) == 0) {
			strname += strlen(LOCAL_PREFIX);
		}

		if(config->op_q_isfile) {
			alpm_pkg_load(config->handle, strname, 1, 0, &pkg);
		} else {
			pkg = alpm_db_get_pkg(db_local, strname);
		}

		if(pkg == NULL) {
			switch(alpm_errno(config->handle)) {
				case ALPM_ERR_PKG_NOT_FOUND:
					pm_printf(ALPM_LOG_ERROR,
							_("package '%s' was not found\n"), strname);
					if(!config->op_q_isfile && access(strname, R_OK) == 0) {
						pm_printf(ALPM_LOG_WARNING,
								_("'%s' is a file, you might want to use %s.\n"),
								strname, "-p/--file");
					}
					break;
				default:
					pm_printf(ALPM_LOG_ERROR,
							_("could not load package '%s': %s\n"), strname,
							alpm_strerror(alpm_errno(config->handle)));
					break;
			}
			ret = 1;
			continue;
		}

		if(filter(pkg)) {
			int value = display(pkg);
			if(value != 0) {
				ret = 1;
			}
			match = 1;
		}

		if(config->op_q_isfile) {
			alpm_pkg_free(pkg);
			pkg = NULL;
		}
	}

	if(!match) {
		ret = 1;
	}

	return ret;
}

/* vim: set ts=2 sw=2 noet: */
