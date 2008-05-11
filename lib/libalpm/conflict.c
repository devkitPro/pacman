/*
 *  conflict.c
 *
 *  Copyright (c) 2006-2010 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2006 by David Kimpe <dnaku@frugalware.org>
 *  Copyright (c) 2006 by Miklos Vajna <vmiklos@frugalware.org>
 *  Copyright (c) 2006 by Christian Hamar <krics@linuxforum.hu>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <dirent.h>

/* libalpm */
#include "conflict.h"
#include "alpm_list.h"
#include "handle.h"
#include "trans.h"
#include "util.h"
#include "log.h"
#include "deps.h"

pmconflict_t *_alpm_conflict_new(const char *package1, const char *package2, const char *reason)
{
	pmconflict_t *conflict;

	ALPM_LOG_FUNC;

	MALLOC(conflict, sizeof(pmconflict_t), RET_ERR(PM_ERR_MEMORY, NULL));

	STRDUP(conflict->package1, package1, RET_ERR(PM_ERR_MEMORY, NULL));
	STRDUP(conflict->package2, package2, RET_ERR(PM_ERR_MEMORY, NULL));
	STRDUP(conflict->reason, reason, RET_ERR(PM_ERR_MEMORY, NULL));

	return(conflict);
}

void _alpm_conflict_free(pmconflict_t *conflict)
{
	FREE(conflict->package2);
	FREE(conflict->package1);
	FREE(conflict->reason);
	FREE(conflict);
}

pmconflict_t *_alpm_conflict_dup(const pmconflict_t *conflict)
{
	pmconflict_t *newconflict;
	CALLOC(newconflict, 1, sizeof(pmconflict_t), RET_ERR(PM_ERR_MEMORY, NULL));

	STRDUP(newconflict->package1, conflict->package1, RET_ERR(PM_ERR_MEMORY, NULL));
	STRDUP(newconflict->package2, conflict->package2, RET_ERR(PM_ERR_MEMORY, NULL));
	STRDUP(newconflict->reason, conflict->reason, RET_ERR(PM_ERR_MEMORY, NULL));

	return(newconflict);
}

int _alpm_conflict_isin(pmconflict_t *needle, alpm_list_t *haystack)
{
	alpm_list_t *i;

	ALPM_LOG_FUNC;

	for(i = haystack; i; i = i->next) {
		pmconflict_t *conflict = i->data;
		char *cpkg1 = conflict->package1;
		char *cpkg2 = conflict->package2;
		char *npkg1 = needle->package1;
		char *npkg2 = needle->package2;
		if((strcmp(cpkg1, npkg1) == 0  && strcmp(cpkg2, npkg2) == 0)
				|| (strcmp(cpkg1, npkg2) == 0 && strcmp(cpkg2, npkg1) == 0)) {
			return(1);
		}
	}

	return(0);
}

/** Check if pkg1 conflicts with pkg2
 * @param pkg1 package we are looking at
 * @param conflict name of the possible conflict
 * @param pkg2 package to check
 * @return 0 for no conflict, non-zero otherwise
 */
static int does_conflict(pmpkg_t *pkg1, const char *conflict, pmpkg_t *pkg2)
{
	const char *pkg1name = alpm_pkg_get_name(pkg1);
	const char *pkg2name = alpm_pkg_get_name(pkg2);
	pmdepend_t *conf = _alpm_splitdep(conflict);
	int match = 0;

	match = alpm_depcmp(pkg2, conf);
	if(match) {
		_alpm_log(PM_LOG_DEBUG, "package %s conflicts with %s (by %s)\n",
				pkg1name, pkg2name, conflict);
	}
	_alpm_dep_free(conf);
	return(match);
}

/** Adds the pkg1/pkg2 conflict to the baddeps list
 * @param *baddeps list to add conflict to
 * @param pkg1 first package
 * @param pkg2 package causing conflict
 */
static void add_conflict(alpm_list_t **baddeps, const char *pkg1,
		const char *pkg2, const char *reason)
{
	pmconflict_t *conflict = _alpm_conflict_new(pkg1, pkg2, reason);
	if(conflict && !_alpm_conflict_isin(conflict, *baddeps)) {
		*baddeps = alpm_list_add(*baddeps, conflict);
	} else {
		_alpm_conflict_free(conflict);
	}
}

/** Check if packages from list1 conflict with packages from list2.
 * This looks at the conflicts fields of all packages from list1, and sees
 * if they match packages from list2.
 * If a conflict (pkg1, pkg2) is found, it is added to the baddeps list
 * in this order if order >= 0, or reverse order (pkg2,pkg1) otherwise.
 *
 * @param list1 first list of packages
 * @param list2 second list of packages
 * @param *baddeps list to store conflicts
 * @param order if >= 0 the conflict order is preserved, if < 0 it's reversed
 */
static void check_conflict(alpm_list_t *list1, alpm_list_t *list2,
		alpm_list_t **baddeps, int order) {
	alpm_list_t *i, *j, *k;

	if(!baddeps) {
		return;
	}
	for(i = list1; i; i = i->next) {
		pmpkg_t *pkg1 = i->data;
		const char *pkg1name = alpm_pkg_get_name(pkg1);

		for(j = alpm_pkg_get_conflicts(pkg1); j; j = j->next) {
			const char *conflict = j->data;

			for(k = list2; k; k = k->next) {
				pmpkg_t *pkg2 = k->data;
				const char *pkg2name = alpm_pkg_get_name(pkg2);

				if(strcmp(pkg1name, pkg2name) == 0) {
					/* skip the package we're currently processing */
					continue;
				}

				if(does_conflict(pkg1, conflict, pkg2)) {
					if(order >= 0) {
						add_conflict(baddeps, pkg1name, pkg2name, conflict);
					} else {
						add_conflict(baddeps, pkg2name, pkg1name, conflict);
					}
				}
			}
		}
	}
}

/* Check for inter-conflicts */
alpm_list_t *_alpm_innerconflicts(alpm_list_t *packages)
{
	alpm_list_t *baddeps = NULL;

	ALPM_LOG_FUNC;

	_alpm_log(PM_LOG_DEBUG, "check targets vs targets\n");
	check_conflict(packages, packages, &baddeps, 0);

	return(baddeps);
}

/* Check for target vs (db - target) conflicts
 * In case of conflict the package1 field of pmdepconflict_t contains
 * the target package, package2 field contains the local package
 */
alpm_list_t *_alpm_outerconflicts(pmdb_t *db, alpm_list_t *packages)
{
	alpm_list_t *baddeps = NULL;

	ALPM_LOG_FUNC;

	if(db == NULL) {
		return(NULL);
	}

	alpm_list_t *dblist = alpm_list_diff(_alpm_db_get_pkgcache(db), packages,
			_alpm_pkg_cmp);

	/* two checks to be done here for conflicts */
	_alpm_log(PM_LOG_DEBUG, "check targets vs db\n");
	check_conflict(packages, dblist, &baddeps, 1);
	_alpm_log(PM_LOG_DEBUG, "check db vs targets\n");
	check_conflict(dblist, packages, &baddeps, -1);

	alpm_list_free(dblist);
	return(baddeps);
}

/** Check the package conflicts in a database
 *
 * @param pkglist the list of packages to check
 * @return an alpm_list_t of pmconflict_t
 */
alpm_list_t SYMEXPORT *alpm_checkconflicts(alpm_list_t *pkglist) {
	return(_alpm_innerconflicts(pkglist));
}

/* Returns a alpm_list_t* of file conflicts.
 *  Hooray for set-intersects!
 *  Pre-condition: both lists are sorted!
 */
static alpm_list_t *chk_fileconflicts(alpm_list_t *filesA, alpm_list_t *filesB)
{
	alpm_list_t *ret = NULL;
	alpm_list_t *pA = filesA, *pB = filesB;

	while(pA && pB) {
		const char *strA = pA->data;
		const char *strB = pB->data;
		/* skip directories, we don't care about dir conflicts */
		if(strA[strlen(strA)-1] == '/') {
			pA = pA->next;
		} else if(strB[strlen(strB)-1] == '/') {
			pB = pB->next;
		} else {
			int cmp = strcmp(strA, strB);
			if(cmp < 0) {
				/* item only in filesA, ignore it */
				pA = pA->next;
			} else if(cmp > 0) {
				/* item only in filesB, ignore it */
				pB = pB->next;
			} else {
				/* item in both, record it */
				ret = alpm_list_add(ret, strdup(strA));
				pA = pA->next;
				pB = pB->next;
			}
	  }
	}

	return(ret);
}

/* Returns a alpm_list_t* of files that are in filesA but *NOT* in filesB
 *  This is an 'A minus B' set operation
 *  Pre-condition: both lists are sorted!
 */
static alpm_list_t *chk_filedifference(alpm_list_t *filesA, alpm_list_t *filesB)
{
	alpm_list_t *ret = NULL;
	alpm_list_t *pA = filesA, *pB = filesB;

	/* if both filesA and filesB have entries, do this loop */
	while(pA && pB) {
		const char *strA = pA->data;
		const char *strB = pB->data;
		/* skip directories, we don't care about dir conflicts */
		if(strA[strlen(strA)-1] == '/') {
			pA = pA->next;
		} else if(strB[strlen(strB)-1] == '/') {
			pB = pB->next;
		} else {
			int cmp = strcmp(strA, strB);
			if(cmp < 0) {
				/* item only in filesA, record it */
				ret = alpm_list_add(ret, strdup(strA));
				pA = pA->next;
			} else if(cmp > 0) {
				/* item only in fileB, but this means nothing */
				pB = pB->next;
			} else {
				/* item in both, ignore it */
				pA = pA->next;
				pB = pB->next;
			}
	  }
	}
	/* ensure we have completely emptied pA */
	while(pA) {
		const char *strA = pA->data;
		/* skip directories */
		if(strA[strlen(strA)-1] != '/') {
			ret = alpm_list_add(ret, strdup(strA));
		}
		pA = pA->next;
	}

	return(ret);
}

/* Adds pmfileconflict_t to a conflicts list. Pass the conflicts list, type (either
 * PM_FILECONFLICT_TARGET or PM_FILECONFLICT_FILESYSTEM), a file string, and either
 * two package names or one package name and NULL. This is a wrapper for former
 * functionality that was done inline.
 */
static alpm_list_t *add_fileconflict(alpm_list_t *conflicts,
                    pmfileconflicttype_t type, const char *filestr,
                    const char* name1, const char* name2)
{
	pmfileconflict_t *conflict;
	MALLOC(conflict, sizeof(pmfileconflict_t), RET_ERR(PM_ERR_MEMORY, NULL));

	conflict->type = type;
	STRDUP(conflict->target, name1, RET_ERR(PM_ERR_MEMORY, NULL));
	STRDUP(conflict->file, filestr, RET_ERR(PM_ERR_MEMORY, NULL));
	if(name2) {
		STRDUP(conflict->ctarget, name2, RET_ERR(PM_ERR_MEMORY, NULL));
	} else {
		conflict->ctarget = "";
	}

	conflicts = alpm_list_add(conflicts, conflict);
	_alpm_log(PM_LOG_DEBUG, "found file conflict %s, packages %s and %s\n",
	          filestr, name1, name2 ? name2 : "(filesystem)");

	return(conflicts);
}

void _alpm_fileconflict_free(pmfileconflict_t *conflict)
{
	if(strlen(conflict->ctarget) > 0) {
		FREE(conflict->ctarget);
	}
	FREE(conflict->file);;
	FREE(conflict->target);
	FREE(conflict);
}

static int dir_belongsto_pkg(char *dirpath, pmpkg_t *pkg)
{
	struct dirent *ent = NULL;
	struct stat sbuf;
	char path[PATH_MAX];
	char abspath[PATH_MAX];
	DIR *dir;

	snprintf(abspath, PATH_MAX, "%s%s", handle->root, dirpath);
	dir = opendir(abspath);
	if(dir == NULL) {
		return(1);
	}
	while((ent = readdir(dir)) != NULL) {
		const char *name = ent->d_name;

		if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
			continue;
		}
		snprintf(path, PATH_MAX, "%s/%s", dirpath, name);
		snprintf(abspath, PATH_MAX, "%s%s", handle->root, path);
		if(stat(abspath, &sbuf) != 0) {
			continue;
		}
		if(S_ISDIR(sbuf.st_mode)) {
			if(dir_belongsto_pkg(path, pkg)) {
				continue;
			} else {
				closedir(dir);
				return(0);
			}
		} else {
			if(alpm_list_find_str(alpm_pkg_get_files(pkg),path)) {
				continue;
			} else {
				closedir(dir);
				return(0);
			}
		}
	}
	closedir(dir);
	return(1);
}

/* Find file conflicts that may occur during the transaction with two checks:
 * 1: check every target against every target
 * 2: check every target against the filesystem */
alpm_list_t *_alpm_db_find_fileconflicts(pmdb_t *db, pmtrans_t *trans,
		alpm_list_t *upgrade, alpm_list_t *remove)
{
	alpm_list_t *i, *j, *conflicts = NULL;
	int numtargs = alpm_list_count(upgrade);
	int current;

	ALPM_LOG_FUNC;

	if(db == NULL || upgrade == NULL || trans == NULL) {
		return(NULL);
	}

	/* TODO this whole function needs a huge change, which hopefully will
	 * be possible with real transactions. Right now we only do half as much
	 * here as we do when we actually extract files in add.c with our 12
	 * different cases. */
	for(current = 1, i = upgrade; i; i = i->next, current++) {
		alpm_list_t *k, *tmpfiles = NULL;
		pmpkg_t *p1, *p2, *dbpkg;
		char path[PATH_MAX+1];

		p1 = i->data;
		if(!p1) {
			continue;
		}

		double percent = (double)current / numtargs;
		PROGRESS(trans, PM_TRANS_PROGRESS_CONFLICTS_START, "", (percent * 100),
		         numtargs, current);
		/* CHECK 1: check every target against every target */
		_alpm_log(PM_LOG_DEBUG, "searching for file conflicts: %s\n",
								alpm_pkg_get_name(p1));
		for(j = i->next; j; j = j->next) {
			p2 = j->data;
			if(!p2) {
				continue;
			}
			tmpfiles = chk_fileconflicts(alpm_pkg_get_files(p1), alpm_pkg_get_files(p2));

			if(tmpfiles) {
				for(k = tmpfiles; k; k = k->next) {
					snprintf(path, PATH_MAX, "%s%s", handle->root, (char *)k->data);
					conflicts = add_fileconflict(conflicts, PM_FILECONFLICT_TARGET, path,
							alpm_pkg_get_name(p1), alpm_pkg_get_name(p2));
				}
				FREELIST(tmpfiles);
			}
		}

		/* declarations for second check */
		struct stat lsbuf, sbuf;
		char *filestr = NULL;

		/* CHECK 2: check every target against the filesystem */
		_alpm_log(PM_LOG_DEBUG, "searching for filesystem conflicts: %s\n", p1->name);
		dbpkg = _alpm_db_get_pkgfromcache(db, p1->name);

		/* Do two different checks here. If the package is currently installed,
		 * then only check files that are new in the new package. If the package
		 * is not currently installed, then simply stat the whole filelist */
		if(dbpkg) {
			/* older ver of package currently installed */
			tmpfiles = chk_filedifference(alpm_pkg_get_files(p1),
					alpm_pkg_get_files(dbpkg));
		} else {
			/* no version of package currently installed */
			tmpfiles = alpm_list_strdup(alpm_pkg_get_files(p1));
		}

		for(j = tmpfiles; j; j = j->next) {
			filestr = j->data;

			snprintf(path, PATH_MAX, "%s%s", handle->root, filestr);

			/* stat the file - if it exists, do some checks */
			if(_alpm_lstat(path, &lsbuf) != 0) {
				continue;
			}
			stat(path, &sbuf);

			if(path[strlen(path)-1] == '/') {
				if(S_ISDIR(lsbuf.st_mode)) {
					_alpm_log(PM_LOG_DEBUG, "%s is a directory, not a conflict\n", path);
					continue;
				} else if(S_ISLNK(lsbuf.st_mode) && S_ISDIR(sbuf.st_mode)) {
					_alpm_log(PM_LOG_DEBUG,
							"%s is a symlink to a dir, hopefully not a conflict\n", path);
					continue;
				}
			}
			_alpm_log(PM_LOG_DEBUG, "checking possible conflict: %s\n", path);

			int resolved_conflict = 0; /* have we acted on this conflict? */

			/* Check remove list (will we remove the conflicting local file?) */
			for(k = remove; k && !resolved_conflict; k = k->next) {
				pmpkg_t *rempkg = k->data;
				if(rempkg && alpm_list_find_str(alpm_pkg_get_files(rempkg), filestr)) {
					_alpm_log(PM_LOG_DEBUG, "local file will be removed, not a conflict: %s\n", filestr);
					resolved_conflict = 1;
				}
			}

			/* Look at all the targets to see if file has changed hands */
			for(k = upgrade; k && !resolved_conflict; k = k->next) {
				p2 = k->data;
				if(!p2 || strcmp(p1->name, p2->name) == 0) {
					continue;
				}
				pmpkg_t *localp2 = _alpm_db_get_pkgfromcache(db, p2->name);

				/* localp2->files will be removed (target conflicts are handled by CHECK 1) */
				if(localp2 && alpm_list_find_str(alpm_pkg_get_files(localp2), filestr)) {
					/* skip removal of file, but not add. this will prevent a second
					 * package from removing the file when it was already installed
					 * by its new owner (whether the file is in backup array or not */
					trans->skip_remove = alpm_list_add(trans->skip_remove, strdup(filestr));
					_alpm_log(PM_LOG_DEBUG, "file changed packages, adding to remove skiplist: %s\n", filestr);
					resolved_conflict = 1;
				}
			}

			/* check if all files of the dir belong to the installed pkg */
			if(!resolved_conflict && S_ISDIR(lsbuf.st_mode) && dbpkg) {
				char *dir = malloc(strlen(filestr) + 2);
				sprintf(dir, "%s/", filestr);
				if(alpm_list_find_str(alpm_pkg_get_files(dbpkg),dir)) {
					_alpm_log(PM_LOG_DEBUG, "check if all files in %s belongs to %s\n",
							dir, dbpkg->name);
					resolved_conflict = dir_belongsto_pkg(filestr, dbpkg);
				}
				free(dir);
			}

			if(!resolved_conflict && dbpkg) {
				char *rpath = calloc(PATH_MAX+1, sizeof(char));
				if(!realpath(path, rpath)) {
					FREE(rpath);
					continue;
				}
				char *filestr = rpath + strlen(handle->root);
				if(alpm_list_find_str(alpm_pkg_get_files(dbpkg),filestr)) {
					resolved_conflict = 1;
				}
				free(rpath);
			}

			if(!resolved_conflict) {
				_alpm_log(PM_LOG_DEBUG, "file found in conflict: %s\n", path);
				conflicts = add_fileconflict(conflicts, PM_FILECONFLICT_FILESYSTEM,
						path, p1->name, NULL);
			}
		}
		FREELIST(tmpfiles);
	}

	return(conflicts);
}

const char SYMEXPORT *alpm_conflict_get_package1(pmconflict_t *conflict)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(conflict != NULL, return(NULL));

	return conflict->package1;
}

const char SYMEXPORT *alpm_conflict_get_package2(pmconflict_t *conflict)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(conflict != NULL, return(NULL));

	return conflict->package2;
}

const char SYMEXPORT *alpm_conflict_get_reason(pmconflict_t *conflict)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(conflict != NULL, return(NULL));

	return conflict->reason;
}

const char SYMEXPORT *alpm_fileconflict_get_target(pmfileconflict_t *conflict)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(conflict != NULL, return(NULL));

	return conflict->target;
}

pmfileconflicttype_t SYMEXPORT alpm_fileconflict_get_type(pmfileconflict_t *conflict)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(-1));
	ASSERT(conflict != NULL, return(-1));

	return conflict->type;
}

const char SYMEXPORT *alpm_fileconflict_get_file(pmfileconflict_t *conflict)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(conflict != NULL, return(NULL));

	return conflict->file;
}

const char SYMEXPORT *alpm_fileconflict_get_ctarget(pmfileconflict_t *conflict)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(conflict != NULL, return(NULL));

	return conflict->ctarget;
}
/* vim: set ts=2 sw=2 noet: */
