/*
 *  sync.h
 * 
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
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
#ifndef _ALPM_SYNC_H
#define _ALPM_SYNC_H

#include "db.h"
#include "package.h"
#include "trans.h"

typedef struct __pmsyncpkg_t {
	unsigned char type;
	pmpkg_t *pkg;
	void *data;
} pmsyncpkg_t;

#define FREESYNC(p) do { if(p) { _alpm_sync_free(p); p = NULL; } } while(0)

pmsyncpkg_t *_alpm_sync_new(int type, pmpkg_t *spkg, void *data);
void _alpm_sync_free(void *data);

PMList *_alpm_sync_load_dbarchive(char *archive);

int _alpm_sync_sysupgrade(pmtrans_t *trans, pmdb_t *db_local, PMList *dbs_sync);
int _alpm_sync_addtarget(pmtrans_t *trans, pmdb_t *db_local, PMList *dbs_sync, char *name);
int _alpm_sync_prepare(pmtrans_t *trans, pmdb_t *db_local, PMList *dbs_sync, PMList **data);
int _alpm_sync_commit(pmtrans_t *trans, pmdb_t *db_local, PMList **data);

#endif /* _ALPM_SYNC_H */

/* vim: set ts=2 sw=2 noet: */
