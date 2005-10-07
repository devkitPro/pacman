/*
 *  db.c
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
