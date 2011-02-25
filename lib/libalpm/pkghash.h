/*
 *  pkghash.h
 *
 *  Copyright (c) 2011 Pacman Development Team <pacman-dev@archlinux.org>
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

#ifndef _ALPM_PKGHASH_H
#define _ALPM_PKGHASH_H

#include <stdlib.h>

#include "alpm.h"
#include "alpm_list.h"


/**
 * @brief A hash table for holding pmpkg_t objects.
 *
 * A combination of a hash table and a list, allowing for fast look-up
 * by package name but also iteration over the packages.
 */
struct __pmpkghash_t {
	/** data held by the hash table */
	alpm_list_t **hash_table;
	/** number of buckets in hash table */
	size_t buckets;
	/** number of entries in hash table */
	size_t entries;
	/** head node of the hash table data in normal list format */
	alpm_list_t *list;
};

typedef struct __pmpkghash_t pmpkghash_t;

pmpkghash_t *_alpm_pkghash_create(size_t size);

pmpkghash_t *_alpm_pkghash_add(pmpkghash_t *hash, pmpkg_t *pkg);
pmpkghash_t *_alpm_pkghash_add_sorted(pmpkghash_t *hash, pmpkg_t *pkg);
pmpkghash_t *_alpm_pkghash_remove(pmpkghash_t *hash, pmpkg_t *pkg, pmpkg_t **data);

void _alpm_pkghash_free(pmpkghash_t *hash);

pmpkg_t *_alpm_pkghash_find(pmpkghash_t *hash, const char *name);

#define MAX_HASH_LOAD 0.7

#endif /* _ALPM_PKGHASH_H */
