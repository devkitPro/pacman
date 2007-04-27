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

#include "alpm_list.h"
#include "alpm.h"

#include <time.h>
#include <download.h>

/* Servers */
struct __pmserver_t {
	/* useless abstraction now? */
	struct url *s_url;
};

#define PM_DLBUF_LEN (1024 * 10)

pmserver_t *_alpm_server_new(const char *url);
void _alpm_server_free(pmserver_t *server);
int _alpm_downloadfiles(alpm_list_t *servers, const char *localpath, alpm_list_t *files);
int _alpm_downloadfiles_forreal(alpm_list_t *servers, const char *localpath,
	alpm_list_t *files, const char *mtime1, char *mtime2);

char *_alpm_fetch_pkgurl(char *target);

#endif /* _ALPM_SERVER_H */

/* vim: set ts=2 sw=2 noet: */
