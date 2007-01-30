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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
 *  USA.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libintl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <download.h>

/* pacman */
#include "server.h"
#include "error.h"
#include "log.h"
#include "alpm.h"
#include "util.h"
#include "handle.h"
#include "log.h"

pmserver_t *_alpm_server_new(const char *url)
{
	struct url *u;
	pmserver_t *server;

	ALPM_LOG_FUNC;

	server = (pmserver_t *)malloc(sizeof(pmserver_t));
	if(server == NULL) {
		_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(pmserver_t));
		RET_ERR(PM_ERR_MEMORY, NULL);
	}

	memset(server, 0, sizeof(pmserver_t));
	u = downloadParseURL(url);
	if(!u) {
		_alpm_log(PM_LOG_ERROR, _("url '%s' is invalid, ignoring"), url);
		return(NULL);
	}
	if(strlen(u->scheme) == 0) {
		_alpm_log(PM_LOG_WARNING, _("url scheme not specified, assuming http"));
		strcpy(u->scheme, "http");
	}

	if(strcmp(u->scheme,"ftp") == 0 && strlen(u->user) == 0) {
		strcpy(u->user, "anonymous");
		strcpy(u->pwd, "libalpm@guest");
	}

	/* This isn't needed... we can actually kill the whole pmserver_t interface
	 * and replace it with libdownload's 'struct url'
	 */
  server->s_url = u;
	server->path = strdup(u->doc);

	return server;
}

void _alpm_server_free(void *data)
{
	pmserver_t *server = data;

	ALPM_LOG_FUNC;

	if(server == NULL) {
		return;
	}

	/* free memory */
	FREE(server->path);
	downloadFreeURL(server->s_url);
	FREE(server);
}

/*
 * Download a list of files from a list of servers
 *   - if one server fails, we try the next one in the list
 *
 * RETURN:  0 for successful download, 1 on error
 */
int _alpm_downloadfiles(alpm_list_t *servers, const char *localpath, alpm_list_t *files)
{
	ALPM_LOG_FUNC;
	return(_alpm_downloadfiles_forreal(servers, localpath, files, NULL, NULL));
}

/*
 * This is the real downloadfiles, used directly by sync_synctree() to check
 * modtimes on remote files.
 *   - if *mtime1 is non-NULL, then only download files
 *     if they are different than *mtime1.  String should be in the form
 *     "YYYYMMDDHHMMSS" to match the form of ftplib's FtpModDate() function.
 *   - if *mtime2 is non-NULL, then it will be filled with the mtime
 *     of the remote file (from MDTM FTP cmd or Last-Modified HTTP header).
 * 
 * RETURN:  0 for successful download
 *          1 if the mtimes are identical
 *         -1 on error
 */
int _alpm_downloadfiles_forreal(alpm_list_t *servers, const char *localpath,
	alpm_list_t *files, const char *mtime1, char *mtime2)
{
	int dltotal_bytes = 0;
	alpm_list_t *lp;
	int done = 0;
	alpm_list_t *complete = NULL;
	alpm_list_t *i;

	ALPM_LOG_FUNC;

	if(files == NULL) {
		return(0);
	}

	for(i = servers; i && !done; i = i->next) {
		pmserver_t *server = (pmserver_t*)i->data;

		/* get each file in the list */
		for(lp = files; lp; lp = lp->next) {
			char realfile[PATH_MAX];
			char output[PATH_MAX];
			char *fn = (char *)lp->data;

			snprintf(realfile, PATH_MAX, "%s/%s", localpath, fn);
			snprintf(output, PATH_MAX, "%s/%s.part", localpath, fn);

			if(alpm_list_find_str(complete, fn)) {
				continue;
			}

			if(!handle->xfercommand) {
				FILE *dlf, *localf = NULL;
				struct url_stat ust;
				struct stat st;
				int chk_resume = 0;

				if(stat(output, &st) == 0 && st.st_size > 0) {
					_alpm_log(PM_LOG_DEBUG, _("existing file found, using it"));
					server->s_url->offset = (off_t)st.st_size;
					dltotal_bytes = st.st_size;
					localf = fopen(output, "a");
					chk_resume = 1;
				} else {
					server->s_url->offset = (off_t)0;
					dltotal_bytes = 0;
				}
				
				FREE(server->s_url->doc);
				int len = strlen(server->path) + strlen(fn) + 2;
				server->s_url->doc = (char *)malloc(len);
				snprintf(server->s_url->doc, len, "%s/%s", server->path, fn);

				/* libdownload does not reset the error code, reset it in the case of previous errors */
				downloadLastErrCode = 0;

				/* 10s timeout - TODO make a config option */
				downloadTimeout = 10000;

			 	/* Make libdownload super verbose... worthwhile for testing */
				if(alpm_option_get_logmask() & PM_LOG_DOWNLOAD) {
						downloadDebug = 1;
				}
				if(alpm_option_get_logmask() & PM_LOG_DEBUG) {
						dlf = downloadXGet(server->s_url, &ust, (handle->nopassiveftp ? "v" : "vp"));
				} else {
						dlf = downloadXGet(server->s_url, &ust, (handle->nopassiveftp ? "" : "p"));
				}

				if(downloadLastErrCode != 0 || dlf == NULL) {
					_alpm_log(PM_LOG_ERROR, _("failed retrieving file '%s' from %s://%s: %s"), fn,
										server->s_url->scheme, server->s_url->host, downloadLastErrString);
					if(localf != NULL) {
						fclose(localf);
					}
					/* try the next server */
					continue;
				} else {
						_alpm_log(PM_LOG_DEBUG, _("server connection to %s complete"), server->s_url->host);
				}
				
				if(ust.mtime && mtime1) {
					char strtime[15];
					_alpm_time2string(ust.mtime, strtime);
					if(strcmp(mtime1, strtime) == 0) {
						_alpm_log(PM_LOG_DEBUG, _("mtimes are identical, skipping %s"), fn);
						complete = alpm_list_add(complete, fn);
						if(localf != NULL) {
							fclose(localf);
						}
						if(dlf != NULL) {
							fclose(dlf);
						}
						return(1);
					}
				}
				
				if(ust.mtime && mtime2) {
					_alpm_time2string(ust.mtime, mtime2);
				}

				if(chk_resume && server->s_url->offset == 0) {
					_alpm_log(PM_LOG_WARNING, _("cannot resume download, starting over"));
					if(localf != NULL) {
						fclose(localf);
						localf = NULL;
					}
				}

				if(localf == NULL) {
					_alpm_rmrf(output);
					server->s_url->offset = (off_t)0;
					dltotal_bytes = 0;
					localf = fopen(output, "w");
					if(localf == NULL) { /* still null? */
						_alpm_log(PM_LOG_ERROR, _("cannot write to file '%s'"), output);
						return -1;
					}
				}

				/* Progress 0 - initialize */
				if(handle->dlcb) handle->dlcb(fn, 0, ust.size);

				int nread = 0;
				char buffer[PM_DLBUF_LEN];
				while((nread = fread(buffer, 1, PM_DLBUF_LEN, dlf)) > 0) {
					int nwritten = 0;
					while((nwritten += fwrite(buffer, 1, (nread - nwritten), localf)) < nread) ;
					dltotal_bytes += nread;

					if(handle->dlcb) handle->dlcb(fn, dltotal_bytes, ust.size);
				}

				fclose(localf);
				fclose(dlf);
				rename(output, realfile);
				complete = alpm_list_add(complete, fn);
			} else {
				int ret;
				int usepart = 0;
				char *ptr1, *ptr2;
				char origCmd[PATH_MAX];
				char parsedCmd[PATH_MAX] = "";
				char url[PATH_MAX];
				char cwd[PATH_MAX];
				/* build the full download url */
				snprintf(url, PATH_MAX, "%s://%s%s/%s", server->s_url->scheme, server->s_url->host,
						server->s_url->doc, fn);
				/* replace all occurrences of %o with fn.part */
				strncpy(origCmd, handle->xfercommand, sizeof(origCmd));
				ptr1 = origCmd;
				while((ptr2 = strstr(ptr1, "%o"))) {
					usepart = 1;
					ptr2[0] = '\0';
					strcat(parsedCmd, ptr1);
					strcat(parsedCmd, fn);
					strcat(parsedCmd, ".part");
					ptr1 = ptr2 + 2;
				}
				strcat(parsedCmd, ptr1);
				/* replace all occurrences of %u with the download URL */
				strncpy(origCmd, parsedCmd, sizeof(origCmd));
				parsedCmd[0] = '\0';
				ptr1 = origCmd;
				while((ptr2 = strstr(ptr1, "%u"))) {
					ptr2[0] = '\0';
					strcat(parsedCmd, ptr1);
					strcat(parsedCmd, url);
					ptr1 = ptr2 + 2;
				}
				strcat(parsedCmd, ptr1);
				/* cwd to the download directory */
				getcwd(cwd, PATH_MAX);
				if(chdir(localpath)) {
					_alpm_log(PM_LOG_WARNING, _("could not chdir to %s"), localpath);
					return(PM_ERR_CONNECT_FAILED);
				}
				/* execute the parsed command via /bin/sh -c */
				_alpm_log(PM_LOG_DEBUG, _("running command: %s"), parsedCmd);
				ret = system(parsedCmd);
				if(ret == -1) {
					_alpm_log(PM_LOG_WARNING, _("running XferCommand: fork failed!"));
					return(PM_ERR_FORK_FAILED);
				} else if(ret != 0) {
					/* download failed */
					_alpm_log(PM_LOG_DEBUG, _("XferCommand command returned non-zero status code (%d)"), ret);
				} else {
					/* download was successful */
					complete = alpm_list_add(complete, fn);
					if(usepart) {
						char fnpart[PATH_MAX];
						/* rename "output.part" file to "output" file */
						snprintf(fnpart, PATH_MAX, "%s.part", fn);
						rename(fnpart, fn);
					}
				}
				chdir(cwd);
			}
		}

		if(alpm_list_count(complete) == alpm_list_count(files)) {
			done = 1;
		}
	}

	return(done ? 0 : -1);
}

char *_alpm_fetch_pkgurl(char *target)
{
	char *p = NULL;
	struct stat st;
	struct url *s_url;

	ALPM_LOG_FUNC;

	s_url = downloadParseURL(target);
	if(!s_url) {
		_alpm_log(PM_LOG_ERROR, _("url '%s' is invalid, ignoring"), target);
		return(NULL);
	}
	if(strlen(s_url->scheme) == 0) {
		_alpm_log(PM_LOG_WARNING, _("url scheme not specified, assuming http"));
		strcpy(s_url->scheme, "http");
	}

	if(strcmp(s_url->scheme,"ftp") == 0 && strlen(s_url->user) == 0) {
		strcpy(s_url->user, "anonymous");
		strcpy(s_url->pwd, "libalpm@guest");
	}

	/* do not download the file if it exists in the current dir */
	if(stat(s_url->doc, &st) == 0) {
		_alpm_log(PM_LOG_DEBUG, _(" %s is already in the current directory"), s_url->doc);
	} else {
		pmserver_t *server;
		alpm_list_t *servers = NULL;
		alpm_list_t *files;

		if((server = (pmserver_t *)malloc(sizeof(pmserver_t))) == NULL) {
			_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(pmserver_t));
			return(NULL);
		}
		if(s_url->doc && (p = strrchr(s_url->doc,'/'))) {
			*p++ = '\0';
			_alpm_log(PM_LOG_DEBUG, _("downloading '%s' from '%s://%s%s"), p, s_url->scheme, s_url->host, s_url->doc);

			server->s_url = s_url;
			server->path = strdup(s_url->doc);
			servers = alpm_list_add(servers, server);

			files = alpm_list_add(NULL, strdup(p));
			if(_alpm_downloadfiles(servers, ".", files)) {
				_alpm_log(PM_LOG_WARNING, _("failed to download %s"), target);
				return(NULL);
			}
			FREELISTPTR(files);
			FREELIST(servers);
		}
	}

	/* dupe before we free the URL struct...*/
	if(p) {
		p = strdup(p);
	}

	downloadFreeURL(s_url);

	/* return the target with the raw filename, no URL */
	return(p);
}

/* vim: set ts=2 sw=2 noet: */
