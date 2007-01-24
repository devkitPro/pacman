/*
 *  db.h
 * 
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
 *  USA.
 */
#ifndef _ALPM_DB_H
#define _ALPM_DB_H

#include "alpm.h"
#include <limits.h>

/* Database entries */
typedef enum _pmdbinfrq_t {
	INFRQ_NONE = 0x00,
	INFRQ_DESC = 0x01,
	INFRQ_DEPENDS = 0x02,
	INFRQ_FILES = 0x04,
	INFRQ_SCRIPTLET = 0x08,
	INFRQ_ALL = 0xFF
} pmdbinfrq_t;

/* Database */
struct __pmdb_t {
	char *path;
	char treename[PATH_MAX];
	void *handle;
	alpm_list_t *pkgcache;
	alpm_list_t *grpcache;
	alpm_list_t *servers;
};
/* db.c, database general calls */
pmdb_t *_alpm_db_new(char *root, char *dbpath, char *treename);
void _alpm_db_free(void *data);
int _alpm_db_cmp(const void *db1, const void *db2);
alpm_list_t *_alpm_db_search(pmdb_t *db, alpm_list_t *needles);
pmdb_t *_alpm_db_register(char *treename, alpm_cb_db_register callback);

/* be.c, backend specific calls */
int _alpm_db_install(pmdb_t *db, const char *dbfile);
int _alpm_db_open(pmdb_t *db);
void _alpm_db_close(pmdb_t *db);
void _alpm_db_rewind(pmdb_t *db);
pmpkg_t *_alpm_db_scan(pmdb_t *db, char *target, pmdbinfrq_t inforeq);
int _alpm_db_read(pmdb_t *db, pmdbinfrq_t inforeq, pmpkg_t *info);
int _alpm_db_write(pmdb_t *db, pmpkg_t *info, pmdbinfrq_t inforeq);
int _alpm_db_remove(pmdb_t *db, pmpkg_t *info);
int _alpm_db_getlastupdate(pmdb_t *db, char *ts);
int _alpm_db_setlastupdate(pmdb_t *db, char *ts);

#endif /* _ALPM_DB_H */

/* vim: set ts=2 sw=2 noet: */
