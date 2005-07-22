/*
 *  package.h
 * 
 *  Copyright (c) 2002-2005 by Judd Vinet <jvinet@zeroflux.org>
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

#include "list.h"

enum {
	PKG_FROM_CACHE = 1,
	PKG_FROM_FILE
};

#define PKG_NAME_LEN     256
#define PKG_VERSION_LEN  64
#define PKG_DESC_LEN     512
#define PKG_URL_LEN      256
#define PKG_DATE_LEN     32
#define PKG_PACKAGER_LEN 64
#define PKG_MD5SUM_LEN   33
#define PKG_ARCH_LEN     32

typedef struct __pmpkg_t {
	char name[PKG_NAME_LEN];
	char version[PKG_VERSION_LEN];
	char desc[PKG_DESC_LEN];
	char url[PKG_URL_LEN];
	char builddate[PKG_DATE_LEN];
	char installdate[PKG_DATE_LEN];
	char packager[PKG_PACKAGER_LEN];
	char md5sum[PKG_MD5SUM_LEN];
	char arch[PKG_ARCH_LEN];
	unsigned long size;
	unsigned char scriptlet;
	unsigned char force;
	unsigned char reason;
	PMList *license;
	PMList *replaces;
	PMList *groups;
	PMList *files;
	PMList *backup;
	PMList *depends;
	PMList *requiredby;
	PMList *conflicts;
	PMList *provides;
	/* internal */
	unsigned char origin;
	void *data;
	unsigned char infolevel;
} pmpkg_t;

#define FREEPKG(p) \
do { \
	if(p) { \
		pkg_free(p); \
		p = NULL; \
	} \
} while(0)

#define FREELISTPKGS(p) \
do { \
	if(p) { \
		PMList *i; \
		for(i = p; i; i = i->next) { \
			FREEPKG(i->data); \
		}\
		FREELIST(p);\
	} \
} while(0)

pmpkg_t* pkg_new();
pmpkg_t *pkg_dup(pmpkg_t *pkg);
void pkg_free(pmpkg_t *pkg);
pmpkg_t *pkg_load(char *pkgfile);
int pkg_isin(pmpkg_t *needle, PMList *haystack);
int pkg_splitname(char *target, char *name, char *version);

#endif /* _ALPM_PACKAGE_H */

/* vim: set ts=2 sw=2 noet: */
