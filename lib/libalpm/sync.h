/*
 *  sync.h
 *
 *  Copyright (c) 2002-2007 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _ALPM_SYNC_H
#define _ALPM_SYNC_H

#include "alpm.h"

/* Sync package */
struct __pmsyncpkg_t {
	pmpkgreason_t newreason;
	pmpkg_t *pkg;
	alpm_list_t *removes;
};

pmsyncpkg_t *_alpm_sync_new(pmpkgreason_t newreason, pmpkg_t *spkg, alpm_list_t *removes);
void _alpm_sync_free(pmsyncpkg_t *data);

int _alpm_sync_sysupgrade(pmtrans_t *trans,
		pmdb_t *db_local, alpm_list_t *dbs_sync, alpm_list_t **syncpkgs);

int _alpm_sync_addtarget(pmtrans_t *trans, pmdb_t *db_local, alpm_list_t *dbs_sync, char *name);
int _alpm_sync_prepare(pmtrans_t *trans, pmdb_t *db_local, alpm_list_t *dbs_sync, alpm_list_t **data);
int _alpm_sync_commit(pmtrans_t *trans, pmdb_t *db_local, alpm_list_t **data);

/* typically trans->packages */
pmsyncpkg_t *_alpm_sync_find(alpm_list_t *syncpkgs, const char* pkgname);

#endif /* _ALPM_SYNC_H */

/* vim: set ts=2 sw=2 noet: */
