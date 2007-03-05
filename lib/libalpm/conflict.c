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
#if defined(__APPLE__) || defined(__OpenBSD__)
#include <sys/syslimits.h>
#endif
#include <sys/stat.h>
#include <libintl.h>

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


/** See if potential conflict 'name' matches package 'pkg'.
 * @param target the name of the parent package we're checking
 * @param depname the name of the dependency we're checking
 * @param pkg the package to check
 * @param conflict the name of the possible conflict
 * @return A depmissing struct indicating the conflict
 * @note The first two paramters are here to simplify the addition
 *			 of new 'depmiss' objects.
 *
 * TODO WTF is a 'depmissing' doing indicating a conflict??
 */
static pmdepmissing_t *does_conflict(const char *target, const char *depname,
																		 pmpkg_t *pkg, const char *conflict)
{
	alpm_list_t *i;

	/* check the actual package name, easy */
	if(strcmp(alpm_pkg_get_name(pkg), conflict) == 0) {
		_alpm_log(PM_LOG_DEBUG, _("   found conflict '%s' : package '%s'"), conflict, target);
		return(_alpm_depmiss_new(target, PM_DEP_TYPE_CONFLICT,
														 PM_DEP_MOD_ANY, depname, NULL));
	} else {
		/* check what this package provides, harder */
		for(i = alpm_pkg_get_provides(pkg); i; i = i->next) {
			const char *provision = i->data;

			if(strcmp(provision, conflict) == 0) {
				_alpm_log(PM_LOG_DEBUG, _("   found conflict '%s' : package '%s' provides '%s'"),
									conflict, target, provision);
				return(_alpm_depmiss_new(target, PM_DEP_TYPE_CONFLICT,
																 PM_DEP_MOD_ANY, depname, NULL));
			}
		}
	}
	return(NULL); /* not a conflict */
}

static alpm_list_t *chk_pkg_vs_db(alpm_list_t *baddeps, pmpkg_t *pkg, pmdb_t *db)
{
	pmdepmissing_t *miss = NULL;
	const char *pkgname;
	alpm_list_t *i, *j;

  pkgname = alpm_pkg_get_name(pkg);

	for(i = alpm_pkg_get_conflicts(pkg); i; i = i->next) {
		const char *conflict = i->data;

		if(strcmp(pkgname, conflict) == 0) {
			/* a package cannot conflict with itself -- that's just not nice */
			_alpm_log(PM_LOG_DEBUG, _("package '%s' conflicts with itself - packaging error"),
								pkgname);
			continue;
		}

		/* CHECK 1: check targets against database */
		_alpm_log(PM_LOG_DEBUG, _("checkconflicts: target '%s' vs db"), pkgname);

		for(j = _alpm_db_get_pkgcache(db); j; j = j->next) {
			pmpkg_t *dbpkg = j->data;

			if(strcmp(alpm_pkg_get_name(dbpkg), pkgname) == 0) {
				/* skip the package we're currently processing */
				continue;
			}

			miss = does_conflict(pkgname, alpm_pkg_get_name(dbpkg), dbpkg, conflict);
			if(miss && !_alpm_depmiss_isin(miss, baddeps)) {
				baddeps = alpm_list_add(baddeps, miss);
			} else {
				FREE(miss);
			}
		}
	}
	return(baddeps);
}

static alpm_list_t *chk_pkg_vs_targets(alpm_list_t *baddeps,
															 pmpkg_t *pkg, pmdb_t *db,
															 alpm_list_t *targets)
{
	pmdepmissing_t *miss = NULL;
	const char *pkgname;
	alpm_list_t *i, *j;

  pkgname = alpm_pkg_get_name(pkg);

	for(i = alpm_pkg_get_conflicts(pkg); i; i = i->next) {
		const char *conflict = i->data;

		if(strcmp(pkgname, conflict) == 0) {
			/* a package cannot conflict with itself -- that's just not nice */
			_alpm_log(PM_LOG_DEBUG, _("package '%s' conflicts with itself - packaging error"),
								pkgname);
			continue;
		}

		/* CHECK 2: check targets against targets */
		_alpm_log(PM_LOG_DEBUG, _("checkconflicts: target '%s' vs all targets"), pkgname);

		for(j = targets; j; j = j->next) {
			const char *targetname;
			pmpkg_t *target = j->data;
			targetname = alpm_pkg_get_name(target);

			if(strcmp(targetname, pkgname) == 0) {
				/* skip the package we're currently processing */
				continue;
			}

			miss = does_conflict(pkgname, targetname, target, conflict);
			if(miss && !_alpm_depmiss_isin(miss, baddeps)) {
				baddeps = alpm_list_add(baddeps, miss);
			} else {
				FREE(miss);
			}
		}
	}
	return(baddeps);
}

static alpm_list_t *chk_db_vs_targets(alpm_list_t *baddeps, pmpkg_t *pkg,
																			pmdb_t *db, alpm_list_t *targets)
{
	pmdepmissing_t *miss = NULL;
	const char *pkgname;
	alpm_list_t *i, *j;

	pkgname = alpm_pkg_get_name(pkg);

	_alpm_log(PM_LOG_DEBUG, _("checkconflicts: db vs target '%s'"), pkgname);
	
	for(i = _alpm_db_get_pkgcache(db); i; i = i->next) {
		alpm_list_t *conflicts = NULL;
		const char *dbpkgname;

		pmpkg_t *dbpkg = i->data;
		dbpkgname = alpm_pkg_get_name(dbpkg);

		if(strcmp(dbpkgname, pkgname) == 0) {
			/* skip the package we're currently processing */
			continue;
		}

		/* is this db package in the targets? if so use the
		 * new package's conflict list to pick up new changes */
		int use_newconflicts = 0;
		for(j = targets; j; j = j->next) {
			pmpkg_t *targ = j->data;
			if(strcmp(alpm_pkg_get_name(targ), dbpkgname) == 0) {
				_alpm_log(PM_LOG_DEBUG, _("target '%s' is also in target list, using NEW conflicts"),
									dbpkgname);
				conflicts = alpm_pkg_get_conflicts(targ);
				use_newconflicts = 1;
				break;
			}
		}
		/* if we didn't find newer conflicts, use the original list */
		if(!use_newconflicts) {
			conflicts = alpm_pkg_get_conflicts(dbpkg);
		}

		for(j = conflicts; j; j = j->next) {
			const char *conflict = j->data;


			miss = does_conflict(pkgname, dbpkgname, pkg, conflict);
			if(miss && !_alpm_depmiss_isin(miss, baddeps)) {
				baddeps = alpm_list_add(baddeps, miss);
			} else {
				FREE(miss);
			}
		}
	}
	return(baddeps);
}

/* Returns a alpm_list_t* of pmdepmissing_t pointers.
 *
 * conflicts are always name only
 */
alpm_list_t *_alpm_checkconflicts(pmdb_t *db, alpm_list_t *packages)
{
	alpm_list_t *i, *baddeps = NULL;

	ALPM_LOG_FUNC;

	if(db == NULL) {
		return(NULL);
	}

	for(i = packages; i; i = i->next) {
		pmpkg_t *pkg = i->data;
		if(pkg == NULL) {
			continue;
		}

		/* run three different conflict checks on each package */
		baddeps = chk_pkg_vs_db(baddeps, pkg, db);
		baddeps = chk_pkg_vs_targets(baddeps, pkg, db, packages);
		baddeps = chk_db_vs_targets(baddeps, pkg, db, packages);
	}

	/* debug loop */
	for(i = baddeps; i; i = i->next) {
		pmdepmissing_t *miss = i->data;
		_alpm_log(PM_LOG_DEBUG, _("\tCONFLICTS:: %s conflicts with %s"), miss->target, miss->depend.name);
	}

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
	STRNCPY(conflict->target, name1, PKG_NAME_LEN);
	STRNCPY(conflict->file, filestr, CONFLICT_FILE_LEN);
	if(name2) {
		STRNCPY(conflict->ctarget, name2, PKG_NAME_LEN);
	} else {
		conflict->ctarget[0] = '\0';
	}

	conflicts = alpm_list_add(conflicts, conflict);
	_alpm_log(PM_LOG_DEBUG, "found file conflict %s, packages %s and %s",
	          filestr, name1, name2 ? name2 : "(filesystem)");

	return(conflicts);
}

alpm_list_t *_alpm_db_find_conflicts(pmdb_t *db, pmtrans_t *trans, char *root)
{
	alpm_list_t *i, *j, *k;
	alpm_list_t *conflicts = NULL;
	alpm_list_t *tmpfiles = NULL;
	alpm_list_t *targets = trans->packages;
	int numtargs = alpm_list_count(targets);
	double percent;

	ALPM_LOG_FUNC;

	if(db == NULL || targets == NULL || root == NULL) {
		return(NULL);
	}

	for(i = targets; i; i = i->next) {
		pmpkg_t *p1, *p2, *dbpkg;
		char *filestr = NULL;
		char path[PATH_MAX+1];
		struct stat buf;

		p1 = i->data;
		if(!p1) {
			continue;
		}

		percent = (double)(alpm_list_count(targets) - alpm_list_count(i) + 1)
			                 / alpm_list_count(targets);
		PROGRESS(trans, PM_TRANS_PROGRESS_CONFLICTS_START, "", (percent * 100),
		         numtargs, (numtargs - alpm_list_count(i) +1));
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
				char path[PATH_MAX];
				for(k = tmpfiles; k; k = k->next) {
					snprintf(path, PATH_MAX, "%s%s", root, (char *)k->data);
					conflicts = add_fileconflict(conflicts, PM_CONFLICT_TYPE_TARGET, path,
																			 alpm_pkg_get_name(p1), alpm_pkg_get_name(p2));
				}
				alpm_list_free_inner(tmpfiles, &free);
				alpm_list_free(tmpfiles);
			}
		}

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

			/* stat the file - if it exists and is not a dir, do some checks */
			if(lstat(path, &buf) == 0 && !S_ISDIR(buf.st_mode)) {
				_alpm_log(PM_LOG_DEBUG, "checking possible conflict: %s", path);

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
			} else {
				_alpm_log(PM_LOG_DEBUG, "%s is a directory, not a conflict", path);
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
