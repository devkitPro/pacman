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

int db_search(PM_DB *db, char *treename, char *needle)
{
	PM_LIST *lp;
	char *targ;

	targ = strdup(needle);
	strtoupper(targ);

	for(lp = alpm_db_getpkgcache(db); lp; lp = alpm_list_next(lp)) {
		PM_PKG *pkg = alpm_list_getdata(lp);
		char *haystack;
		char *pkgname, *pkgdesc;
		int match = 0;

		pkgname = alpm_pkg_getinfo(pkg, PM_PKG_NAME);
		pkgdesc = alpm_pkg_getinfo(pkg, PM_PKG_DESC);

		/* check name */
		haystack = strdup(pkgname);
		strtoupper(haystack);
		if(strstr(haystack, targ)) {
			match = 1;
		}
		FREE(haystack);

		/* check description */
		if(!match) {
			haystack = strdup(pkgdesc);
			strtoupper(haystack);
			if(strstr(haystack, targ)) {
				match = 1;
			}
			FREE(haystack);
		}

		/* check provides */
		if(!match) {
			PM_LIST *m;

			for(m = alpm_pkg_getinfo(pkg, PM_PKG_PROVIDES); m; m = alpm_list_next(m)) {
				haystack = strdup(alpm_list_getdata(m));
				strtoupper(haystack);
				if(strstr(haystack, targ)) {
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

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
