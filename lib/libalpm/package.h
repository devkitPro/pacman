/*
 *  package.h
 * 
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
 *  USA.
 */
#ifndef _ALPM_PACKAGE_H
#define _ALPM_PACKAGE_H

#if defined(__APPLE__) || defined(__sun__)
#include <time.h>
#endif

#include "alpm.h"
#include "db.h"

typedef enum _pmpkgfrom_t {
	PKG_FROM_CACHE = 1,
	PKG_FROM_FILE
} pmpkgfrom_t;

/* Packages */
#define PKG_FILENAME_LEN 512
#define PKG_NAME_LEN     256
#define PKG_VERSION_LEN  64
#define PKG_FULLNAME_LEN (PKG_NAME_LEN-1)+1+(PKG_VERSION_LEN-1)+1
#define PKG_DESC_LEN     512
#define PKG_URL_LEN      256
#define PKG_DATE_LEN     32
#define PKG_TYPE_LEN     32
#define PKG_PACKAGER_LEN 64
#define PKG_MD5SUM_LEN   33
#define PKG_SHA1SUM_LEN  41
#define PKG_ARCH_LEN     32

struct __pmpkg_t {
	char filename[PKG_FILENAME_LEN];
	char name[PKG_NAME_LEN];
	char version[PKG_VERSION_LEN];
	char desc[PKG_DESC_LEN];
	char url[PKG_URL_LEN];
	char builddate[PKG_DATE_LEN];
	char buildtype[PKG_TYPE_LEN];
	char installdate[PKG_DATE_LEN];
	char packager[PKG_PACKAGER_LEN];
	char md5sum[PKG_MD5SUM_LEN];
	char sha1sum[PKG_SHA1SUM_LEN];
	char arch[PKG_ARCH_LEN];
	unsigned long size;
	unsigned long isize;
	unsigned short scriptlet;
	unsigned short force;
	time_t date;
	pmpkgreason_t reason;
	alpm_list_t *desc_localized;
	alpm_list_t *license;
	alpm_list_t *replaces;
	alpm_list_t *groups;
	alpm_list_t *files;
	alpm_list_t *backup;
	alpm_list_t *depends;
	alpm_list_t *removes;
	alpm_list_t *requiredby;
	alpm_list_t *conflicts;
	alpm_list_t *provides;
	/* internal */
	unsigned short origin;
	void *data;
	pmdbinfrq_t infolevel;
};

#define FREEPKG(p) do { if(p){_alpm_pkg_free(p); p = NULL;}} while(0)
#define FREELISTPKGS(p) _FREELIST(p, _alpm_pkg_free)

pmpkg_t* _alpm_pkg_new(const char *name, const char *version);
pmpkg_t *_alpm_pkg_dup(pmpkg_t *pkg);
void _alpm_pkg_free(void *data);
int _alpm_pkg_cmp(const void *p1, const void *p2);
pmpkg_t *_alpm_pkg_load(char *pkgfile);
pmpkg_t *_alpm_pkg_isin(char *needle, alpm_list_t *haystack);
int _alpm_pkg_splitname(char *target, char *name, char *version, int witharch);


#endif /* _ALPM_PACKAGE_H */

/* vim: set ts=2 sw=2 noet: */
