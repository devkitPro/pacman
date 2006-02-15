/*
 *  db.c
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include <alpm.h>
/* pacman */
#include "util.h"
#include "list.h"
#include "sync.h"
#include "db.h"

/* reads dbpath/.lastupdate and populates *ts with the contents.
 * *ts should be malloc'ed and should be at least 15 bytes.
 *
 * Returns 0 on success, 1 on error
 *
 */
int db_getlastupdate(PM_DB *db, char *ts)
{
	FILE *fp;
	char *root, *dbpath, *treename;
	char file[PATH_MAX];

	if(db == NULL || ts == NULL) {
		return(-1);
	}

	alpm_get_option(PM_OPT_ROOT, (long *)&root);
	alpm_get_option(PM_OPT_DBPATH, (long *)&dbpath);
	treename = alpm_db_getinfo(db, PM_DB_TREENAME);
	snprintf(file, PATH_MAX, "%s%s/%s/.lastupdate", root, dbpath, treename);

	/* get the last update time, if it's there */
	if((fp = fopen(file, "r")) == NULL) {
		return(-1);
	} else {
		char line[256];
		if(fgets(line, sizeof(line), fp)) {
			STRNCPY(ts, line, 15); /* YYYYMMDDHHMMSS */
			ts[14] = '\0';
		} else {
			fclose(fp);
			return(-1);
		}
	}
	fclose(fp);
	return(0);
}

/* writes the dbpath/.lastupdate with the contents of *ts
 */
int db_setlastupdate(PM_DB *db, char *ts)
{
	FILE *fp;
	char *root, *dbpath, *treename;
	char file[PATH_MAX];

	if(db == NULL || ts == NULL || strlen(ts) == 0) {
		return(-1);
	}

	alpm_get_option(PM_OPT_ROOT, (long *)&root);
	alpm_get_option(PM_OPT_DBPATH, (long *)&dbpath);
	treename = alpm_db_getinfo(db, PM_DB_TREENAME);
	snprintf(file, PATH_MAX, "%s%s/%s/.lastupdate", root, dbpath, treename);

	if((fp = fopen(file, "w")) == NULL) {
		return(-1);
	}
	if(fputs(ts, fp) <= 0) {
		fclose(fp);
		return(-1);
	}
	fclose(fp);

	return(0);
}

int db_search(PM_DB *db, const char *treename, list_t *needles)
{
	list_t *i;

	if(needles == NULL || needles->data == NULL) {
		return(0);
	}
	
	for(i = needles; i; i = i->next) {
		PM_LIST *j;
		char *targ;
		int ret;

		if(i->data == NULL) {
			continue;
		}
		targ = strdup(i->data);

		for(j = alpm_db_getpkgcache(db); j; j = alpm_list_next(j)) {
			PM_PKG *pkg = alpm_list_getdata(j);
			char *haystack;
			char *pkgname, *pkgdesc;
			int match = 0;

			pkgname = alpm_pkg_getinfo(pkg, PM_PKG_NAME);
			pkgdesc = alpm_pkg_getinfo(pkg, PM_PKG_DESC);

			/* check name */
			haystack = strdup(pkgname);
			ret = reg_match(haystack, targ);
			if(ret < 0) {
				/* bad regexp */
				FREE(haystack);
				return(1);
			} else if(ret) {
				match = 1;
			}
			FREE(haystack);

			/* check description */
			if(!match) {
				haystack = strdup(pkgdesc);
				ret = reg_match(haystack, targ);
				if(ret < 0) {
					/* bad regexp */
					FREE(haystack);
					return(1);
				} else if(ret) {
					match = 1;
				}
				FREE(haystack);
			}

			/* check provides */
			if(!match) {
				PM_LIST *m;

				for(m = alpm_pkg_getinfo(pkg, PM_PKG_PROVIDES); m; m = alpm_list_next(m)) {
					haystack = strdup(alpm_list_getdata(m));
					ret = reg_match(haystack, targ);
					if(ret < 0) {
						/* bad regexp */
						FREE(haystack);
						return(1);
					} else if(ret) {
						match = 1;
					}
					FREE(haystack);
				}
			}

			if(match) {
				printf("%s/%s %s\n    ", treename, pkgname, (char *)alpm_pkg_getinfo(pkg, PM_PKG_VERSION));
				indentprint(pkgdesc, 4);
				printf("\n");
			}
		}

		FREE(targ);
	}

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
