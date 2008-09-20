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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _PM_UTIL_H
#define _PM_UTIL_H

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <alpm_list.h>

#ifdef ENABLE_NLS
#include <libintl.h> /* here so it doesn't need to be included elsewhere */
/* define _() as shortcut for gettext() */
#define _(str) gettext(str)
#else
#define _(str) str
#endif

/* update speed for the fill_progress based functions */
#define UPDATE_SPEED_SEC 0.2f

int trans_init(pmtranstype_t type, pmtransflag_t flags);
int trans_release(void);
int needs_transaction(void);
int getcols(void);
int makepath(const char *path);
int rmrf(const char *path);
char *mbasename(const char *path);
char *mdirname(const char *path);
void indentprint(const char *str, int indent);
char *strtoupper(char *str);
char *strtrim(char *str);
char *strreplace(const char *str, const char *needle, const char *replace);
alpm_list_t *strsplit(const char *str, const char splitchar);
void string_display(const char *title, const char *string);
void list_display(const char *title, const alpm_list_t *list);
void list_display_linebreak(const char *title, const alpm_list_t *list);
void display_targets(const alpm_list_t *pkgs, int install);
void display_synctargets(const alpm_list_t *syncpkgs);
void display_optdepends(pmpkg_t *pkg);
int yesno(short preset, char *fmt, ...);
int pm_printf(pmloglevel_t level, const char *format, ...) __attribute__((format(printf,2,3)));
int pm_fprintf(FILE *stream, pmloglevel_t level, const char *format, ...) __attribute__((format(printf,3,4)));
int pm_vfprintf(FILE *stream, pmloglevel_t level, const char *format, va_list args) __attribute__((format(printf,3,0)));
int pm_vasprintf(char **string, pmloglevel_t level, const char *format, va_list args) __attribute__((format(printf,3,0)));

#ifndef HAVE_STRNDUP
char *strndup(const char *s, size_t n);
#endif

#endif /* _PM_UTIL_H */

/* vim: set ts=2 sw=2 noet: */
