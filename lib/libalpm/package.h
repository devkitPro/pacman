/*
 *  package.h
 *
 *  Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
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

#include "config.h" /* ensure off_t is correct length */

#include <sys/types.h> /* off_t */
#include <time.h> /* time_t */

#include "alpm.h"
#include "backup.h"
#include "db.h"
#include "signing.h"

typedef enum _pmpkgfrom_t {
	PKG_FROM_FILE = 1,
	PKG_FROM_LOCALDB,
	PKG_FROM_SYNCDB
} pmpkgfrom_t;

/** Package operations struct. This struct contains function pointers to
 * all methods used to access data in a package to allow for things such
 * as lazy package intialization (such as used by the file backend). Each
 * backend is free to define a stuct containing pointers to a specific
 * implementation of these methods. Some backends may find using the
 * defined default_pkg_ops struct to work just fine for their needs.
 */
struct pkg_operations {
	const char *(*get_filename) (pmpkg_t *);
	const char *(*get_desc) (pmpkg_t *);
	const char *(*get_url) (pmpkg_t *);
	time_t (*get_builddate) (pmpkg_t *);
	time_t (*get_installdate) (pmpkg_t *);
	const char *(*get_packager) (pmpkg_t *);
	const char *(*get_md5sum) (pmpkg_t *);
	const char *(*get_arch) (pmpkg_t *);
	off_t (*get_size) (pmpkg_t *);
	off_t (*get_isize) (pmpkg_t *);
	pmpkgreason_t (*get_reason) (pmpkg_t *);
	int (*has_scriptlet) (pmpkg_t *);

	alpm_list_t *(*get_licenses) (pmpkg_t *);
	alpm_list_t *(*get_groups) (pmpkg_t *);
	alpm_list_t *(*get_depends) (pmpkg_t *);
	alpm_list_t *(*get_optdepends) (pmpkg_t *);
	alpm_list_t *(*get_conflicts) (pmpkg_t *);
	alpm_list_t *(*get_provides) (pmpkg_t *);
	alpm_list_t *(*get_replaces) (pmpkg_t *);
	alpm_list_t *(*get_deltas) (pmpkg_t *);
	alpm_list_t *(*get_files) (pmpkg_t *);
	alpm_list_t *(*get_backup) (pmpkg_t *);

	void *(*changelog_open) (pmpkg_t *);
	size_t (*changelog_read) (void *, size_t, const pmpkg_t *, const void *);
	int (*changelog_close) (const pmpkg_t *, void *);

	/* still to add:
	 * checkmd5sum() ?
	 * compute_requiredby()
	 */
};

/** The standard package operations struct. get fields directly from the
 * struct itself with no abstraction layer or any type of lazy loading.
 * The actual definition is in package.c so it can have access to the
 * default accessor functions which are defined there.
 */
extern struct pkg_operations default_pkg_ops;

struct __pmpkg_t {
	unsigned long name_hash;
	char *filename;
	char *name;
	char *version;
	char *desc;
	char *url;
	char *packager;
	char *md5sum;
	char *base64_sig;
	char *arch;

	time_t builddate;
	time_t installdate;

	off_t size;
	off_t isize;
	off_t download_size;

	int scriptlet;

	pmpkgreason_t reason;
	pmdbinfrq_t infolevel;
	pmpkgfrom_t origin;
	/* origin == PKG_FROM_FILE, use pkg->origin_data.file
	 * origin == PKG_FROM_*DB, use pkg->origin_data.db */
	union {
		pmdb_t *db;
		char *file;
	} origin_data;
	pmhandle_t *handle;

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
	alpm_list_t *removes; /* in transaction targets only */

	struct pkg_operations *ops;
};

pmpkg_t* _alpm_pkg_new(void);
pmpkg_t *_alpm_pkg_dup(pmpkg_t *pkg);
void _alpm_pkg_free(pmpkg_t *pkg);
void _alpm_pkg_free_trans(pmpkg_t *pkg);


pmpkg_t *_alpm_pkg_load_internal(pmhandle_t *handle, const char *pkgfile,
		int full, const char *md5sum, const char *base64_sig,
		pgp_verify_t check_sig);

int _alpm_pkg_cmp(const void *p1, const void *p2);
int _alpm_pkg_compare_versions(pmpkg_t *local_pkg, pmpkg_t *pkg);
pmpkg_t *_alpm_pkg_find(alpm_list_t *haystack, const char *needle);
int _alpm_pkg_should_ignore(pmhandle_t *handle, pmpkg_t *pkg);

#endif /* _ALPM_PACKAGE_H */

/* vim: set ts=2 sw=2 noet: */
