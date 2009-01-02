/*
 *  db.h
 *
 *  Copyright (c) 2002-2007 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2006 by Miklos Vajna <vmiklos@frugalware.org>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _ALPM_DB_H
#define _ALPM_DB_H

#include "alpm.h"
#include <limits.h>
#include <time.h>

/* Database entries */
typedef enum _pmdbinfrq_t {
	INFRQ_BASE = 0x01,
	INFRQ_DESC = 0x02,
	INFRQ_DEPENDS = 0x04,
	INFRQ_FILES = 0x08,
	INFRQ_SCRIPTLET = 0x10,
	INFRQ_DELTAS = 0x20,
	/* ALL should be sum of all above */
	INFRQ_ALL = 0x3F
} pmdbinfrq_t;

/* Database */
struct __pmdb_t {
	char *path;
	char *treename;
	void *handle;
	alpm_list_t *pkgcache;
	alpm_list_t *grpcache;
	alpm_list_t *servers;
};

/* db.c, database general calls */
pmdb_t *_alpm_db_new(const char *dbpath, const char *treename);
void _alpm_db_free(pmdb_t *db);
int _alpm_db_cmp(const void *d1, const void *d2);
alpm_list_t *_alpm_db_search(pmdb_t *db, const alpm_list_t *needles);
pmdb_t *_alpm_db_register_local(void);
pmdb_t *_alpm_db_register_sync(const char *treename);

/* be.c, backend specific calls */
int _alpm_db_open(pmdb_t *db);
void _alpm_db_close(pmdb_t *db);
int _alpm_db_populate(pmdb_t *db);
int _alpm_db_read(pmdb_t *db, pmpkg_t *info, pmdbinfrq_t inforeq);
int _alpm_db_prepare(pmdb_t *db, pmpkg_t *info);
int _alpm_db_write(pmdb_t *db, pmpkg_t *info, pmdbinfrq_t inforeq);
int _alpm_db_remove(pmdb_t *db, pmpkg_t *info);

#endif /* _ALPM_DB_H */

/* vim: set ts=2 sw=2 noet: */
