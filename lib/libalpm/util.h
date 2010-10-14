/*
 *  util.h
 *
 *  Copyright (c) 2006-2010 Pacman Development Team <pacman-dev@archlinux.org>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _ALPM_UTIL_H
#define _ALPM_UTIL_H

#include "config.h"

#include "alpm_list.h"
#include "package.h" /* pmpkg_t */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h> /* struct stat */
#include <archive.h> /* struct archive */

#ifdef ENABLE_NLS
#include <libintl.h> /* here so it doesn't need to be included elsewhere */
/* define _() as shortcut for gettext() */
#define _(str) dgettext ("libalpm", str)
#else
#define _(s) s
#endif

#define ALLOC_FAIL(s) do { _alpm_log(PM_LOG_ERROR, _("alloc failure: could not allocate %zd bytes\n"), s); } while(0)

#define MALLOC(p, s, action) do { p = calloc(1, s); if(p == NULL) { ALLOC_FAIL(s); action; } } while(0)
#define CALLOC(p, l, s, action) do { p = calloc(l, s); if(p == NULL) { ALLOC_FAIL(s); action; } } while(0)
/* This strdup macro is NULL safe- copying NULL will yield NULL */
#define STRDUP(r, s, action) do { if(s != NULL) { r = strdup(s); if(r == NULL) { ALLOC_FAIL(strlen(s)); action; } } else { r = NULL; } } while(0)

#define FREE(p) do { free(p); p = NULL; } while(0)

#define ASSERT(cond, action) do { if(!(cond)) { action; } } while(0)

#define RET_ERR(err, ret) do { pm_errno = (err); \
	_alpm_log(PM_LOG_DEBUG, "returning error %d from %s : %s\n", err, __func__, alpm_strerrorlast()); \
	return(ret); } while(0)

int _alpm_makepath(const char *path);
int _alpm_makepath_mode(const char *path, mode_t mode);
int _alpm_copyfile(const char *src, const char *dest);
char *_alpm_strtrim(char *str);
int _alpm_lckmk();
int _alpm_lckrm();
int _alpm_unpack_single(const char *archive, const char *prefix, const char *fn);
int _alpm_unpack(const char *archive, const char *prefix, alpm_list_t *list, int breakfirst);
int _alpm_rmrf(const char *path);
int _alpm_logaction(int usesyslog, FILE *f, const char *fmt, va_list args);
int _alpm_run_chroot(const char *root, const char *path, char *const argv[]);
int _alpm_ldconfig(const char *root);
int _alpm_str_cmp(const void *s1, const void *s2);
char *_alpm_filecache_find(const char *filename);
const char *_alpm_filecache_setup(void);
int _alpm_lstat(const char *path, struct stat *buf);
int _alpm_test_md5sum(const char *filepath, const char *md5sum);
char *_alpm_archive_fgets(char *line, size_t size, struct archive *a);
int _alpm_splitname(const char *target, pmpkg_t *pkg);

#ifndef HAVE_STRSEP
char *strsep(char **, const char *);
#endif

/* check exported library symbols with: nm -C -D <lib> */
#define SYMEXPORT __attribute__((visibility("default")))
#define SYMHIDDEN __attribute__((visibility("internal")))

/* max percent of package size to download deltas */
#define MAX_DELTA_RATIO 0.7

#endif /* _ALPM_UTIL_H */

/* vim: set ts=2 sw=2 noet: */
