/*
 *  util.h
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
#ifndef _PM_UTIL_H
#define _PM_UTIL_H

#include <stdlib.h>
#include <string.h>
#include <libintl.h>

#include <alpm.h>
#include <alpm_list.h>

#define MALLOC(p, b) do { \
	if((b) > 0) { \
		p = malloc(b); \
		if (!(p)) { \
			fprintf(stderr, "malloc failure: could not allocate %d bytes\n", (int)b); \
			exit(EXIT_FAILURE); \
		} \
	} else { \
		p = NULL; \
	} \
} while(0)

#define FREE(p) do { if (p) { free(p); (p) = NULL; }} while(0)

#define STRNCPY(s1, s2, len) do { \
	strncpy(s1, s2, (len)-1); \
	s1[(len)-1] = 0; \
} while(0)

#define _(str) gettext(str)
unsigned int getcols();
int makepath(char *path);
int rmrf(char *path);
void indentprint(const char *str, unsigned int indent);
char *buildstring(alpm_list_t *strlist);
char *strtoupper(char *str);
char *strtrim(char *str);
int reg_match(char *string, char *pattern);
void list_display(const char *title, alpm_list_t *list);

#endif /* _PM_UTIL_H */

/* vim: set ts=2 sw=2 noet: */
