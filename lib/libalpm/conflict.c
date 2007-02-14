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

#if defined(__APPLE__) || defined(__OpenBSD__)
#include <sys/syslimits.h>
#endif

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <libintl.h>
/* pacman */
#include "handle.h"
#include "alpm_list.h"
#include "trans.h"
#include "util.h"
#include "error.h"
#include "log.h"
#include "cache.h"
#include "deps.h"
#include "conflict.h"

/* Returns a alpm_list_t* of pmdepmissing_t pointers.
 *
 * conflicts are always name only
 */
alpm_list_t *_alpm_checkconflicts(pmdb_t *db, alpm_list_t *packages)
{
	pmpkg_t *info = NULL;
	alpm_list_t *i, *j, *k;
	alpm_list_t *baddeps = NULL;
	pmdepmissing_t *miss = NULL;

	ALPM_LOG_FUNC;

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
			_alpm_log(PM_LOG_DEBUG, _("checkconflicts: targ '%s' vs db"), tp->name);
			for(k = _alpm_db_get_pkgcache(db, INFRQ_DEPENDS); k; k = k->next) {
				pmpkg_t *dp = (pmpkg_t *)k->data;
				if(!strcmp(dp->name, tp->name)) {
					/* a package cannot conflict with itself -- that's just not nice */
					continue;
				}
				if(!strcmp(j->data, dp->name)) {
					/* conflict */
					_alpm_log(PM_LOG_DEBUG, _("targs vs db: found %s as a conflict for %s"),
					          dp->name, tp->name);
					miss = _alpm_depmiss_new(tp->name, PM_DEP_TYPE_CONFLICT, PM_DEP_MOD_ANY, dp->name, NULL);
					if(!_alpm_depmiss_isin(miss, baddeps)) {
						baddeps = alpm_list_add(baddeps, miss);
					} else {
						FREE(miss);
					}
				} else {
					/* see if dp provides something in tp's conflict list */
					alpm_list_t *m;
					for(m = dp->provides; m; m = m->next) {
						if(!strcmp(m->data, j->data)) {
							/* confict */
							_alpm_log(PM_LOG_DEBUG, _("targs vs db: found %s as a conflict for %s"),
							          dp->name, tp->name);
							miss = _alpm_depmiss_new(tp->name, PM_DEP_TYPE_CONFLICT, PM_DEP_MOD_ANY, dp->name, NULL);
							if(!_alpm_depmiss_isin(miss, baddeps)) {
								baddeps = alpm_list_add(baddeps, miss);
							} else {
								FREE(miss);
							}
						}
					}
				}
			}
			/* CHECK 2: check targets against targets */
			_alpm_log(PM_LOG_DEBUG, _("checkconflicts: targ '%s' vs targs"), tp->name);
			for(k = packages; k; k = k->next) {
				pmpkg_t *otp = (pmpkg_t *)k->data;
				if(!strcmp(otp->name, tp->name)) {
					/* a package cannot conflict with itself -- that's just not nice */
					continue;
				}
				if(!strcmp(otp->name, (char *)j->data)) {
					/* otp is listed in tp's conflict list */
					_alpm_log(PM_LOG_DEBUG, _("targs vs targs: found %s as a conflict for %s"),
					          otp->name, tp->name);
					miss = _alpm_depmiss_new(tp->name, PM_DEP_TYPE_CONFLICT, PM_DEP_MOD_ANY, otp->name, NULL);
					if(!_alpm_depmiss_isin(miss, baddeps)) {
						baddeps = alpm_list_add(baddeps, miss);
					} else {
						FREE(miss);
					}
				} else {
					/* see if otp provides something in tp's conflict list */ 
					alpm_list_t *m;
					for(m = otp->provides; m; m = m->next) {
						if(!strcmp(m->data, j->data)) {
							_alpm_log(PM_LOG_DEBUG, _("targs vs targs: found %s as a conflict for %s"),
							          otp->name, tp->name);
							miss = _alpm_depmiss_new(tp->name, PM_DEP_TYPE_CONFLICT, PM_DEP_MOD_ANY, otp->name, NULL);
							if(!_alpm_depmiss_isin(miss, baddeps)) {
								baddeps = alpm_list_add(baddeps, miss);
							} else {
								FREE(miss);
							}
						}
					}
				}
			}
		}
		/* CHECK 3: check database against targets */
		_alpm_log(PM_LOG_DEBUG, _("checkconflicts: db vs targ '%s'"), tp->name);
		for(k = _alpm_db_get_pkgcache(db, INFRQ_DEPENDS); k; k = k->next) {
			alpm_list_t *conflicts = NULL;
			int usenewconflicts = 0;

			info = k->data;
			if(!strcmp(info->name, tp->name)) {
				/* a package cannot conflict with itself -- that's just not nice */
				continue;
			}
			/* If this package (*info) is also in our packages alpm_list_t, use the
			 * conflicts list from the new package, not the old one (*info)
			 */
			for(j = packages; j; j = j->next) {
				pmpkg_t *pkg = j->data;
				if(!strcmp(pkg->name, info->name)) {
					/* Use the new, to-be-installed package's conflicts */
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
					_alpm_log(PM_LOG_DEBUG, _("db vs targs: found %s as a conflict for %s"),
					          info->name, tp->name);
					miss = _alpm_depmiss_new(tp->name, PM_DEP_TYPE_CONFLICT, PM_DEP_MOD_ANY, info->name, NULL);
					if(!_alpm_depmiss_isin(miss, baddeps)) {
						baddeps = alpm_list_add(baddeps, miss);
					} else {
						FREE(miss);
					}
				} else {
					/* see if the db package conflicts with something we provide */
					alpm_list_t *m;
					for(m = conflicts; m; m = m->next) {
						alpm_list_t *n;
						for(n = tp->provides; n; n = n->next) {
							if(!strcmp(m->data, n->data)) {
								_alpm_log(PM_LOG_DEBUG, _("db vs targs: found %s as a conflict for %s"),
								          info->name, tp->name);
								miss = _alpm_depmiss_new(tp->name, PM_DEP_TYPE_CONFLICT, PM_DEP_MOD_ANY, info->name, NULL);
								if(!_alpm_depmiss_isin(miss, baddeps)) {
									baddeps = alpm_list_add(baddeps, miss);
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
	for(alpm_list_t *i = ret; i; i = i->next) {
		_alpm_log(PM_LOG_DEBUG, "found conflict = %s", i->data);
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
	for(alpm_list_t *i = ret; i; i = i->next) {
		_alpm_log(PM_LOG_DEBUG, "found conflict = %s", i->data);
	}
	return(ret);
}

/* Adds a PM_CONFLICT_TYPE_FILE to a conflicts list.
 * Pass the conflicts list, package name, and a file string of the conflict.
 */
static alpm_list_t *add_fileconflict(alpm_list_t *conflicts, char *name,
                                     char *filestr)
{
	pmconflict_t *conflict = malloc(sizeof(pmconflict_t));
	if(conflict == NULL) {
		_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"),
				sizeof(pmconflict_t));
		return(conflicts);
	}
	conflict->type = PM_CONFLICT_TYPE_FILE;
	STRNCPY(conflict->target, name, PKG_NAME_LEN);
	STRNCPY(conflict->file, filestr, CONFLICT_FILE_LEN);
	conflict->ctarget[0] = 0;
	conflicts = alpm_list_add(conflicts, conflict);

	return(conflicts);
}

alpm_list_t *_alpm_db_find_conflicts(pmdb_t *db, pmtrans_t *trans, char *root,
                                     alpm_list_t **skip_list)
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

		p1 = (pmpkg_t*)i->data;
		percent = (double)(alpm_list_count(targets) - alpm_list_count(i) + 1)
			                 / alpm_list_count(targets);
		PROGRESS(trans, PM_TRANS_PROGRESS_CONFLICTS_START, "", (percent * 100),
		         numtargs, (numtargs - alpm_list_count(i) +1));
		/* CHECK 1: check every target against every target */
		for(j = i->next; j; j = j->next) {
			p2 = (pmpkg_t*)j->data;
			tmpfiles = chk_fileconflicts(p1->files, p2->files);

			if(tmpfiles) {
				for(k = tmpfiles; k; k = k->next) {
					pmconflict_t *conflict = malloc(sizeof(pmconflict_t));
					if(conflict == NULL) {
						_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"),
											sizeof(pmconflict_t));
						continue;
					}
					conflict->type = PM_CONFLICT_TYPE_TARGET;
					STRNCPY(conflict->target, p1->name, PKG_NAME_LEN);
					STRNCPY(conflict->file, k->data, CONFLICT_FILE_LEN);
					STRNCPY(conflict->ctarget, p2->name, PKG_NAME_LEN);
					conflicts = alpm_list_add(conflicts, conflict);
				}
				alpm_list_free_inner(tmpfiles, &free);
				alpm_list_free(tmpfiles);
			}
		}

		/* CHECK 2: check every target against the filesystem */
		dbpkg = _alpm_db_get_pkgfromcache(db, p1->name);

		/* Do two different checks here. f the package is currently installed,
		 * then only check files that are new in the new package. If the package
		 * is not currently installed, then simply stat the whole filelist */
		if(dbpkg) {
			/* older ver of package currently installed */
			tmpfiles = chk_filedifference(p1->files, alpm_pkg_get_files(dbpkg));
			for(j = tmpfiles; j; j = j->next) {
				filestr = j->data;

				snprintf(path, PATH_MAX, "%s%s", root, filestr);

				/* stat the file - if it exists and is not a dir, do some checks */
				if(lstat(path, &buf) == 0 && !S_ISDIR(buf.st_mode)) {
					/* Look at all the targets to see if file has changed hands */
					for(k = targets; k; k = k->next) {
						pmpkg_t *p2 = (pmpkg_t *)k->data;
						/* Ensure we aren't looking at current package */
						if(strcmp(p2->name, p1->name)) {
							pmpkg_t *localp2 = _alpm_db_get_pkgfromcache(db, p2->name);
							/* Check if it used to exist in a package, but doesn't anymore */
							if(localp2 && !alpm_list_find_str(alpm_pkg_get_files(p2), filestr)
									&& alpm_list_find_str(alpm_pkg_get_files(localp2), filestr)) {
								*skip_list = alpm_list_add(*skip_list, strdup(filestr));
							} else {
								conflicts = add_fileconflict(conflicts, p1->name, filestr);
								break;
							}
						}
					}
				}
			}
			alpm_list_free_inner(tmpfiles, &free);
			alpm_list_free(tmpfiles);
		} else {
			/* no version of package currently installed */
			for(j = p1->files; j; j = j->next) {
				filestr = j->data;

				snprintf(path, PATH_MAX, "%s%s", root, filestr);

				/* stat the file - if it exists and is not a dir, report a conflict */
				if(lstat(path, &buf) == 0 && !S_ISDIR(buf.st_mode)) {
					conflicts = add_fileconflict(conflicts, p1->name, filestr);
				}
			}
		}
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
