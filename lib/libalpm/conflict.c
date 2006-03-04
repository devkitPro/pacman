/*
 *  conflict.c
 * 
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
 *  USA.
 */

#include "config.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
/* pacman */
#include "util.h"
#include "log.h"
#include "cache.h"
#include "deps.h"
#include "conflict.h"

/* Returns a PMList* of pmdepmissing_t pointers.
 *
 * conflicts are always name only
 */
PMList *_alpm_checkconflicts(pmdb_t *db, PMList *packages)
{
	pmpkg_t *info = NULL;
	PMList *i, *j, *k;
	PMList *baddeps = NULL;
	pmdepmissing_t *miss = NULL;

	if(db == NULL) {
		return(NULL);
	}

	for(i = packages; i; i = i->next) {
		pmpkg_t *tp = i->data;
		if(tp == NULL) {
			continue;
		}

		for(j = tp->conflicts; j; j = j->next) {
			if(!strcmp(tp->name, j->data)) {
				/* a package cannot conflict with itself -- that's just not nice */
				continue;
			}
			/* CHECK 1: check targets against database */
			_alpm_log(PM_LOG_DEBUG, "checkconflicts: targ '%s' vs db", tp->name);
			for(k = _alpm_db_get_pkgcache(db); k; k = k->next) {
				pmpkg_t *dp = (pmpkg_t *)k->data;
				if(!strcmp(dp->name, tp->name)) {
					/* a package cannot conflict with itself -- that's just not nice */
					continue;
				}
				if(!strcmp(j->data, dp->name)) {
					/* conflict */
					_alpm_log(PM_LOG_DEBUG, "targs vs db: found %s as a conflict for %s",
					          dp->name, tp->name);
					miss = _alpm_depmiss_new(tp->name, PM_DEP_TYPE_CONFLICT, PM_DEP_MOD_ANY, dp->name, NULL);
					if(!_alpm_depmiss_isin(miss, baddeps)) {
						baddeps = _alpm_list_add(baddeps, miss);
					} else {
						FREE(miss);
					}
				} else {
					/* see if dp provides something in tp's conflict list */
					PMList *m;
					for(m = dp->provides; m; m = m->next) {
						if(!strcmp(m->data, j->data)) {
							/* confict */
							_alpm_log(PM_LOG_DEBUG, "targs vs db: found %s as a conflict for %s",
							          dp->name, tp->name);
							miss = _alpm_depmiss_new(tp->name, PM_DEP_TYPE_CONFLICT, PM_DEP_MOD_ANY, dp->name, NULL);
							if(!_alpm_depmiss_isin(miss, baddeps)) {
								baddeps = _alpm_list_add(baddeps, miss);
							} else {
								FREE(miss);
							}
						}
					}
				}
			}
			/* CHECK 2: check targets against targets */
			_alpm_log(PM_LOG_DEBUG, "checkconflicts: targ '%s' vs targs", tp->name);
			for(k = packages; k; k = k->next) {
				pmpkg_t *otp = (pmpkg_t *)k->data;
				if(!strcmp(otp->name, tp->name)) {
					/* a package cannot conflict with itself -- that's just not nice */
					continue;
				}
				if(!strcmp(otp->name, (char *)j->data)) {
					/* otp is listed in tp's conflict list */
					_alpm_log(PM_LOG_DEBUG, "targs vs targs: found %s as a conflict for %s",
					          otp->name, tp->name);
					miss = _alpm_depmiss_new(tp->name, PM_DEP_TYPE_CONFLICT, PM_DEP_MOD_ANY, otp->name, NULL);
					if(!_alpm_depmiss_isin(miss, baddeps)) {
						baddeps = _alpm_list_add(baddeps, miss);
					} else {
						FREE(miss);
					}
				} else {
					/* see if otp provides something in tp's conflict list */ 
					PMList *m;
					for(m = otp->provides; m; m = m->next) {
						if(!strcmp(m->data, j->data)) {
							_alpm_log(PM_LOG_DEBUG, "targs vs targs: found %s as a conflict for %s",
							          otp->name, tp->name);
							miss = _alpm_depmiss_new(tp->name, PM_DEP_TYPE_CONFLICT, PM_DEP_MOD_ANY, otp->name, NULL);
							if(!_alpm_depmiss_isin(miss, baddeps)) {
								baddeps = _alpm_list_add(baddeps, miss);
							} else {
								FREE(miss);
							}
						}
					}
				}
			}
		}
		/* CHECK 3: check database against targets */
		_alpm_log(PM_LOG_DEBUG, "checkconflicts: db vs targ '%s'", tp->name);
		for(k = _alpm_db_get_pkgcache(db); k; k = k->next) {
			PMList *conflicts = NULL;
			int usenewconflicts = 0;

			info = k->data;
			if(!strcmp(info->name, tp->name)) {
				/* a package cannot conflict with itself -- that's just not nice */
				continue;
			}
			/* If this package (*info) is also in our packages PMList, use the
			 * conflicts list from the new package, not the old one (*info)
			 */
			for(j = packages; j; j = j->next) {
				pmpkg_t *pkg = j->data;
				if(!strcmp(pkg->name, info->name)) {
					// Use the new, to-be-installed package's conflicts
					conflicts = pkg->conflicts;
					usenewconflicts = 1;
				}
			}
			if(!usenewconflicts) {
				/* Use the old package's conflicts, it's the only set we have */
				conflicts = info->conflicts;
			}
			for(j = conflicts; j; j = j->next) {
				if(!strcmp((char *)j->data, tp->name)) {
					_alpm_log(PM_LOG_DEBUG, "db vs targs: found %s as a conflict for %s",
					          info->name, tp->name);
					miss = _alpm_depmiss_new(tp->name, PM_DEP_TYPE_CONFLICT, PM_DEP_MOD_ANY, info->name, NULL);
					if(!_alpm_depmiss_isin(miss, baddeps)) {
						baddeps = _alpm_list_add(baddeps, miss);
					} else {
						FREE(miss);
					}
				} else {
					/* see if the db package conflicts with something we provide */
					PMList *m;
					for(m = conflicts; m; m = m->next) {
						PMList *n;
						for(n = tp->provides; n; n = n->next) {
							if(!strcmp(m->data, n->data)) {
								_alpm_log(PM_LOG_DEBUG, "db vs targs: found %s as a conflict for %s",
								          info->name, tp->name);
								miss = _alpm_depmiss_new(tp->name, PM_DEP_TYPE_CONFLICT, PM_DEP_MOD_ANY, info->name, NULL);
								if(!_alpm_depmiss_isin(miss, baddeps)) {
									baddeps = _alpm_list_add(baddeps, miss);
								} else {
									FREE(miss);
								}
							}
						}
					}
				}
			}
		}
	}

	return(baddeps);
}

PMList *_alpm_db_find_conflicts(pmdb_t *db, PMList *targets, char *root, PMList **skip_list)
{
	PMList *i, *j, *k;
	char *filestr = NULL;
	char path[PATH_MAX+1];
	struct stat buf, buf2;
	PMList *conflicts = NULL;

	if(db == NULL || targets == NULL || root == NULL) {
		return(NULL);
	}

	/* CHECK 1: check every target against every target */
	for(i = targets; i; i = i->next) {
		pmpkg_t *p1 = (pmpkg_t*)i->data;
		for(j = i; j; j = j->next) {
			pmpkg_t *p2 = (pmpkg_t*)j->data;
			if(strcmp(p1->name, p2->name)) {
				for(k = p1->files; k; k = k->next) {
					filestr = k->data;
					if(filestr[strlen(filestr)-1] == '/') {
						/* this filename has a trailing '/', so it's a directory -- skip it. */
						continue;
					}
					if(!strcmp(filestr, "._install") || !strcmp(filestr, ".INSTALL")) {
						continue;
					}
					if(_alpm_list_is_strin(filestr, p2->files)) {
						pmconflict_t *conflict = malloc(sizeof(pmconflict_t));
						conflict->type = PM_CONFLICT_TYPE_TARGET;
						STRNCPY(conflict->target, p1->name, PKG_NAME_LEN);
						STRNCPY(conflict->file, filestr, CONFLICT_FILE_LEN);
						STRNCPY(conflict->ctarget, p2->name, PKG_NAME_LEN);
						conflicts = _alpm_list_add(conflicts, conflict);
					}
				}
			}
		}
	}

	/* CHECK 2: check every target against the filesystem */
	for(i = targets; i; i = i->next) {
		pmpkg_t *p = (pmpkg_t*)i->data;
		pmpkg_t *dbpkg = NULL;
		for(j = p->files; j; j = j->next) {
			int isdir = 0;
			filestr = (char*)j->data;
			snprintf(path, PATH_MAX, "%s%s", root, filestr);
			/* is this target a file or directory? */
			if(path[strlen(path)-1] == '/') {
				isdir = 1;
				path[strlen(path)-1] = '\0';
			}
			if(!lstat(path, &buf)) {
				int ok = 0;
				if(!S_ISLNK(buf.st_mode) && ((isdir && !S_ISDIR(buf.st_mode)) || (!isdir && S_ISDIR(buf.st_mode)))) {
					/* if the package target is a directory, and the filesystem target
					 * is not (or vice versa) then it's a conflict
					 */
					ok = 0;
					goto donecheck;
				}
				/* re-fetch with stat() instead of lstat() */
				stat(path, &buf);
				if(S_ISDIR(buf.st_mode)) {
					/* if it's a directory, then we have no conflict */
					ok = 1;
				} else {
					if(dbpkg == NULL) {
						dbpkg = _alpm_db_scan(db, p->name, INFRQ_DESC | INFRQ_FILES);
					}
					if(dbpkg && _alpm_list_is_strin(j->data, dbpkg->files)) {
						ok = 1;
					}
					/* Make sure that the supposedly-conflicting file is not actually just
					 * a symlink that points to a path that used to exist in the package.
					 */
					/* Check if any part of the conflicting file's path is a symlink */
					if(dbpkg && !ok) {
						char str[PATH_MAX];
						for(k = dbpkg->files; k; k = k->next) {
							snprintf(str, PATH_MAX, "%s%s", root, (char*)k->data);
							stat(str, &buf2);
							if(buf.st_ino == buf2.st_ino) {
								ok = 1;
							}
						}
					}
					/* Check if the conflicting file has been moved to another package/target */
					if(!ok) {
						/* Look at all the targets */
						for(k = targets; k && !ok; k = k->next) {
							pmpkg_t *p1 = (pmpkg_t *)k->data;
							/* As long as they're not the current package */
							if(strcmp(p1->name, p->name)) {
								pmpkg_t *dbpkg2 = NULL;
								dbpkg2 = _alpm_db_scan(db, p1->name, INFRQ_DESC | INFRQ_FILES);
								/* If it used to exist in there, but doesn't anymore */
								if(dbpkg2 && !_alpm_list_is_strin(filestr, p1->files) && _alpm_list_is_strin(filestr, dbpkg2->files)) {
									ok = 1;
									/* Add to the "skip list" of files that we shouldn't remove during an upgrade.
									 *
									 * This is a workaround for the following scenario:
									 *
									 *    - the old package A provides file X
									 *    - the new package A does not
									 *    - the new package B provides file X
									 *    - package A depends on B, so B is upgraded first
									 *
									 * Package B is upgraded, so file X is installed.  Then package A
									 * is upgraded, and it *removes* file X, since it no longer exists
									 * in package A.
									 *
									 * Our workaround is to scan through all "old" packages and all "new"
									 * ones, looking for files that jump to different packages.
									 */
									*skip_list = _alpm_list_add(*skip_list, strdup(filestr));
								}
								FREEPKG(dbpkg2);
							}
						}
					}
				}
donecheck:
				if(!ok) {
					pmconflict_t *conflict = malloc(sizeof(pmconflict_t));
					conflict->type = PM_CONFLICT_TYPE_FILE;
					STRNCPY(conflict->target, p->name, PKG_NAME_LEN);
					STRNCPY(conflict->file, filestr, CONFLICT_FILE_LEN);
					conflict->ctarget[0] = 0;
					conflicts = _alpm_list_add(conflicts, conflict);
				}
			}
		}
		FREEPKG(dbpkg);
	}

	return(conflicts);
}

/* vim: set ts=2 sw=2 noet: */
