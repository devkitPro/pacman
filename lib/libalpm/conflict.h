/*
 *  conflict.h
 *
 *  Copyright (c) 2006-2010 Pacman Development Team <pacman-dev@archlinux.org>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _ALPM_CONFLICT_H
#define _ALPM_CONFLICT_H

#include "alpm.h"
#include "db.h"
#include "package.h"

struct __pmconflict_t {
	char *package1;
	char *package2;
	char *reason;
};

struct __pmfileconflict_t {
	char *target;
	pmfileconflicttype_t type;
	char *file;
	char *ctarget;
};

pmconflict_t *_alpm_conflict_new(const char *package1, const char *package2, const char *reason);
pmconflict_t *_alpm_conflict_dup(const pmconflict_t *conflict);
void _alpm_conflict_free(pmconflict_t *conflict);
int _alpm_conflict_isin(pmconflict_t *needle, alpm_list_t *haystack);
alpm_list_t *_alpm_innerconflicts(alpm_list_t *packages);
alpm_list_t *_alpm_outerconflicts(pmdb_t *db, alpm_list_t *packages);
alpm_list_t *_alpm_db_find_fileconflicts(pmdb_t *db, pmtrans_t *trans,
					 alpm_list_t *upgrade, alpm_list_t *remove);

void _alpm_fileconflict_free(pmfileconflict_t *conflict);

#endif /* _ALPM_CONFLICT_H */

/* vim: set ts=2 sw=2 noet: */
