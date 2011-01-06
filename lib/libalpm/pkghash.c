/*
 *  pkghash.c
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

#include "pkghash.h"
#include "util.h"
#include "log.h"

/* List of primes for possible sizes of hash tables.
 *
 * The maximum table size is the last prime under 1,000,000.  That is
 * more than an order of magnitude greater than the number of packages
 * in any Linux distribution.
 */
static const size_t prime_list[] =
{
	11ul, 13ul, 17ul, 19ul, 23ul, 29ul, 31ul, 37ul, 41ul, 43ul, 47ul,
	53ul, 59ul, 61ul, 67ul, 71ul, 73ul, 79ul, 83ul, 89ul, 97ul, 103ul,
	109ul, 113ul, 127ul, 137ul, 139ul, 149ul, 157ul, 167ul, 179ul, 193ul,
	199ul, 211ul, 227ul, 241ul, 257ul, 277ul, 293ul, 313ul, 337ul, 359ul,
	383ul, 409ul, 439ul, 467ul, 503ul, 541ul, 577ul, 619ul, 661ul, 709ul,
	761ul, 823ul, 887ul, 953ul, 1031ul, 1109ul, 1193ul, 1289ul, 1381ul,
	1493ul, 1613ul, 1741ul, 1879ul, 2029ul, 2179ul, 2357ul, 2549ul,
	2753ul, 2971ul, 3209ul, 3469ul, 3739ul, 4027ul, 4349ul, 4703ul,
	5087ul, 5503ul, 5953ul, 6427ul, 6949ul, 7517ul, 8123ul, 8783ul,
	9497ul, 10273ul, 11113ul, 12011ul, 12983ul, 14033ul, 15173ul,
	16411ul, 17749ul, 19183ul, 20753ul, 22447ul, 24281ul, 26267ul,
	28411ul, 30727ul, 33223ul, 35933ul, 38873ul, 42043ul, 45481ul,
	49201ul, 53201ul, 57557ul, 62233ul, 67307ul, 72817ul, 78779ul,
	85229ul, 92203ul, 99733ul, 107897ul, 116731ul, 126271ul, 136607ul,
	147793ul, 159871ul, 172933ul, 187091ul, 202409ul, 218971ul, 236897ul,
	256279ul, 277261ul, 299951ul, 324503ul, 351061ul, 379787ul, 410857ul,
	444487ul, 480881ul, 520241ul, 562841ul, 608903ul, 658753ul, 712697ul,
	771049ul, 834181ul, 902483ul, 976369ul
};

/* Allocate a hash table with at least "size" buckets */
pmpkghash_t *_alpm_pkghash_create(size_t size)
{
	pmpkghash_t *hash = NULL;
	size_t i, loopsize;

	MALLOC(hash, sizeof(pmpkghash_t), RET_ERR(PM_ERR_MEMORY, NULL));

	hash->list = NULL;
	hash->entries = 0;

	loopsize = sizeof(prime_list) / sizeof(*prime_list);
	for(i = 0; i < loopsize; i++) {
		if(prime_list[i] > size) {
			hash->buckets = prime_list[i];
			break;
		}
	}

	CALLOC(hash->hash_table, hash->buckets, sizeof(alpm_list_t*), \
				free(hash); RET_ERR(PM_ERR_MEMORY, NULL));

	return(hash);
}

/* Expand the hash table size to the next increment and rebin the entries */
static pmpkghash_t *rehash(pmpkghash_t *oldhash)
{
	//pmpkghash_t *newhash;

	/* TODO - calculate size of newhash */
	/* Hash tables will need resized in two cases:
	 *  - adding packages to the local database
	 *  - poor estimation of the number of packages in sync database
	 *
	 * For small hash tables sizes (<500) the increase in size is by a
	 * minimum of a factor of 2 for optimal rehash efficiency.  For
	 * larger database sizes, this increase is reduced to avoid excess
	 * memory allocation as both scenarios requiring a rehash should not
	 * require a table size increase that large. */

	//newhash = _alpm_pkghash_create(oldhash->buckets);

	/* TODO - rebin entries */

	//_alpm_pkghash_free(oldhash);

	//return newhash;

	printf("rehash needed!!!\n");
	return(oldhash);
}

pmpkghash_t *_alpm_pkghash_add(pmpkghash_t *hash, pmpkg_t *pkg)
{
	alpm_list_t *ptr;
	size_t position;

	if(pkg == NULL || hash == NULL) {
		return(hash);
	}

	if((hash->entries + 1) / MAX_HASH_LOAD > hash->buckets) {
		hash = rehash(hash);
	}

	position = pkg->name_hash % hash->buckets;

	/* collision resolution using open addressing with linear probing */
	while((ptr = hash->hash_table[position]) != NULL) {
		position = (position + 1) % hash->buckets;
	}

	ptr = calloc(1, sizeof(alpm_list_t));
	if(ptr == NULL) {
		return(hash);
	}

	ptr->data = pkg;
	ptr->next = NULL;
	ptr->prev = ptr;

	hash->hash_table[position] = ptr;
	hash->list = alpm_list_join(hash->list, ptr);
	hash->entries += 1;

	return(hash);
}

pmpkghash_t *_alpm_pkghash_add_sorted(pmpkghash_t *hash, pmpkg_t *pkg)
{
	if(!hash) {
		return(_alpm_pkghash_add(hash, pkg));
	}

	alpm_list_t *ptr;
	size_t position;

	if((hash->entries + 1) / MAX_HASH_LOAD > hash->buckets) {
		hash = rehash(hash);
	}

	position = pkg->name_hash % hash->buckets;

	/* collision resolution using open addressing with linear probing */
	while((ptr = hash->hash_table[position]) != NULL) {
		position = (position + 1) % hash->buckets;
	}

	ptr = calloc(1, sizeof(alpm_list_t));
	if(ptr == NULL) {
		return(hash);
	}

	ptr->data = pkg;
	ptr->next = NULL;
	ptr->prev = ptr;

	hash->hash_table[position] = ptr;
	hash->list = alpm_list_mmerge(hash->list, ptr, _alpm_pkg_cmp);
	hash->entries += 1;

	return(hash);
}

/**
 * @brief Remove a package from a pkghash.
 *
 * @param hash     the hash to remove the package from
 * @param pkg      the package we are removing
 * @param data     output parameter containing the removed item
 *
 * @return the resultant hash
 */
pmpkghash_t *_alpm_pkghash_remove(pmpkghash_t *hash, pmpkg_t *pkg, pmpkg_t *data)
{
	alpm_list_t *i;
	size_t position;

	if(pkg == NULL || hash == NULL) {
		return(hash);
	}

	position = pkg->name_hash % hash->buckets;
	while((i = hash->hash_table[position]) != NULL) {
		pmpkg_t *info = i->data;

		if(info->name_hash == pkg->name_hash &&
					strcmp(info->name, pkg->name) == 0) {

#if 0  /* Actually removing items is not a good idea with linear probing...  */
			/* remove from list */
			/* TODO - refactor with alpm_list_remove */
			if(i == hash->list) {
				/* Special case: removing the head node which has a back reference to
				 * the tail node */
				hash->list = i->next;
				if(hash->list) {
					hash->list->prev = i->prev;
				}
				i->prev = NULL;
			} else if(i == hash->list->prev) {
				/* Special case: removing the tail node, so we need to fix the back
				 * reference on the head node. We also know tail != head. */
				if(i->prev) {
					/* i->next should always be null */
					i->prev->next = i->next;
					hash->list->prev = i->prev;
					i->prev = NULL;
				}
			} else {
				/* Normal case, non-head and non-tail node */
				if(i->next) {
					i->next->prev = i->prev;
				}
				if(i->prev) {
					i->prev->next = i->next;
				}
			}

			/* remove from hash */
			data = info;
			info = NULL;
			free(i);
			i = NULL;

			hash->entries -= 1;
#endif

			/* fake removal - pkgname can not be blank so 0 hash is impossible */
			info->name_hash = 0;

			return(hash);
		}

		position = (position + 1) % hash->buckets;
	}

	return(hash);
}

void _alpm_pkghash_free(pmpkghash_t *hash)
{
	size_t i;
	if(hash != NULL) {
		for(i = 0; i < hash->buckets; i++) {
			free(hash->hash_table[i]);
		}
		free(hash->hash_table);
	}
	free(hash);
}

pmpkg_t *_alpm_pkghash_find(pmpkghash_t *hash, const char *name)
{
	alpm_list_t *lp;
	unsigned long name_hash;
	size_t position;

	ALPM_LOG_FUNC;

	if(name == NULL || hash == NULL) {
		return(NULL);
	}

	name_hash = _alpm_hash_sdbm(name);

	position = name_hash % hash->buckets;

	while((lp = hash->hash_table[position]) != NULL) {
		pmpkg_t *info = lp->data;

		if(info->name_hash == name_hash && strcmp(info->name, name) == 0) {
			return(info);
		}

		position = (position + 1) % hash->buckets;
	}

	return(NULL);
}
