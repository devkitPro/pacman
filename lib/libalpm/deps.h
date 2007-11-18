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
#include "package.h"
#include "alpm.h"

/* Dependency */
struct __pmdepend_t {
	pmdepmod_t mod;
	char name[PKG_NAME_LEN];
	char version[PKG_VERSION_LEN];
};

/* Missing dependency */
struct __pmdepmissing_t {
	char target[PKG_NAME_LEN];
	pmdepend_t depend;
};

/* Graphs */
struct __pmgraph_t {
	int state; /* 0: untouched, -1: entered, other: leaving time */
	void *data;
	struct __pmgraph_t *parent; /* where did we come from? */
	alpm_list_t *children;
	alpm_list_t *childptr; /* points to a child in children list */
};

pmdepmissing_t *_alpm_depmiss_new(const char *target, pmdepmod_t depmod,
		const char *depname, const char *depversion);
int _alpm_depmiss_isin(pmdepmissing_t *needle, alpm_list_t *haystack);
alpm_list_t *_alpm_sortbydeps(alpm_list_t *targets, pmtranstype_t mode);
alpm_list_t *_alpm_checkdeps(pmdb_t *db, pmtranstype_t op,
                             alpm_list_t *packages);
void _alpm_recursedeps(pmdb_t *db, alpm_list_t *targs, int include_explicit);
int _alpm_resolvedeps(pmdb_t *local, alpm_list_t *dbs_sync, pmpkg_t *syncpkg,
                      alpm_list_t **list, pmtrans_t *trans, alpm_list_t **data);

#endif /* _ALPM_DEPS_H */

/* vim: set ts=2 sw=2 noet: */
