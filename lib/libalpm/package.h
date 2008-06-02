/*
 *  package.h
 *
 *  Copyright (c) 2002-2007 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2006 by David Kimpe <dnaku@frugalware.org>
 *  Copyright (c) 2005, 2006 by Christian Hamar <krics@linuxforum.hu>
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
#ifndef _ALPM_PACKAGE_H
#define _ALPM_PACKAGE_H

#include <sys/types.h> /* off_t */
#include <time.h> /* time_t */

#include "alpm.h"
#include "db.h"

typedef enum _pmpkgfrom_t {
	PKG_FROM_CACHE = 1,
	PKG_FROM_FILE
} pmpkgfrom_t;

struct __pmpkg_t {
	char *filename;
	char *name;
	char *version;
	char *desc;
	char *url;
	time_t builddate;
	time_t installdate;
	char *packager;
	char *md5sum;
	char *arch;
	off_t size;
	off_t isize;
	off_t download_size;
	unsigned short scriptlet;
	unsigned short force;
	pmpkgreason_t reason;
	alpm_list_t *licenses;
	alpm_list_t *replaces;
	alpm_list_t *groups;
	alpm_list_t *files;
	alpm_list_t *backup;
	alpm_list_t *depends;
	alpm_list_t *optdepends;
	alpm_list_t *conflicts;
	alpm_list_t *provides;
	alpm_list_t *deltas;
	alpm_list_t *delta_path;
	/* internal */
	pmpkgfrom_t origin;
	/* Replaced 'void *data' with this union as follows:
  origin == PKG_FROM_CACHE, use pkg->origin_data.db
  origin == PKG_FROM_FILE, use pkg->origin_data.file
	*/
  union {
		pmdb_t *db;
		char *file;
	} origin_data;
	pmdbinfrq_t infolevel;
};

pmpkg_t* _alpm_pkg_new(void);
pmpkg_t *_alpm_pkg_dup(pmpkg_t *pkg);
void _alpm_pkg_free(pmpkg_t *pkg);
int _alpm_pkg_cmp(const void *p1, const void *p2);
int _alpm_pkg_compare_versions(pmpkg_t *local_pkg, pmpkg_t *pkg);
pmpkg_t *_alpm_pkg_find(alpm_list_t *haystack, const char *needle);
int _alpm_pkg_should_ignore(pmpkg_t *pkg);

#endif /* _ALPM_PACKAGE_H */

/* vim: set ts=2 sw=2 noet: */
