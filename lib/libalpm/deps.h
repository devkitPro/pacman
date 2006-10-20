/*
 *  deps.h
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
#ifndef _ALPM_DEPS_H
#define _ALPM_DEPS_H

#include "db.h"
#include "sync.h"

typedef struct __pmdepend_t {
	unsigned char mod;
	char name[PKG_NAME_LEN];
	char version[PKG_VERSION_LEN];
} pmdepend_t;

typedef struct __pmdepmissing_t {
	char target[PKG_NAME_LEN];
	unsigned char type;
	pmdepend_t depend;
} pmdepmissing_t;

pmdepmissing_t *_alpm_depmiss_new(const char *target, unsigned char type, unsigned char depmod,
                            const char *depname, const char *depversion);
int _alpm_depmiss_isin(pmdepmissing_t *needle, pmlist_t *haystack);
pmlist_t *_alpm_sortbydeps(pmlist_t *targets, int mode);
pmlist_t *_alpm_checkdeps(pmtrans_t *trans, pmdb_t *db, unsigned char op, pmlist_t *packages);
int _alpm_splitdep(char *depstr, pmdepend_t *depend);
pmlist_t *_alpm_removedeps(pmdb_t *db, pmlist_t *targs);
int _alpm_resolvedeps(pmdb_t *local, pmlist_t *dbs_sync, pmpkg_t *syncpkg, pmlist_t *list,
                pmlist_t *trail, pmtrans_t *trans, pmlist_t **data);

#endif /* _ALPM_DEPS_H */

/* vim: set ts=2 sw=2 noet: */
