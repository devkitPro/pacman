/*
 *  util.h
 *
 *  Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
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
#define _n(str1, str2, ct) ngettext(str1, str2, ct)
#else
#define _(str) str
#define _n(str1, str2, ct) (ct == 1 ? str1 : str2)
#endif

/* update speed for the fill_progress based functions */
#define UPDATE_SPEED_SEC 0.2f

int trans_init(pmtransflag_t flags);
int trans_release(void);
int needs_root(void);
int getcols(int def);
int rmrf(const char *path);
const char *mbasename(const char *path);
char *mdirname(const char *path);
void indentprint(const char *str, int indent);
char *strtoupper(char *str);
char *strtrim(char *str);
char *strreplace(const char *str, const char *needle, const char *replace);
alpm_list_t *strsplit(const char *str, const char splitchar);
void string_display(const char *title, const char *string);
double humanize_size(off_t bytes, const char target_unit, int long_labels, const char **label);
int table_display(const char *title, const alpm_list_t *header, const alpm_list_t *rows);
void list_display(const char *title, const alpm_list_t *list);
void list_display_linebreak(const char *title, const alpm_list_t *list);
void display_targets(const alpm_list_t *pkgs, int install);
int str_cmp(const void *s1, const void *s2);
void display_new_optdepends(pmpkg_t *oldpkg, pmpkg_t *newpkg);
void display_optdepends(pmpkg_t *pkg);
void print_packages(const alpm_list_t *packages);
void select_display(const alpm_list_t *pkglist);
int select_question(int count);
int multiselect_question(char *array, int count);
int yesno(char *fmt, ...);
int noyes(char *fmt, ...);
int pm_printf(pmloglevel_t level, const char *format, ...) __attribute__((format(printf,2,3)));
int pm_fprintf(FILE *stream, pmloglevel_t level, const char *format, ...) __attribute__((format(printf,3,4)));
int pm_asprintf(char **string, const char *format, ...);
int pm_vfprintf(FILE *stream, pmloglevel_t level, const char *format, va_list args) __attribute__((format(printf,3,0)));
int pm_vasprintf(char **string, pmloglevel_t level, const char *format, va_list args) __attribute__((format(printf,3,0)));

#ifndef HAVE_STRNDUP
char *strndup(const char *s, size_t n);
#endif

#endif /* _PM_UTIL_H */

/* vim: set ts=2 sw=2 noet: */
