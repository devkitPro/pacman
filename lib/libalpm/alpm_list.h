/*
 *  alpm_alpm_list.h
 * 
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
 *  USA.
 */
#ifndef _ALPM_LIST_H
#define _ALPM_LIST_H

#include "alpm.h"

/* Chained list struct */
struct __alpm_list_t {
	void *data;
	struct __alpm_list_t *prev;
	struct __alpm_list_t *next;
};

/* TODO we should do away with these... they're messy*/
#define _FREELIST(p, f) do { alpm_list_free_inner(p, f); alpm_list_free(p); p = NULL; } while(0)
#define FREELIST(p)     _FREELIST(p, free)
#define FREELISTPTR(p)  do { alpm_list_free(p); p = NULL; } while(0)

typedef void (*alpm_list_fn_free)(void *); /* item deallocation callback */
typedef int (*alpm_list_fn_cmp)(const void *, const void *); /* item comparison callback */

/* allocation */
alpm_list_t *alpm_list_new(void);
void alpm_list_free(alpm_list_t *list);
void alpm_list_free_inner(alpm_list_t *list, alpm_list_fn_free fn);

/* item mutators */
alpm_list_t *alpm_list_add(alpm_list_t *list, void *data);
alpm_list_t *alpm_list_add_sorted(alpm_list_t *list, void *data, alpm_list_fn_cmp fn);
alpm_list_t* alpm_list_mmerge(alpm_list_t *left, alpm_list_t *right, alpm_list_fn_cmp fn);
alpm_list_t* alpm_list_msort(alpm_list_t *list, int n, alpm_list_fn_cmp fn);
alpm_list_t *alpm_list_remove(alpm_list_t *haystack, void *needle, alpm_list_fn_cmp fn, void **data);
alpm_list_t *alpm_list_remove_node(alpm_list_t *node);
alpm_list_t *alpm_list_remove_dupes(alpm_list_t *list);
alpm_list_t *alpm_list_strdup(alpm_list_t *list);
alpm_list_t *alpm_list_reverse(alpm_list_t *list);

/* item accessors */
alpm_list_t *alpm_list_first(alpm_list_t *list);
alpm_list_t* alpm_list_nth(alpm_list_t *list, int n);
alpm_list_t *alpm_list_next(alpm_list_t *list);
alpm_list_t *alpm_list_last(alpm_list_t *list);
void *alpm_list_getdata(const alpm_list_t *entry);
#define alpm_list_data(type, list) (type)alpm_list_getdata((list))

/* misc */
int alpm_list_count(const alpm_list_t *list);
int alpm_list_is_in(const void *needle, alpm_list_t *haystack);
int alpm_list_is_strin(const char *needle, alpm_list_t *haystack);

#endif /* _ALPM_LIST_H */

/* vim: set ts=2 sw=2 noet: */
