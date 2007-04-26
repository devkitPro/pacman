/*
 *  util.h
 *
 *  Copyright (c) 2002-2007 by Judd Vinet <jvinet@zeroflux.org>
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
#include <libintl.h> /* here so it doesn't need to be included elsewhere */

#include <alpm_list.h>

/* update speed for the fill_progress based functions */
#define UPDATE_SPEED_SEC 0.2f

/* define _() as shortcut for gettext() */
#ifdef ENABLE_NLS
#define _(str) gettext(str)
#else
#define _(str) str
#endif

int getcols();
int makepath(char *path);
int rmrf(char *path);
void indentprint(const char *str, int indent);
char *strtoupper(char *str);
char *strtrim(char *str);
int reg_match(char *string, char *pattern);
void list_display(const char *title, alpm_list_t *list);
void display_targets(alpm_list_t *syncpkgs);
int yesno(char *fmt, ...);

#endif /* _PM_UTIL_H */

/* vim: set ts=2 sw=2 noet: */
