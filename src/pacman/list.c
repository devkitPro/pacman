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

extern int maxcols;

static list_t *list_last(list_t *list);

list_t *list_new()
{
	list_t *list = NULL;
	
	list = (list_t *)malloc(sizeof(list_t));
	if(list == NULL) {
		return(NULL);
	}
	list->data = NULL;
	list->next = NULL;
	return(list);
}

void list_free(list_t *list)
{
	list_t *ptr, *it = list;

	while(it) {
		ptr = it->next;
		free(it->data);
		free(it);
		it = ptr;
	}
	return;
}

list_t *list_add(list_t *list, void *data)
{
	list_t *ptr, *lp;

	ptr = list;
	if(ptr == NULL) {
		ptr = list_new();
	}

	lp = list_last(ptr);
	if(lp == ptr && lp->data == NULL) {
		/* nada */
	} else {
		lp->next = list_new();
		if(lp->next == NULL) {
			return(NULL);
		}
		lp = lp->next;
	}
	lp->data = data;
	return(ptr);
}

int list_count(list_t *list)
{
	int i;
	list_t *lp;

	for(lp = list, i = 0; lp; lp = lp->next, i++);

	return(i);
}

static list_t *list_last(list_t *list)
{
	list_t *ptr;

	for(ptr = list; ptr && ptr->next; ptr = ptr->next);
	return(ptr);
}

/* Test for existence of a string in a list_t
 */
int list_is_strin(char *needle, list_t *haystack)
{
	list_t *lp;

	for(lp = haystack; lp; lp = lp->next) {
		if(lp->data && !strcmp(lp->data, needle)) {
			return(1);
		}
	}
	return(0);
}

/* Display the content of a list_t struct of strings
 */

void list_display(const char *title, list_t *list)
{
	list_t *lp;
	int cols, len;

	len = strlen(title);
	printf("%s ", title);

	if(list) {
		for(lp = list, cols = len; lp; lp = lp->next) {
			int s = strlen((char *)lp->data)+1;
			if(s+cols >= maxcols) {
				int i;
				cols = len;
				printf("\n");
				for (i = 0; i < len+1; i++) {
					printf(" ");
				}
			}
			printf("%s ", (char *)lp->data);
			cols += s;
		}
		printf("\n");
	} else {
		printf("None\n");
	}
}

void PM_LIST_display(const char *title, PM_LIST *list)
{
	PM_LIST *lp;
	int cols, len;

	len = strlen(title);
	printf("%s ", title);

	if(list) {
		for(lp = list, cols = len; lp; lp = alpm_list_next(lp)) {
			int s = strlen(alpm_list_getdata(lp))+1;
			if(s+cols >= maxcols) {
				int i;
				cols = len;
				printf("\n");
				for (i = 0; i < len+1; i++) {
					printf(" ");
				}
			}
			printf("%s ", (char *)alpm_list_getdata(lp));
			cols += s;
		}
		printf("\n");
	} else {
		printf("None\n");
	}
}

/* Filter out any duplicate strings in a PM_LIST
 *
 * Not the most efficient way, but simple to implement -- we assemble
 * a new list, using is_in() to check for dupes at each iteration.
 *
 * This function takes a PM_LIST* and returns a list_t*
 *
 */
list_t *PM_LIST_remove_dupes(PM_LIST *list)
{
	PM_LIST *i;
	list_t *newlist = NULL;

	for(i = alpm_list_first(list); i; i = alpm_list_next(i)) {
		char *data = alpm_list_getdata(i);
		if(!list_is_strin(data, newlist)) {
			newlist = list_add(newlist, strdup(data));
		}
	}
	return newlist;
}

/* vim: set ts=2 sw=2 noet: */
