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
#include <regex.h>
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

pmdb_t *_alpm_db_new(const char *root, const char *dbpath, const char *treename)
{
	pmdb_t *db;

	ALPM_LOG_FUNC;

	db = calloc(1, sizeof(pmdb_t));
	if(db == NULL) {
		_alpm_log(PM_LOG_ERROR, _("malloc failed: could not allocate %d bytes"),
				  sizeof(pmdb_t));
		RET_ERR(PM_ERR_MEMORY, NULL);
	}

	db->path = calloc(1, strlen(root)+strlen(dbpath)+strlen(treename)+2);
	if(db->path == NULL) {
		_alpm_log(PM_LOG_ERROR, _("malloc failed: could not allocate %d bytes"),
				  strlen(root)+strlen(dbpath)+strlen(treename)+2);
		FREE(db);
		RET_ERR(PM_ERR_MEMORY, NULL);
	}
	sprintf(db->path, "%s%s%s", root, dbpath, treename);

	STRNCPY(db->treename, treename, PATH_MAX);

	return(db);
}

void _alpm_db_free(void *data)
{
	pmdb_t *db = data;

	ALPM_LOG_FUNC;

	FREELISTSERVERS(db->servers);
	FREE(db->path);
	FREE(db);

	return;
}

int _alpm_db_cmp(const void *db1, const void *db2)
{
	ALPM_LOG_FUNC;
	return(strcmp(((pmdb_t *)db1)->treename, ((pmdb_t *)db2)->treename));
}

alpm_list_t *_alpm_db_search(pmdb_t *db, alpm_list_t *needles)
{
	alpm_list_t *i, *j, *k, *ret = NULL;

	ALPM_LOG_FUNC;

	for(i = needles; i; i = i->next) {
		char *targ;
		regex_t reg;

		if(i->data == NULL) {
			continue;
		}
		targ = i->data;
		_alpm_log(PM_LOG_DEBUG, "searching for target '%s'", targ);
		
		if(regcomp(&reg, targ, REG_EXTENDED | REG_NOSUB | REG_ICASE | REG_NEWLINE) != 0) {
			RET_ERR(PM_ERR_INVALID_REGEX, NULL);
		}

		for(j = _alpm_db_get_pkgcache(db, INFRQ_DESC|INFRQ_DEPENDS); j; j = j->next) {
			pmpkg_t *pkg = j->data;
			char *matched = NULL;

			/* check name */
			if (regexec(&reg, pkg->name, 0, 0, 0) == 0) {
				matched = pkg->name;
			}
			/* check desc */
			else if (regexec(&reg, pkg->desc, 0, 0, 0) == 0) {
				matched = pkg->desc;
			}
			/* check provides */
			/* TODO: should we be doing this, and should we print something
			 * differently when we do match it since it isn't currently printed? */
			else {
				for(k = pkg->provides; k; k = k->next) {
					if (regexec(&reg, k->data, 0, 0, 0) == 0) {
						matched = k->data;
						break;
					}
				}
			}

			if(matched != NULL) {
				_alpm_log(PM_LOG_DEBUG, "    search target '%s' matched '%s'",
				          targ, matched);
				ret = alpm_list_add(ret, pkg);
			}
		}

		regfree(&reg);
	}

	return(ret);
}

pmdb_t *_alpm_db_register(const char *treename, alpm_cb_db_register callback)
{
	struct stat buf;
	pmdb_t *db;
	char path[PATH_MAX];

	ALPM_LOG_FUNC;

	if(strcmp(treename, "local") == 0) {
		if(handle->db_local != NULL) {
			_alpm_log(PM_LOG_WARNING, _("attempt to re-register the 'local' DB"));
			RET_ERR(PM_ERR_DB_NOT_NULL, NULL);
		}
	} else {
		alpm_list_t *i;
		for(i = handle->dbs_sync; i; i = i->next) {
			pmdb_t *sdb = i->data;
			if(strcmp(treename, sdb->treename) == 0) {
				_alpm_log(PM_LOG_DEBUG, _("attempt to re-register the '%s' database, using existing"), sdb->treename);
				return sdb;
			}
		}
	}
	
	_alpm_log(PM_LOG_DEBUG, _("registering database '%s'"), treename);

	/* make sure the database directory exists */
	snprintf(path, PATH_MAX, "%s%s/%s", handle->root, handle->dbpath, treename);
	if(stat(path, &buf) != 0 || !S_ISDIR(buf.st_mode)) {
		_alpm_log(PM_LOG_DEBUG, _("database directory '%s' does not exist -- try creating it"), path);
		if(_alpm_makepath(path) != 0) {
			RET_ERR(PM_ERR_SYSTEM, NULL);
		}
	}

	db = _alpm_db_new(handle->root, handle->dbpath, treename);
	if(db == NULL) {
		RET_ERR(PM_ERR_DB_CREATE, NULL);
	}

	_alpm_log(PM_LOG_DEBUG, _("opening database '%s'"), db->treename);
	if(_alpm_db_open(db) == -1) {
		_alpm_db_free(db);
		RET_ERR(PM_ERR_DB_OPEN, NULL);
	}

	/* Only call callback on NEW registration. */
	if(callback) callback(treename, db);

	if(strcmp(treename, "local") == 0) {
		handle->db_local = db;
	} else {
		handle->dbs_sync = alpm_list_add(handle->dbs_sync, db);
	}

	return(db);
}

const char SYMEXPORT *alpm_db_get_name(pmdb_t *db)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));

	return db->treename;
}

const char *alpm_db_get_url(pmdb_t *db)
{
	char path[PATH_MAX];
	pmserver_t *s;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));

	s = (pmserver_t*)db->servers->data;

	snprintf(path, PATH_MAX, "%s://%s%s", s->s_url->scheme, s->s_url->host, s->s_url->doc);
	return strdup(path);
}

/* vim: set ts=2 sw=2 noet: */
