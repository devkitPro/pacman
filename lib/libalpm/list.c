/*
 *  list.c
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

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
/* pacman */
#include "list.h"

PMList* pm_list_new()
{
	PMList *list = NULL;
	
	list = (PMList *)malloc(sizeof(PMList));
	if(list == NULL) {
		return(NULL);
	}
	list->data = NULL;
	list->prev = NULL;
	list->next = NULL;
	return(list);
}

void pm_list_free(PMList *list)
{
	if(list == NULL) {
		return;
	}
	if(list->data != NULL) {
		free(list->data);
		list->data = NULL;
	}
	if(list->next != NULL) {
		pm_list_free(list->next);
	}
	free(list);
	return;
}

PMList* pm_list_add(PMList *list, void *data)
{
	PMList *ptr, *lp;

	ptr = list;
	if(ptr == NULL) {
		ptr = pm_list_new();
	}

	lp = pm_list_last(ptr);
	if(lp == ptr && lp->data == NULL) {
		/* nada */
	} else {
		lp->next = pm_list_new();
		if(lp->next == NULL) {
			return(NULL);
		}
		lp->next->prev = lp;
		lp = lp->next;
	}
	lp->data = data;
	return(ptr);
}

/* Add items to a list in sorted order. Use the given comparision func to 
 * determine order.
 */
PMList* pm_list_add_sorted(PMList *list, void *data, pm_fn_cmp fn)
{
	PMList *add;
	PMList *prev = NULL;
	PMList *iter = list;

	add = pm_list_new();
	add->data = data;

	/* Find insertion point. */
	while(iter) {
		if(fn(add->data, iter->data) <= 0) break;
		prev = iter;
		iter = iter->next;
	}

	/*  Insert node before insertion point. */
	add->prev = prev;
	add->next = iter;
	if(iter != NULL) {
		 /* Not at end. */
		iter->prev = add;
	}
	if(prev != NULL) {
		 /* In middle. */
		prev->next = add;
	} else {
		/* Start or empty, new list head. */
		list = add;
	}

	return(list);
}

/* Remove an item in a list. Use the given comparaison function to find the
 * item.
 * If found, 'ptr' is set to point to the removed element, so that the caller
 * can free it. Otherwise, ptr is NULL.
 * Return the new list (without the removed element).
 */
PMList *_alpm_list_remove(PMList *list, void *data, pm_fn_cmp fn, void **ptr)
{
	PMList *i = list;

	while(i) {
		if(fn(data, i->data) == 0) {
			break;
		}
		i = i->next;
	}

	if(ptr) {
		*ptr = NULL;
	}

	if(i) {
		/* we found a matching item */
		if(i->next) {
			i->next->prev = i->prev;
		}
		if(i->prev) {
			i->prev->next = i->next;
		}
		if(i == list) {
			/* The item found is the first in the chain,
			 * so we move the header to the next element.
			 */
			list = list->next;
		}

		if(ptr) {
			*ptr = i->data;
		}

		free(i);
	}

	return(list);
}

int pm_list_count(PMList *list)
{
	int i;
	PMList *lp;

	for(lp = list, i = 0; lp; lp = lp->next, i++);

	return(i);
}

int pm_list_is_ptrin(PMList *haystack, void *needle)
{
	PMList *lp;

	for(lp = haystack; lp; lp = lp->next) {
		if(lp->data == needle) {
			return(1);
		}
	}
	return(0);
}

/* Test for existence of a string in a PMList
 */
PMList *pm_list_is_strin(char *needle, PMList *haystack)
{
	PMList *lp;

	for(lp = haystack; lp; lp = lp->next) {
		if(lp->data && !strcmp(lp->data, needle)) {
			return(lp);
		}
	}
	return(NULL);
}

PMList* pm_list_last(PMList *list)
{
	PMList *ptr;

	for(ptr = list; ptr && ptr->next; ptr = ptr->next);
	return(ptr);
}

/* vim: set ts=2 sw=2 noet: */
