/*
 *  conflict.c
 * 
 *  Copyright (c) 2002-2005 by Judd Vinet <jvinet@zeroflux.org>
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
#include <sys/stat.h>
/* pacman */
#include "util.h"
#include "conflict.h"

PMList *db_find_conflicts(pmdb_t *db, PMList *targets, char *root)
{
	PMList *i, *j, *k;
	char *filestr = NULL;
	char path[PATH_MAX+1];
	char *str = NULL;
	struct stat buf, buf2;
	PMList *conflicts = NULL;

	if(db == NULL || targets == NULL || root == NULL) {
		return(NULL);
	}

	/* CHECK 1: check every db package against every target package */
	/* XXX: I've disabled the database-against-targets check for now, as the
	 *      many many strcmp() calls slow it down heavily and most of the
	 *      checking is redundant to the targets-against-filesystem check.
	 *      This will be re-enabled if I can improve performance significantly.
	 *
	pmpkg_t *info = NULL;
	char *dbstr   = NULL;
	db_rewind(db);
	while((info = db_scan(db, NULL, INFRQ_DESC | INFRQ_FILES)) != NULL) {
		for(i = info->files; i; i = i->next) {
			if(i->data == NULL) continue;
			dbstr = (char*)i->data;
			for(j = targets; j; j = j->next) {
				pmpkg_t *targ = (pmpkg_t*)j->data;
				if(strcmp(info->name, targ->name)) {
					for(k = targ->files; k; k = k->next) {
						filestr = (char*)k->data;
						if(!strcmp(dbstr, filestr)) {
							if(rindex(k->data, '/') == filestr+strlen(filestr)-1) {
								continue;
							}
							MALLOC(str, 512);
							snprintf(str, 512, "%s: exists in \"%s\" (target) and \"%s\" (installed)", dbstr,
								targ->name, info->name);
							conflicts = pm_list_add(conflicts, str);
						}
					}
				}
			}
		}
	}*/

	/* CHECK 2: check every target against every target */
	for(i = targets; i; i = i->next) {
		pmpkg_t *p1 = (pmpkg_t*)i->data;
		for(j = i; j; j = j->next) {
			pmpkg_t *p2 = (pmpkg_t*)j->data;
			if(strcmp(p1->name, p2->name)) {
				for(k = p1->files; k; k = k->next) {
					filestr = k->data;
					if(!strcmp(filestr, "._install") || !strcmp(filestr, ".INSTALL")) {
						continue;
					}
					if(rindex(filestr, '/') == filestr+strlen(filestr)-1) {
						/* this filename has a trailing '/', so it's a directory -- skip it. */
						continue;
					}
					if(pm_list_is_strin(filestr, p2->files)) {
						MALLOC(str, 512);
						snprintf(str, 512, "%s: exists in \"%s\" (target) and \"%s\" (target)",
							filestr, p1->name, p2->name);
						conflicts = pm_list_add(conflicts, str);
					}
				}
			}
		}
	}

	/* CHECK 3: check every target against the filesystem */
	for(i = targets; i; i = i->next) {
		pmpkg_t *p = (pmpkg_t*)i->data;
		pmpkg_t *dbpkg = NULL;
		for(j = p->files; j; j = j->next) {
			filestr = (char*)j->data;
			snprintf(path, PATH_MAX, "%s%s", root, filestr);
			if(!stat(path, &buf) && !S_ISDIR(buf.st_mode)) {
				int ok = 0;
				if(dbpkg == NULL) {
					dbpkg = db_scan(db, p->name, INFRQ_DESC | INFRQ_FILES);
				}
				if(dbpkg && pm_list_is_strin(j->data, dbpkg->files)) {
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
							dbpkg2 = db_scan(db, p1->name, INFRQ_DESC | INFRQ_FILES);
							/* If it used to exist in there, but doesn't anymore */
							if(dbpkg2 && !pm_list_is_strin(filestr, p1->files) && pm_list_is_strin(filestr, dbpkg2->files)) {
								ok = 1;
							}
							FREEPKG(dbpkg2);
						}
					}
				}
				if(!ok) {
					MALLOC(str, 512);
					snprintf(str, 512, "%s: %s: exists in filesystem", p->name, path);
					conflicts = pm_list_add(conflicts, str);
				}
			}
		}
		FREEPKG(dbpkg);
	}

	return(conflicts);
}

/* vim: set ts=2 sw=2 noet: */
