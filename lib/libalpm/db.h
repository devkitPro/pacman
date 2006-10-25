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

#include <limits.h>
#include "package.h"
#include "alpm.h"

/* Database entries */
#define INFRQ_NONE     0x00
#define INFRQ_DESC     0x01
#define INFRQ_DEPENDS  0x02
#define INFRQ_FILES    0x04
#define INFRQ_SCRIPLET 0x08
#define INFRQ_ALL      0xFF

#define DB_O_CREATE 0x01

/* Database */
typedef struct __pmdb_t {
	char *path;
	char treename[PATH_MAX];
	void *handle;
	pmlist_t *pkgcache;
	pmlist_t *grpcache;
	pmlist_t *servers;
} pmdb_t;

pmdb_t *_alpm_db_new(char *root, char *dbpath, char *treename);
void _alpm_db_free(void *data);
int _alpm_db_cmp(const void *db1, const void *db2);
pmlist_t *_alpm_db_search(pmdb_t *db, pmlist_t *needles);

/* Prototypes for backends functions */
int _alpm_db_install(pmdb_t *db, const char *dbfile);
int _alpm_db_open(pmdb_t *db);
void _alpm_db_close(pmdb_t *db);
void _alpm_db_rewind(pmdb_t *db);
pmpkg_t *_alpm_db_scan(pmdb_t *db, char *target, unsigned int inforeq);
int _alpm_db_read(pmdb_t *db, unsigned int inforeq, pmpkg_t *info);
int _alpm_db_write(pmdb_t *db, pmpkg_t *info, unsigned int inforeq);
int _alpm_db_remove(pmdb_t *db, pmpkg_t *info);
int _alpm_db_getlastupdate(pmdb_t *db, char *ts);
int _alpm_db_setlastupdate(pmdb_t *db, char *ts);
pmdb_t *_alpm_db_register(char *treename, alpm_cb_db_register callback);

#endif /* _ALPM_DB_H */

/* vim: set ts=2 sw=2 noet: */
