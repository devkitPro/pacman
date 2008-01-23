/*
 *  server.c
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <download.h>

/* libalpm */
#include "server.h"
#include "alpm_list.h"
#include "error.h"
#include "log.h"
#include "alpm.h"
#include "util.h"
#include "handle.h"
#include "package.h"

pmserver_t *_alpm_server_new(const char *url)
{
	struct url *u;
	pmserver_t *server;

	ALPM_LOG_FUNC;

	CALLOC(server, 1, sizeof(pmserver_t), RET_ERR(PM_ERR_MEMORY, NULL));

	u = downloadParseURL(url);
	if(!u) {
		_alpm_log(PM_LOG_ERROR, _("url '%s' is invalid, ignoring\n"), url);
		RET_ERR(PM_ERR_SERVER_BAD_URL, NULL);
	}
	if(strlen(u->scheme) == 0) {
		_alpm_log(PM_LOG_WARNING, _("url scheme not specified, assuming http\n"));
		strcpy(u->scheme, "http");
	}

	if(strcmp(u->scheme,"ftp") == 0 && strlen(u->user) == 0) {
		strcpy(u->user, "anonymous");
		strcpy(u->pwd, "libalpm@guest");
	}

	/* remove trailing slashes, just to clean up the rest of the code */
	for(int i = strlen(u->doc) - 1; u->doc[i] == '/'; --i)
		u->doc[i] = '\0';

  server->s_url = u;

	return server;
}

void _alpm_server_free(pmserver_t *server)
{
	ALPM_LOG_FUNC;

	if(server == NULL) {
		return;
	}

	/* free memory */
	downloadFreeURL(server->s_url);
	FREE(server);
}

/* vim: set ts=2 sw=2 noet: */
