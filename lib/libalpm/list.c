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
#include <assert.h>
/* pacman */
#include "list.h"

/* Check PMList sanity
 *
 * 1: List seems to be OK.
 * 0: We're in deep ...
 */
int _alpm_list_check(PMList* list)
{
	PMList* it = NULL;

	if(list == NULL) {
		return(1);
	}
	if(list->last == NULL) {
		return(0);
	}

	for(it = list; it && it->next; it = it->next);
		if(it != list->last) {
			return(0);
		}

	return(1);
}

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
	list->last = list;
	return(list);
}

void pm_list_free(PMList *list)
{
	PMList *ptr, *it = list;

	while(it) {
		ptr = it->next;
		free(it->data);
		free(it);
		it = ptr;
	}
}

PMList* pm_list_add(PMList *list, void *data)
{
	PMList *ptr, *lp;

	ptr = list;
	if(ptr == NULL) {
		ptr = pm_list_new();
		if(ptr == NULL) {
			return(NULL);
		}
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
		lp->last = NULL;
		lp = lp->next;
	}

	lp->data = data;
	ptr->last = lp;

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
		iter->prev = add;   /*  Not at end.  */
	} else {
		if (list != NULL) {
			list->last = add;   /* Added new to end, so update the link to last. */
		}
	}

	if(prev != NULL) {
		prev->next = add;       /*  In middle.  */
	} else {
		if(list == NULL) {
			add->last = add;
		} else {
			add->last = list->last;
			list->last = NULL;
		}
		list = add;           /*  Start or empty, new list head.  */
	}

	return(list);
}

/* list: the beginning of the list
 * item: the item in the list to be removed
 *
 * returns:
 *     list with item removed
 */

PMList* _alpm_list_remove(PMList* list, PMList* item)
{
	assert(_alpm_list_check(list));

	if(list == NULL || item == NULL) {
		return(NULL);
	}

	/* Remove first item in list. */
	if(item == list) {
		if(list->next == NULL) {            /* Only item in list. */
			pm_list_free(item);
			return(NULL);
		} else {
			list->next->prev = NULL;
			list->next->last = list->last;
			list = list->next;
			item->prev = item->next = NULL;
			pm_list_free(item);
			return(list);
		}
	}

	/* Remove last item in list. */
	if(list->last == item) {
		list->last = item->prev;
		item->prev->next = NULL;
		item->prev = item->next = NULL;
		pm_list_free(item);
		return(list);
	}

	/* Remove middle item in list. */
	assert(item->prev != NULL && item->next != NULL);

	item->prev->next = item->next;
	item->next->prev = item->prev;
	item->prev = item->next = NULL;
	pm_list_free(item);

	assert(_alpm_list_check(list));

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
	if (list == NULL)
		return(NULL);

	assert(list->last != NULL);

	return(list->last);
}

/* Reverse the order of a list
 *
 * The caller is responsible for freeing the old list
 */
PMList* _alpm_list_reverse(PMList *list)
{ 
	/* simple but functional -- we just build a new list, starting
	 * with the old list's tail
	 */
	PMList *newlist = NULL;
	PMList *lp;

	for(lp = list->last; lp; lp = lp->prev) {
		newlist = pm_list_add(newlist, lp->data);
	}

	return(newlist);
}

/* vim: set ts=2 sw=2 noet: */
