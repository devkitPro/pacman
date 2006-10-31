/*
 *  server.h
 * 
 *  Copyright (c) 2006 by Miklos Vajna <vmiklos@frugalware.org>
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
#ifndef _ALPM_SERVER_H
#define _ALPM_SERVER_H

#include "list.h"
#include <time.h>

#include <fetch.h>

#define FREESERVER(p) \
do { \
	if(p) { \
		_alpm_server_free(p); \
		p = NULL; \
	} \
} while(0)

#define FREELISTSERVERS(p) _FREELIST(p, _alpm_server_free)

/* Servers */
typedef struct __pmserver_t {
	char *path;
	struct url *s_url;
} pmserver_t;

#define PM_DLBUF_LEN (1024 * 10)

typedef void (*download_progress_cb)(const char *filename, int xfered, int total);

pmserver_t *_alpm_server_new(char *url);
void _alpm_server_free(void *data);
int _alpm_downloadfiles(pmlist_t *servers, const char *localpath, pmlist_t *files);
int _alpm_downloadfiles_forreal(pmlist_t *servers, const char *localpath,
	pmlist_t *files, const char *mtime1, char *mtime2);

char *_alpm_fetch_pkgurl(char *target);

extern download_progress_cb pm_dlcb;

#endif /* _ALPM_SERVER_H */

/* vim: set ts=2 sw=2 noet: */
