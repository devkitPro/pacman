/*
 *  db.c
 * 
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2005 by Christian Hamar <krics@linuxforum.hu>
 *  Copyright (c) 2006 by David Kimpe <dnaku@frugalware.org>
 *  Copyright (c) 2005, 2006 by Miklos Vajna <vmiklos@frugalware.org>
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
#include <sys/stat.h>
#endif

#include "config.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <libintl.h>
#ifdef CYGWIN
#include <limits.h> /* PATH_MAX */
#endif
/* pacman */
#include "log.h"
#include "util.h"
#include "error.h"
#include "server.h"
#include "db.h"
#include "handle.h"
#include "cache.h"
#include "alpm.h"

extern pmhandle_t *handle;

pmdb_t *_alpm_db_new(char *root, char* dbpath, char *treename)
{
	pmdb_t *db;

	db = (pmdb_t *)malloc(sizeof(pmdb_t));
	if(db == NULL) {
		_alpm_log(PM_LOG_ERROR, _("malloc failed: could not allocate %d bytes"),
				sizeof(pmdb_t));
		RET_ERR(PM_ERR_MEMORY, NULL);
	}

	db->path = (char *)malloc(strlen(root)+strlen(dbpath)+strlen(treename)+2);
	if(db->path == NULL) {
		_alpm_log(PM_LOG_ERROR, _("malloc failed: could not allocate %d bytes"),
				strlen(root)+strlen(dbpath)+strlen(treename)+2);
		FREE(db);
		RET_ERR(PM_ERR_MEMORY, NULL);
	}
	sprintf(db->path, "%s%s/%s", root, dbpath, treename);

	STRNCPY(db->treename, treename, PATH_MAX);

	db->pkgcache = NULL;
	db->grpcache = NULL;
	db->servers = NULL;

	return(db);
}

void _alpm_db_free(void *data)
{
	pmdb_t *db = data;

	FREELISTSERVERS(db->servers);
	free(db->path);
	free(db);

	return;
}

int _alpm_db_cmp(const void *db1, const void *db2)
{
	return(strcmp(((pmdb_t *)db1)->treename, ((pmdb_t *)db2)->treename));
}

PMList *_alpm_db_search(pmdb_t *db, PMList *needles)
{
	PMList *i, *j, *k, *ret = NULL;

	for(i = needles; i; i = i->next) {
		char *targ;
		int retval;

		if(i->data == NULL) {
			continue;
		}
		targ = strdup(i->data);

		for(j = _alpm_db_get_pkgcache(db); j; j = j->next) {
			pmpkg_t *pkg = j->data;
			char *haystack;
			int match = 0;

			/* check name */
			haystack = strdup(pkg->name);
			retval = _alpm_reg_match(haystack, targ);
			if(retval < 0) {
				/* bad regexp */
				FREE(haystack);
				return(NULL);
			} else if(retval) {
				match = 1;
			}
			FREE(haystack);

			/* check description */
			if(!match) {
				haystack = strdup(pkg->desc);
				retval = _alpm_reg_match(haystack, targ);
				if(retval < 0) {
					/* bad regexp */
					FREE(haystack);
					return(NULL);
				} else if(retval) {
					match = 1;
				}
				FREE(haystack);
			}

			/* check provides */
			if(!match) {
				for(k = pkg->provides; k; k = k->next) {
					haystack = strdup(k->data);
					retval = _alpm_reg_match(haystack, targ);
					if(retval < 0) {
						/* bad regexp */
						FREE(haystack);
						return(NULL);
					} else if(retval) {
						match = 1;
					}
					FREE(haystack);
				}
			}

			if(match) {
				ret = _alpm_list_add(ret, pkg);
			}
		}

		FREE(targ);
	}

	return(ret);
}
/* vim: set ts=2 sw=2 noet: */
