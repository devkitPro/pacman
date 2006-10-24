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
#include <sys/time.h>
#include <ftplib.h>

/* pacman */
#include "server.h"
#include "error.h"
#include "log.h"
#include "alpm.h"
#include "util.h"
#include "handle.h"

FtpCallback pm_dlcb = NULL;
/* progress bar */
char *pm_dlfnm=NULL;
int *pm_dloffset=NULL;
struct timeval *pm_dlt0=NULL, *pm_dlt=NULL;
float *pm_dlrate=NULL;
int *pm_dlxfered1=NULL;
unsigned char *pm_dleta_h=NULL, *pm_dleta_m=NULL, *pm_dleta_s=NULL;

pmserver_t *_alpm_server_new(char *url)
{
	pmserver_t *server;
	char *ptr;

	server = (pmserver_t *)malloc(sizeof(pmserver_t));
	if(server == NULL) {
		_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(pmserver_t));
		RET_ERR(PM_ERR_MEMORY, NULL);
	}

	memset(server, 0, sizeof(pmserver_t));

	/* parse our special url */
	ptr = strstr(url, "://");
	if(ptr == NULL) {
		RET_ERR(PM_ERR_SERVER_BAD_LOCATION, NULL);
	}
	*ptr = '\0';
	ptr++; ptr++; ptr++;
	if(ptr == NULL || *ptr == '\0') {
		RET_ERR(PM_ERR_SERVER_BAD_LOCATION, NULL);
	}
	server->protocol = strdup(url);
	if(!strcmp(server->protocol, "ftp") || !strcmp(server->protocol, "http")) {
		char *slash;
		/* split the url into domain and path */
		slash = strchr(ptr, '/');
		if(slash == NULL) {
			/* no path included, default to / */
			server->path = strdup("/");
		} else {
			/* add a trailing slash if we need to */
			if(slash[strlen(slash)-1] == '/') {
				server->path = strdup(slash);
			} else {
				if((server->path = (char *)malloc(strlen(slash)+2)) == NULL) {
					_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(strlen(slash+2)));
					RET_ERR(PM_ERR_MEMORY, NULL);
				}
				sprintf(server->path, "%s/", slash);
			}
			*slash = '\0';
		}
		server->server = strdup(ptr);
	} else if(!strcmp(server->protocol, "file")){
		/* add a trailing slash if we need to */
		if(ptr[strlen(ptr)-1] == '/') {
			server->path = strdup(ptr);
		} else {
			server->path = (char *)malloc(strlen(ptr)+2);
			if(server->path == NULL) {
				_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(strlen(ptr+2)));
				RET_ERR(PM_ERR_MEMORY, NULL);
			}
			sprintf(server->path, "%s/", ptr);
		}
	} else {
		RET_ERR(PM_ERR_SERVER_PROTOCOL_UNSUPPORTED, NULL);
	}

	return(server);
}

void _alpm_server_free(void *data)
{
	pmserver_t *server = data;

	if(server == NULL) {
		return;
	}

	/* free memory */
	FREE(server->protocol);
	FREE(server->server);
	FREE(server->path);
	free(server);
}

/*
 * Download a list of files from a list of servers
 *   - if one server fails, we try the next one in the list
 *
 * RETURN:  0 for successful download, 1 on error
 */
int _alpm_downloadfiles(pmlist_t *servers, const char *localpath, pmlist_t *files)
{
	return(!!_alpm_downloadfiles_forreal(servers, localpath, files, NULL, NULL));
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
 *         -1 if the mtimes are identical
 *          1 on error
 */
int _alpm_downloadfiles_forreal(pmlist_t *servers, const char *localpath,
	pmlist_t *files, const char *mtime1, char *mtime2)
{
	int fsz;
	netbuf *control = NULL;
	pmlist_t *lp;
	int done = 0;
	pmlist_t *complete = NULL;
	pmlist_t *i;

	if(files == NULL) {
		return(0);
	}

  _alpm_log(PM_LOG_DEBUG, _("server check, %d\n"),servers);
	for(i = servers; i && !done; i = i->next) {
		_alpm_log(PM_LOG_DEBUG, _("server check, done? %d\n"),done);
		pmserver_t *server = (pmserver_t*)i->data;

		if(!handle->xfercommand && strcmp(server->protocol, "file")) {
			if(!strcmp(server->protocol, "ftp") && !handle->proxyhost) {
				FtpInit();
				_alpm_log(PM_LOG_DEBUG, _("connecting to %s:21\n"), server->server);
				if(!FtpConnect(server->server, &control)) {
					_alpm_log(PM_LOG_WARNING, _("cannot connect to %s\n"), server->server);
					continue;
				}
				if(!FtpLogin("anonymous", "libalpm@guest", control)) {
					_alpm_log(PM_LOG_WARNING, _("anonymous login failed\n"));
					FtpQuit(control);
					continue;
				}	
				if(!FtpChdir(server->path, control)) {
					_alpm_log(PM_LOG_WARNING, _("could not cwd to %s: %s\n"), server->path, FtpLastResponse(control));
					FtpQuit(control);
					continue;
				}
				if(!handle->nopassiveftp) {
					if(!FtpOptions(FTPLIB_CONNMODE, FTPLIB_PASSIVE, control)) {
					_alpm_log(PM_LOG_WARNING, _("failed to set passive mode\n"));
					}
				} else {
					_alpm_log(PM_LOG_DEBUG, _("FTP passive mode not set\n"));
				}
			} else if(handle->proxyhost) {
				char *host;
				unsigned port;
				host = (handle->proxyhost) ? handle->proxyhost : server->server;
				port = (handle->proxyport) ? handle->proxyport : 80;
				if(strchr(host, ':')) {
					_alpm_log(PM_LOG_DEBUG, _("connecting to %s\n"), host);
				} else {
					_alpm_log(PM_LOG_DEBUG, _("connecting to %s:%u\n"), host, port);
				}
				if(!HttpConnect(host, port, &control)) {
					_alpm_log(PM_LOG_WARNING, _("cannot connect to %s\n"), host);
					continue;
				}
			}

			/* set up our progress bar's callback (and idle timeout) */
			if(strcmp(server->protocol, "file") && control) {
				if(pm_dlcb) {
					FtpOptions(FTPLIB_CALLBACK, (long)pm_dlcb, control);
				} else {
					_alpm_log(PM_LOG_DEBUG, _("downloadfiles: progress bar's callback is not set\n"));
				}
				FtpOptions(FTPLIB_IDLETIME, (long)1000, control);
				FtpOptions(FTPLIB_CALLBACKARG, (long)&fsz, control);
				FtpOptions(FTPLIB_CALLBACKBYTES, (10*1024), control);
			}
		}

		/* get each file in the list */
		for(lp = files; lp; lp = lp->next) {
			char *fn = (char *)lp->data;

			if(_alpm_list_is_strin(fn, complete)) {
				continue;
			}

			if(handle->xfercommand && strcmp(server->protocol, "file")) {
				int ret;
				int usepart = 0;
				char *ptr1, *ptr2;
				char origCmd[PATH_MAX];
				char parsedCmd[PATH_MAX] = "";
				char url[PATH_MAX];
				char cwd[PATH_MAX];
				/* build the full download url */
				snprintf(url, PATH_MAX, "%s://%s%s%s", server->protocol, server->server,
						server->path, fn);
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
					_alpm_log(PM_LOG_WARNING, _("could not chdir to %s\n"), localpath);
					return(PM_ERR_CONNECT_FAILED);
				}
				/* execute the parsed command via /bin/sh -c */
				_alpm_log(PM_LOG_DEBUG, _("running command: %s\n"), parsedCmd);
				ret = system(parsedCmd);
				if(ret == -1) {
					_alpm_log(PM_LOG_WARNING, _("running XferCommand: fork failed!\n"));
					return(PM_ERR_FORK_FAILED);
				} else if(ret != 0) {
					/* download failed */
					_alpm_log(PM_LOG_DEBUG, _("XferCommand command returned non-zero status code (%d)\n"), ret);
				} else {
					/* download was successful */
					complete = _alpm_list_add(complete, fn);
					if(usepart) {
						char fnpart[PATH_MAX];
						/* rename "output.part" file to "output" file */
						snprintf(fnpart, PATH_MAX, "%s.part", fn);
						rename(fnpart, fn);
					}
				}
				chdir(cwd);
			} else {
				char output[PATH_MAX];
				unsigned int j;
				int filedone = 0;
				char *ptr;
				struct stat st;
				snprintf(output, PATH_MAX, "%s/%s.part", localpath, fn);
				if(pm_dlfnm) {
					strncpy(pm_dlfnm, fn, PM_DLFNM_LEN);
				}
				/* drop filename extension */
				ptr = strstr(fn, PM_EXT_DB);
				if(pm_dlfnm && ptr && (ptr-fn) < PM_DLFNM_LEN) {
					pm_dlfnm[ptr-fn] = '\0';
				}
				ptr = strstr(fn, PM_EXT_PKG);
				if(ptr && (ptr-fn) < PM_DLFNM_LEN) {
					pm_dlfnm[ptr-fn] = '\0';
				}
				if(pm_dlfnm) {
					for(j = strlen(pm_dlfnm); j < PM_DLFNM_LEN; j++) {
						(pm_dlfnm)[j] = ' ';
					}
					pm_dlfnm[PM_DLFNM_LEN] = '\0';
				}
				if(pm_dloffset) {
					*pm_dloffset = 0;
				}

				/* ETA setup */
				if(pm_dlt0 && pm_dlt && pm_dlrate && pm_dlxfered1 && pm_dleta_h && pm_dleta_m && pm_dleta_s) {
					gettimeofday(pm_dlt0, NULL);
					*pm_dlt = *pm_dlt0;
					*pm_dlrate = 0;
					*pm_dlxfered1 = 0;
					*pm_dleta_h = 0;
					*pm_dleta_m = 0;
					*pm_dleta_s = 0;
				}

				if(!strcmp(server->protocol, "ftp") && !handle->proxyhost) {
					if(!FtpSize(fn, &fsz, FTPLIB_IMAGE, control)) {
						_alpm_log(PM_LOG_WARNING, _("failed to get filesize for %s\n"), fn);
					}
					/* check mtimes */
					if(mtime1) {
						char fmtime[64];
						if(!FtpModDate(fn, fmtime, sizeof(fmtime)-1, control)) {
							_alpm_log(PM_LOG_WARNING, _("failed to get mtime for %s\n"), fn);
						} else {
							_alpm_strtrim(fmtime);
							if(mtime1 && !strcmp(mtime1, fmtime)) {
								/* mtimes are identical, skip this file */
								_alpm_log(PM_LOG_DEBUG, _("mtimes are identical, skipping %s\n"), fn);
								filedone = -1;
								complete = _alpm_list_add(complete, fn);
							} else {
								if(mtime2) {
									strncpy(mtime2, fmtime, 15); /* YYYYMMDDHHMMSS (=14b) */
									mtime2[14] = '\0';
								}
							}
						}
					}
					if(!filedone) {
						if(!stat(output, &st)) {
							*pm_dloffset = (int)st.st_size;
							if(!FtpRestart(*pm_dloffset, control)) {
								_alpm_log(PM_LOG_WARNING, _("failed to resume download -- restarting\n"));
								/* can't resume: */
								/* unlink the file in order to restart download from scratch */
								unlink(output);
							}
						}
						if(!FtpGet(output, fn, FTPLIB_IMAGE, control)) {
							_alpm_log(PM_LOG_WARNING, _("\nfailed downloading %s from %s: %s\n"),
								fn, server->server, FtpLastResponse(control));
							/* we leave the partially downloaded file in place so it can be resumed later */
						} else {
							_alpm_log(PM_LOG_DEBUG, _("downloaded %s from %s\n"),
								fn, server->server);
							filedone = 1;
						}
					}
				} else if(!strcmp(server->protocol, "http") || (handle->proxyhost && strcmp(server->protocol, "file"))) {
					char src[PATH_MAX];
					char *host;
					unsigned port;
					struct tm fmtime1;
					struct tm fmtime2;
					memset(&fmtime1, 0, sizeof(struct tm));
					memset(&fmtime2, 0, sizeof(struct tm));
					if(!strcmp(server->protocol, "http") && !handle->proxyhost) {
						/* HTTP servers hang up after each request (but not proxies), so
						 * we have to re-connect for each file.
						 */
						host = (handle->proxyhost) ? handle->proxyhost : server->server;
						port = (handle->proxyhost) ? handle->proxyport : 80;
						if(strchr(host, ':')) {
							_alpm_log(PM_LOG_DEBUG, _("connecting to %s\n"), host);
						} else {
							_alpm_log(PM_LOG_DEBUG, _("connecting to %s:%u\n"), host, port);
						}
						if(!HttpConnect(host, port, &control)) {
							_alpm_log(PM_LOG_WARNING, _("cannot connect to %s\n"), host);
							continue;
						}
						/* set up our progress bar's callback (and idle timeout) */
						if(strcmp(server->protocol, "file") && control) {
							if(pm_dlcb) {
								FtpOptions(FTPLIB_CALLBACK, (long)pm_dlcb, control);
							} else {
								_alpm_log(PM_LOG_DEBUG, _("downloadfiles: progress bar's callback is not set\n"));
							}
							FtpOptions(FTPLIB_IDLETIME, (long)1000, control);
							FtpOptions(FTPLIB_CALLBACKARG, (long)&fsz, control);
							FtpOptions(FTPLIB_CALLBACKBYTES, (10*1024), control);
						}
					}

					if(!stat(output, &st)) {
						*pm_dloffset = (int)st.st_size;
					}
					if(!handle->proxyhost) {
						snprintf(src, PATH_MAX, "%s%s", server->path, fn);
					} else {
						snprintf(src, PATH_MAX, "%s://%s%s%s", server->protocol, server->server, server->path, fn);
					}
					if(mtime1 && strlen(mtime1)) {
						struct tm tmref;
						time_t t, tref;
						int diff;
						/* date conversion from YYYYMMDDHHMMSS to "rfc1123-date" */
						sscanf(mtime1, "%4d%2d%2d%2d%2d%2d",
						       &fmtime1.tm_year, &fmtime1.tm_mon, &fmtime1.tm_mday,
						       &fmtime1.tm_hour, &fmtime1.tm_min, &fmtime1.tm_sec);
						fmtime1.tm_year -= 1900;
						fmtime1.tm_mon--;
						/* compute the week day because some web servers (like lighttpd) need them. */
						/* we set tmref to "Thu, 01 Jan 1970 00:00:00" */
						memset(&tmref, 0, sizeof(struct tm));
						tmref.tm_mday = 1;
						tref = mktime(&tmref);
						/* then we compute the difference with mtime1 */
						t = mktime(&fmtime1);
						diff = ((t-tref)/3600/24)%7;
						fmtime1.tm_wday = diff+(diff >= 3 ? -3 : 4);

					}
					fmtime2.tm_year = 0;
					if(!HttpGet(server->server, output, src, &fsz, control, *pm_dloffset,
					            (mtime1) ? &fmtime1 : NULL, (mtime2) ? &fmtime2 : NULL)) {
						if(strstr(FtpLastResponse(control), "304")) {
							_alpm_log(PM_LOG_DEBUG, _("mtimes are identical, skipping %s\n"), fn);
							filedone = -1;
							complete = _alpm_list_add(complete, fn);
						} else {
							_alpm_log(PM_LOG_WARNING, _("\nfailed downloading %s from %s: %s\n"),
								src, server->server, FtpLastResponse(control));
							/* we leave the partially downloaded file in place so it can be resumed later */
						}
					} else {
						if(mtime2) {
							if(fmtime2.tm_year) {
								/* date conversion from "rfc1123-date" to YYYYMMDDHHMMSS */
								sprintf(mtime2, "%4d%02d%02d%02d%02d%02d",
								        fmtime2.tm_year+1900, fmtime2.tm_mon+1, fmtime2.tm_mday,
								        fmtime2.tm_hour, fmtime2.tm_min, fmtime2.tm_sec);
							} else {
								_alpm_log(PM_LOG_WARNING, _("failed to get mtime for %s\n"), fn);
							}
						}
						filedone = 1;
					}
				} else if(!strcmp(server->protocol, "file")) {
					char src[PATH_MAX];
					snprintf(src, PATH_MAX, "%s%s", server->path, fn);
					_alpm_log(PM_LOG_DEBUG, _("copying %s to %s/%s\n"), src, localpath, fn);
					/* local repository, just copy the file */
					if(_alpm_copyfile(src, output)) {
						_alpm_log(PM_LOG_WARNING, _("failed copying %s\n"), src);
					} else {
						filedone = 1;
					}
				}

				if(filedone > 0) {
					char completefile[PATH_MAX];
					if(!strcmp(server->protocol, "file")) {
						EVENT(handle->trans, PM_TRANS_EVT_RETRIEVE_LOCAL, pm_dlfnm, server->path);
					} else if(pm_dlcb) {
						pm_dlcb(control, fsz-*pm_dloffset, &fsz);
					}
					complete = _alpm_list_add(complete, fn);
					/* rename "output.part" file to "output" file */
					snprintf(completefile, PATH_MAX, "%s/%s", localpath, fn);
					rename(output, completefile);
				} else if(filedone < 0) {
					return(-1);
				}
			}
		}
		if(!handle->xfercommand) {
			if(!strcmp(server->protocol, "ftp") && !handle->proxyhost) {
				FtpQuit(control);
			} else if(!strcmp(server->protocol, "http") || (handle->proxyhost && strcmp(server->protocol, "file"))) {
				HttpQuit(control);
			}
		}

		if(_alpm_list_count(complete) == _alpm_list_count(files)) {
			done = 1;
		}
	}

  _alpm_log(PM_LOG_DEBUG, _("end _alpm_downloadfiles_forreal - return %d"),!done);
	return(!done);
}

char *_alpm_fetch_pkgurl(char *target)
{
	char spath[PATH_MAX];
	char url[PATH_MAX];
	char *host, *path, *fn;
	struct stat buf;

	strncpy(url, target, PATH_MAX);
	host = strstr(url, "://");
	*host = '\0';
	host += 3;
	path = strchr(host, '/');
	*path = '\0';
	path++;
	fn = strrchr(path, '/');
	if(fn) {
		*fn = '\0';
		if(path[0] == '/') {
			snprintf(spath, PATH_MAX, "%s/", path);
		} else {
			snprintf(spath, PATH_MAX, "/%s/", path);
		}
		fn++;
	} else {
		fn = path;
		strcpy(spath, "/");
	}

	/* do not download the file if it exists in the current dir
	 */
	if(stat(fn, &buf) == 0) {
		_alpm_log(PM_LOG_DEBUG, _(" %s is already in the current directory\n"), fn);
	} else {
		pmserver_t *server;
		pmlist_t *servers = NULL;
		pmlist_t *files;

		if((server = (pmserver_t *)malloc(sizeof(pmserver_t))) == NULL) {
			_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(pmserver_t));
			return(NULL);
		}
		server->protocol = url;
		server->server = host;
		server->path = spath;
		servers = _alpm_list_add(servers, server);

		files = _alpm_list_add(NULL, fn);
		if(_alpm_downloadfiles(servers, ".", files)) {
			_alpm_log(PM_LOG_WARNING, _("failed to download %s\n"), target);
			return(NULL);
		}
		FREELISTPTR(files);

		FREELIST(servers);
	}

	/* return the target with the raw filename, no URL */
	#if defined(__OpenBSD__) || defined(__APPLE__)
	return(strdup(fn));
	#else
	return(strndup(fn, PATH_MAX));
	#endif
}

/* vim: set ts=2 sw=2 noet: */
