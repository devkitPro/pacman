/*
 *  cache.h
 *
 *  Copyright (c) 2002-2007 by Judd Vinet <jvinet@zeroflux.org>
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
#ifndef _ALPM_CACHE_H
#define _ALPM_CACHE_H

#include "db.h"
#include "alpm_list.h"
#include "group.h"
#include "package.h"

/* packages */
int _alpm_db_load_pkgcache(pmdb_t *db);
void _alpm_db_free_pkgcache(pmdb_t *db);
int _alpm_db_add_pkgincache(pmdb_t *db, pmpkg_t *pkg);
int _alpm_db_remove_pkgfromcache(pmdb_t *db, pmpkg_t *pkg);
alpm_list_t *_alpm_db_get_pkgcache(pmdb_t *db);
int _alpm_db_ensure_pkgcache(pmdb_t *db, pmdbinfrq_t infolevel);
pmpkg_t *_alpm_db_get_pkgfromcache(pmdb_t *db, const char *target);
/* groups */
int _alpm_db_load_grpcache(pmdb_t *db);
void _alpm_db_free_grpcache(pmdb_t *db);
alpm_list_t *_alpm_db_get_grpcache(pmdb_t *db);
pmgrp_t *_alpm_db_get_grpfromcache(pmdb_t *db, const char *target);

#endif /* _ALPM_CACHE_H */

/* vim: set ts=2 sw=2 noet: */
