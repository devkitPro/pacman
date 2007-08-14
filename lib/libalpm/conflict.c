/*
 *  conflict.c
 * 
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
 *  USA.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>

/* libalpm */
#include "conflict.h"
#include "alpm_list.h"
#include "handle.h"
#include "trans.h"
#include "util.h"
#include "error.h"
#include "log.h"
#include "cache.h"
#include "deps.h"


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
	pmdepend_t *conf = alpm_splitdep(conflict);
	int match = 0;

	match = alpm_depcmp(pkg2, conf);
	if(match) {
		_alpm_log(PM_LOG_DEBUG, "package %s conflicts with %s (by %s)",
				pkg1name, pkg2name, conflict);
	}
	free(conf);
	return(match);
}

/** Adds the pkg1/pkg2 conflict to the baddeps list
 * @param *baddeps list to add conflict to
 * @param pkg1 first package
 * @param pkg2 package causing conflict
 */
/* TODO WTF is a 'depmissing' doing indicating a conflict? */
static void add_conflict(alpm_list_t **baddeps, const char *pkg1,
		const char *pkg2)
{
	pmdepmissing_t *miss = _alpm_depmiss_new(pkg1, PM_DEP_TYPE_CONFLICT,
			PM_DEP_MOD_ANY, pkg2, NULL);
	if(miss && !_alpm_depmiss_isin(miss, *baddeps)) {
		*baddeps = alpm_list_add(*baddeps, miss);
	} else {
		FREE(miss);
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
						add_conflict(baddeps, pkg1name, pkg2name);
					} else {
						add_conflict(baddeps, pkg2name, pkg1name);
					}
				}
			}
		}
	}
}

/* Returns a alpm_list_t* of pmdepmissing_t pointers. */
alpm_list_t *_alpm_checkconflicts(pmdb_t *db, alpm_list_t *packages)
{
	alpm_list_t *baddeps = NULL;

	ALPM_LOG_FUNC;

	if(db == NULL) {
		return(NULL);
	}

	alpm_list_t *dblist = alpm_list_diff(_alpm_db_get_pkgcache(db), packages,
			_alpm_pkg_cmp);

	/* three checks to be done here for conflicts */
	_alpm_log(PM_LOG_DEBUG, "check targets vs db");
	check_conflict(packages, dblist, &baddeps, 1);
	_alpm_log(PM_LOG_DEBUG, "check db vs targets");
	check_conflict(dblist, packages, &baddeps, -1);
	_alpm_log(PM_LOG_DEBUG, "check targets vs targets");
	check_conflict(packages, packages, &baddeps, 0);

	alpm_list_free(dblist);
	return(baddeps);
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

	return(ret);
}

/* Adds pmconflict_t to a conflicts list. Pass the conflicts list, type (either
 * PM_CONFLICT_TYPE_TARGET or PM_CONFLICT_TYPE_FILE), a file string, and either
 * two package names or one package name and NULL. This is a wrapper for former
 * functionality that was done inline.
 */
static alpm_list_t *add_fileconflict(alpm_list_t *conflicts,
                    pmconflicttype_t type, const char *filestr,
										const char* name1, const char* name2)
{
	pmconflict_t *conflict = malloc(sizeof(pmconflict_t));
	if(conflict == NULL) {
		_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"),
				sizeof(pmconflict_t));
		return(conflicts);
	}
	conflict->type = type;
	strncpy(conflict->target, name1, PKG_NAME_LEN);
	strncpy(conflict->file, filestr, CONFLICT_FILE_LEN);
	if(name2) {
		strncpy(conflict->ctarget, name2, PKG_NAME_LEN);
	} else {
		conflict->ctarget[0] = '\0';
	}

	conflicts = alpm_list_add(conflicts, conflict);
	_alpm_log(PM_LOG_DEBUG, "found file conflict %s, packages %s and %s",
	          filestr, name1, name2 ? name2 : "(filesystem)");

	return(conflicts);
}

/* Find file conflicts that may occur during the transaction with two checks:
 * 1: check every target against every target
 * 2: check every target against the filesystem */
alpm_list_t *_alpm_db_find_conflicts(pmdb_t *db, pmtrans_t *trans, char *root)
{
	alpm_list_t *i, *conflicts = NULL;
	alpm_list_t *targets = trans->packages;
	int numtargs = alpm_list_count(targets);
	int current;

	ALPM_LOG_FUNC;

	if(db == NULL || targets == NULL || root == NULL) {
		return(NULL);
	}

	for(current = 1, i = targets; i; i = i->next, current++) {
		alpm_list_t *j, *k, *tmpfiles = NULL;
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
		for(j = i->next; j; j = j->next) {
			p2 = j->data;
			if(!p2) {
				continue;
			}
			_alpm_log(PM_LOG_DEBUG, "searching for file conflicts: %s and %s",
								alpm_pkg_get_name(p1), alpm_pkg_get_name(p2));
			tmpfiles = chk_fileconflicts(alpm_pkg_get_files(p1), alpm_pkg_get_files(p2));

			if(tmpfiles) {
				for(k = tmpfiles; k; k = k->next) {
					snprintf(path, PATH_MAX, "%s%s", root, (char *)k->data);
					conflicts = add_fileconflict(conflicts, PM_CONFLICT_TYPE_TARGET, path,
																			 alpm_pkg_get_name(p1), alpm_pkg_get_name(p2));
				}
				alpm_list_free_inner(tmpfiles, &free);
				alpm_list_free(tmpfiles);
			}
		}

		/* declarations for second check */
		struct stat buf;
		char *filestr = NULL;

		/* CHECK 2: check every target against the filesystem */
		_alpm_log(PM_LOG_DEBUG, "searching for filesystem conflicts: %s", p1->name);
		dbpkg = _alpm_db_get_pkgfromcache(db, p1->name);

		/* Do two different checks here. f the package is currently installed,
		 * then only check files that are new in the new package. If the package
		 * is not currently installed, then simply stat the whole filelist */
		if(dbpkg) {
			/* older ver of package currently installed */
			tmpfiles = chk_filedifference(alpm_pkg_get_files(p1), alpm_pkg_get_files(dbpkg));
		} else {
			/* no version of package currently installed */
			tmpfiles = alpm_list_strdup(alpm_pkg_get_files(p1));
		}

		/* loop over each file to be installed */
		for(j = tmpfiles; j; j = j->next) {
			filestr = j->data;

			snprintf(path, PATH_MAX, "%s%s", root, filestr);

			/* stat the file - if it exists, do some checks */
			if(lstat(path, &buf) != 0) {
				continue;
			}
			if(S_ISDIR(buf.st_mode)) {
				_alpm_log(PM_LOG_DEBUG, "%s is a directory, not a conflict", path);
			} else {
				_alpm_log(PM_LOG_DEBUG, "checking possible conflict: %s", path);

				/* Make sure the possible conflict is not a symlink that points to a
				 * path in the old package. This is kind of dirty with inode usage */
				if(dbpkg) {
					struct stat buf2;
					char str[PATH_MAX+1];
					unsigned ok = 0;
					for(k = dbpkg->files; k; k = k->next) {
						snprintf(str, PATH_MAX, "%s%s", root, (char*)k->data);
						if(!lstat(str, &buf2) && buf.st_ino == buf2.st_ino) {
							ok = 1;
							_alpm_log(PM_LOG_DEBUG, "conflict was a symlink: %s", path);
							break;
						}
					}
					if(ok == 1) {
						continue;
					}
				}

				/* Look at all the targets to see if file has changed hands */
				int resolved_conflict = 0; /* have we acted on this conflict? */
				for(k = targets; k; k = k->next) {
					p2 = k->data;
					if(!p2 || strcmp(p1->name, p2->name) == 0) {
						continue;
					}

					pmpkg_t *localp2 = _alpm_db_get_pkgfromcache(db, p2->name);

					/* Check if it used to exist in a package, but doesn't anymore */
					alpm_list_t *pkgfiles, *localfiles; /* added for readability */
					pkgfiles = alpm_pkg_get_files(p2);
					localfiles = alpm_pkg_get_files(localp2);

					if(localp2 && !alpm_list_find_str(pkgfiles, filestr)
						 && alpm_list_find_str(localfiles, filestr)) {
						/* check if the file is now in the backup array */
						if(alpm_list_find_str(alpm_pkg_get_backup(p1), filestr)) {
							/* keep file intact if it is in backup array */
							trans->skip_add = alpm_list_add(trans->skip_add, strdup(path));
							trans->skip_remove = alpm_list_add(trans->skip_remove, strdup(path));
							_alpm_log(PM_LOG_DEBUG, "file in backup array, adding to add and remove skiplist: %s", filestr);
							resolved_conflict = 1;
							break;
						} else {
							/* skip removal of file, but not add. this will prevent a second
							 * package from removing the file when it was already installed
							 * by its new owner */
							trans->skip_remove = alpm_list_add(trans->skip_remove, strdup(path));
							_alpm_log(PM_LOG_DEBUG, "file changed packages, adding to remove skiplist: %s", filestr);
							resolved_conflict = 1;
							break;
						}
					}
				}
				if(!resolved_conflict) {
					_alpm_log(PM_LOG_DEBUG, "file found in conflict: %s", path);
					conflicts = add_fileconflict(conflicts, PM_CONFLICT_TYPE_FILE,
																			 path, p1->name, NULL);
				}
			}
		}
		alpm_list_free_inner(tmpfiles, &free);
		alpm_list_free(tmpfiles);
	}

	return(conflicts);
}

const char SYMEXPORT *alpm_conflict_get_target(pmconflict_t *conflict)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(conflict != NULL, return(NULL));

	return conflict->target;
}

pmconflicttype_t SYMEXPORT alpm_conflict_get_type(pmconflict_t *conflict)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(-1));
	ASSERT(conflict != NULL, return(-1));

	return conflict->type;
}

const char SYMEXPORT *alpm_conflict_get_file(pmconflict_t *conflict)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(conflict != NULL, return(NULL));

	return conflict->file;
}

const char SYMEXPORT *alpm_conflict_get_ctarget(pmconflict_t *conflict)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(conflict != NULL, return(NULL));

	return conflict->ctarget;
}
/* vim: set ts=2 sw=2 noet: */
