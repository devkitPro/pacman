/*
 *  util.h
 * 
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2005 by Christian Hamar <krics@linuxforum.hu>
 *  Copyright (c) 2006 by David Kimpe <dnaku@frugalware.org>
 *  Copyright (c) 2005, 2006 by Miklos Vajna <vmiklos@frugalware.org>
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
#ifndef _ALPM_UTIL_H
#define _ALPM_UTIL_H

#include <stdio.h>
#include <libintl.h> /* here so it doesn't need to be included elsewhere */
#include <time.h>

#define FREE(p) do { if (p) { free(p); p = NULL; } } while(0)

#define ASSERT(cond, action) do { if(!(cond)) { action; } } while(0)

/* define _() as shortcut for gettext() */
#ifdef ENABLE_NLS
#define _(str) dgettext ("libalpm", str)
#else
#define _(s) s
#endif

int _alpm_makepath(const char *path);
int _alpm_copyfile(const char *src, const char *dest);
char *_alpm_strtoupper(char *str);
char *_alpm_strtrim(char *str);
char *_alpm_strreplace(const char *str, const char *needle, const char *replace);
int _alpm_lckmk();
int _alpm_lckrm();
int _alpm_unpack(const char *archive, const char *prefix, const char *fn);
int _alpm_rmrf(const char *path);
int _alpm_logaction(unsigned short usesyslog, FILE *f, const char *fmt, va_list args);
int _alpm_ldconfig(const char *root);
void _alpm_time2string(time_t t, char *buffer);
int _alpm_str_cmp(const void *s1, const void *s2);
char *_alpm_filecache_find(const char *filename);
const char *_alpm_filecache_setup(void);

#ifndef HAVE_STRVERSCMP
static int strverscmp(const char *, const char *);
#endif
#ifndef HAVE_STRSEP
char *strsep(char **, const char *);
#endif

/* check exported library symbols with: nm -C -D <lib> */
#define SYMEXPORT __attribute__((visibility("default")))
#define SYMHIDDEN __attribute__((visibility("hidden")))

#endif /* _ALPM_UTIL_H */

/* vim: set ts=2 sw=2 noet: */
