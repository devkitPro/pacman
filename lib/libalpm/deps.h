/*
 *  deps.h
 *
 *  Copyright (c) 2006-2010 Pacman Development Team <pacman-dev@archlinux.org>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
	char *name;
	char *version;
};

/* Missing dependency */
struct __pmdepmissing_t {
	char *target;
	pmdepend_t *depend;
	char *causingpkg; /* this is used in case of remove dependency error only */
};

void _alpm_dep_free(pmdepend_t *dep);
pmdepend_t *_alpm_dep_dup(const pmdepend_t *dep);
pmdepmissing_t *_alpm_depmiss_new(const char *target, pmdepend_t *dep,
		const char *causinpkg);
void _alpm_depmiss_free(pmdepmissing_t *miss);
alpm_list_t *_alpm_sortbydeps(alpm_list_t *targets, int reverse);
void _alpm_recursedeps(pmdb_t *db, alpm_list_t *targs, int include_explicit);
pmpkg_t *_alpm_resolvedep(pmdepend_t *dep, alpm_list_t *dbs, alpm_list_t *excluding, int prompt);
int _alpm_resolvedeps(alpm_list_t *localpkgs, alpm_list_t *dbs_sync, pmpkg_t *pkg,
		alpm_list_t *preferred, alpm_list_t **packages, alpm_list_t *remove,
		alpm_list_t **data);
int _alpm_dep_edge(pmpkg_t *pkg1, pmpkg_t *pkg2);
pmdepend_t *_alpm_splitdep(const char *depstring);
pmpkg_t *_alpm_find_dep_satisfier(alpm_list_t *pkgs, pmdepend_t *dep);

#endif /* _ALPM_DEPS_H */

/* vim: set ts=2 sw=2 noet: */
