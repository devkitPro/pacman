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
#include <archive.h>
#include <archive_entry.h>

#define FREE(p) do { if (p) { free(p); p = NULL; } } while(0)

#define ASSERT(cond, action) do { if(!(cond)) { action; } } while(0)

#define ARCHIVE_EXTRACT_FLAGS ARCHIVE_EXTRACT_OWNER | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_TIME

/* define _() as shortcut for gettext() */
#ifdef ENABLE_NLS
#define _(str) dgettext ("libalpm", str)
#else
#define _(s) s
#endif

/*TODO wtf? why is this done like this? */
#define SCRIPTLET_START "START "
#define SCRIPTLET_DONE "DONE "

int _alpm_makepath(const char *path);
int _alpm_copyfile(const char *src, const char *dest);
char *_alpm_strtoupper(char *str);
char *_alpm_strtrim(char *str);
int _alpm_lckmk(const char *file);
int _alpm_lckrm(const char *file);
int _alpm_unpack(const char *archive, const char *prefix, const char *fn);
int _alpm_rmrf(const char *path);
int _alpm_logaction(unsigned short usesyslog, FILE *f, const char *str);
int _alpm_ldconfig(const char *root);
/* TODO wtf? this can't be right */
#ifdef _ALPM_TRANS_H
int _alpm_runscriptlet(const char *root, const char *installfn,
											 const char *script, const char *ver,
											 const char *oldver, pmtrans_t *trans);
#ifndef __sun__
int _alpm_check_freespace(pmtrans_t *trans, alpm_list_t **data);
#endif
#endif
void _alpm_time2string(time_t t, char *buffer);
int _alpm_str_cmp(const void *s1, const void *s2);

#ifdef __sun__
char* strsep(char** str, const char* delims);
char* mkdtemp(char *template);
#endif

/* check exported library symbols with: nm -C -D <lib> */
#define SYMEXPORT __attribute__((visibility("default")))
#define SYMHIDDEN __attribute__((visibility("hidden")))

#endif /* _ALPM_UTIL_H */

/* vim: set ts=2 sw=2 noet: */
