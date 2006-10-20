/*
 *  list.h
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

/* Chained list struct */
typedef struct __pmlist_t {
	void *data;
	struct __pmlist_t *prev;
	struct __pmlist_t *next;
	struct __pmlist_t *last; /* Quick access to last item in list */
} pmlist_t;

#define _FREELIST(p, f) do { if(p) { _alpm_list_free(p, f); p = NULL; } } while(0)
#define FREELIST(p) _FREELIST(p, free)
#define FREELISTPTR(p) _FREELIST(p, NULL)

typedef void (*_alpm_fn_free)(void *);
/* Sort comparison callback function declaration */
typedef int (*_alpm_fn_cmp)(const void *, const void *);

pmlist_t *_alpm_list_new(void);
void _alpm_list_free(pmlist_t *list, _alpm_fn_free fn);
pmlist_t *_alpm_list_add(pmlist_t *list, void *data);
pmlist_t *_alpm_list_add_sorted(pmlist_t *list, void *data, _alpm_fn_cmp fn);
pmlist_t *_alpm_list_remove(pmlist_t *haystack, void *needle, _alpm_fn_cmp fn, void **data);
int _alpm_list_count(pmlist_t *list);
int _alpm_list_is_in(void *needle, pmlist_t *haystack);
int _alpm_list_is_strin(char *needle, pmlist_t *haystack);
pmlist_t *_alpm_list_last(pmlist_t *list);
pmlist_t *_alpm_list_remove_dupes(pmlist_t *list);
pmlist_t *_alpm_list_reverse(pmlist_t *list);
pmlist_t *_alpm_list_strdup(pmlist_t *list);

#endif /* _ALPM_LIST_H */

/* vim: set ts=2 sw=2 noet: */
