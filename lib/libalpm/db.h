/*
 *  db.h
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
#ifndef _ALPM_DB_H
#define _ALPM_DB_H

#include <dirent.h>

#include "list.h"
#include "package.h"

/* Database entries */
#define INFRQ_NONE     0x00
#define INFRQ_DESC     0x01
#define INFRQ_DEPENDS  0x02
#define INFRQ_FILES    0x04
#define INFRQ_SCRIPLET 0x08
#define INFRQ_ALL      0xFF

#define DB_TREENAME_LEN 128

/* Database */
typedef struct __pmdb_t {
	char *path;
	char treename[DB_TREENAME_LEN];
	DIR *dir;
	PMList *pkgcache;
	PMList *grpcache;
} pmdb_t;

pmdb_t *db_open(char *root, char *dbpath, char *treename);
void db_close(pmdb_t *db);
int db_create(char *root, char *dbpath, char *treename);
int db_update(char *root, char *dbpath, char *treename, char *archive);

void db_rewind(pmdb_t *db);
pmpkg_t *db_scan(pmdb_t *db, char *target, unsigned int inforeq);
int db_read(pmdb_t *db, char *name, unsigned int inforeq, pmpkg_t *info);
int db_write(pmdb_t *db, pmpkg_t *info, unsigned int inforeq);
int db_remove(pmdb_t *db, pmpkg_t *info);

PMList *db_find_conflicts(pmdb_t *db, PMList *targets, char *root);

#endif /* _ALPM_DB_H */

/* vim: set ts=2 sw=2 noet: */
