/*
 *  download.c
 * 
 *  Copyright (c) 2002-2005 by Judd Vinet <jvinet@zeroflux.org>
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

#include "config.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <ftplib.h>

#include <alpm.h>
/* pacman */
#include "util.h"
#include "log.h"
#include "list.h"
#include "download.h"

/* progress bar */
static char sync_fnm[25];
static int offset;
static struct timeval t0, t;
static float rate;
static int xfered1;
static unsigned char eta_h, eta_m, eta_s;

/* pacman options */
extern char *pmo_proxyhost;
extern char *pmo_xfercommand;

extern unsigned short pmo_proxyport;
extern unsigned short pmo_nopassiveftp;

extern int maxcols;

static int log_progress(netbuf *ctl, int xfered, void *arg)
{
	int fsz = *(int*)arg;
	int pct = ((float)(xfered+offset) / fsz) * 100;
	int i, cur;
	struct timeval t1;
	float timediff;

	gettimeofday(&t1, NULL);
	if(xfered+offset == fsz) {
		t = t0;
	}
	timediff = t1.tv_sec-t.tv_sec + (float)(t1.tv_usec-t.tv_usec) / 1000000;

	if(xfered+offset == fsz) {
		/* average download rate */
		rate = xfered / (timediff * 1024);
		/* total download time */
		eta_s = (int)timediff;
		eta_h = eta_s / 3600;
		eta_s -= eta_h * 3600;
		eta_m = eta_s / 60;
		eta_s -= eta_m * 60;
	} else if(timediff > 1) {
		/* we avoid computing the rate & ETA on too small periods of time, so that
		   results are more significant */
		rate = (xfered-xfered1) / (timediff * 1024);
		xfered1 = xfered;
		gettimeofday(&t, NULL);
		eta_s = (fsz-(xfered+offset)) / (rate * 1024);
		eta_h = eta_s / 3600;
		eta_s -= eta_h * 3600;
		eta_m = eta_s / 60;
		eta_s -= eta_m * 60;
	}

	printf(" %s [", sync_fnm);
	cur = (int)((maxcols-64)*pct/100);
	for(i = 0; i < maxcols-64; i++) {
		(i < cur) ? printf("#") : printf(" ");
	}
	if(rate > 1000) {
		printf("] %3d%%  %6dK  %6.0fK/s  %02d:%02d:%02d\r", pct, ((xfered+offset) / 1024), rate, eta_h, eta_m, eta_s);
	} else {
		printf("] %3d%%  %6dK  %6.1fK/s  %02d:%02d:%02d\r", pct, ((xfered+offset) / 1024), rate, eta_h, eta_m, eta_s);
	}
	fflush(stdout);
	return(1);
}

static int copyfile(char *src, char *dest)
{
	FILE *in, *out;
	size_t len;
	char buf[4097];

	in = fopen(src, "r");
	if(in == NULL) {
		return(1);
	}
	out = fopen(dest, "w");
	if(out == NULL) {
		return(1);
	}

	while((len = fread(buf, 1, 4096, in))) {
		fwrite(buf, 1, len, out);
	}

	fclose(in);
	fclose(out);
	return(0);
}

/*
 * Download a list of files from a list of servers
 *   - if one server fails, we try the next one in the list
 *
 * RETURN:  0 for successful download, 1 on error
 */
int downloadfiles(list_t *servers, const char *localpath, list_t *files)
{
	return(!!downloadfiles_forreal(servers, localpath, files, NULL, NULL));
}

/*
 * This is the real downloadfiles, used directly by sync_synctree() to check
 * modtimes on remote (ftp only) files.
 *   - if *mtime1 is non-NULL, then only download files
 *     if they are different than *mtime1.  String should be in the form
 *     "YYYYMMDDHHMMSS" to match the form of ftplib's FtpModDate() function.
 *   - if *mtime2 is non-NULL, then it will be filled with the mtime
 *     of the remote FTP file (from MDTM).
 * 
 * NOTE: the *mtime option only works for FTP repositories, and won't work
 *       if XferCommand is used.  We only use it to check mtimes on the
 *       repo db files.
 *
 * RETURN:  0 for successful download
 *         -1 if the mtimes are identical
 *          1 on error
 */
int downloadfiles_forreal(list_t *servers, const char *localpath,
		list_t *files, const char *mtime1, char *mtime2)
{
	int fsz;
	netbuf *control = NULL;
	list_t *lp;
	int done = 0;
	list_t *complete = NULL;
	list_t *i;

	if(files == NULL) {
		return(0);
	}

	for(i = servers; i && !done; i = i->next) {
		server_t *server = (server_t*)i->data;

		if(!pmo_xfercommand && strcmp(server->protocol, "file")) {
			if(!strcmp(server->protocol, "ftp") && !pmo_proxyhost) {
				FtpInit();
				vprint("connecting to %s:21\n", server->server);
				if(!FtpConnect(server->server, &control)) {
					fprintf(stderr, "error: cannot connect to %s\n", server->server);
					continue;
				}
				if(!FtpLogin("anonymous", "arch@guest", control)) {
					fprintf(stderr, "error: anonymous login failed\n");
					FtpQuit(control);
					continue;
				}	
				if(!FtpChdir(server->path, control)) {
					fprintf(stderr, "error: could not cwd to %s: %s\n", server->path,
							FtpLastResponse(control));
					FtpQuit(control);
					continue;
				}
				if(!pmo_nopassiveftp) {
					if(!FtpOptions(FTPLIB_CONNMODE, FTPLIB_PASSIVE, control)) {
						fprintf(stderr, "warning: failed to set passive mode\n");
					}
				} else {
					vprint("FTP passive mode not set\n");
				}
			} else if(pmo_proxyhost) {
				char *host;
				unsigned port;
				host = (pmo_proxyhost) ? pmo_proxyhost : server->server;
				port = (pmo_proxyhost) ? pmo_proxyport : 80;
				if(strchr(host, ':')) {
					vprint("connecting to %s\n", host);
				} else {
					vprint("connecting to %s:%u\n", host, port);
				}
				if(!HttpConnect(host, port, &control)) {
					fprintf(stderr, "error: cannot connect to %s\n", host);
					continue;
				}
			}

			/* set up our progress bar's callback (and idle timeout) */
			if(strcmp(server->protocol, "file") && control) {
				FtpOptions(FTPLIB_CALLBACK, (long)log_progress, control);
				FtpOptions(FTPLIB_IDLETIME, (long)1000, control);
				FtpOptions(FTPLIB_CALLBACKARG, (long)&fsz, control);
				FtpOptions(FTPLIB_CALLBACKBYTES, (10*1024), control);
			}
		}

		/* get each file in the list */
		for(lp = files; lp; lp = lp->next) {
			char *fn = (char *)lp->data;

			if(list_is_strin(fn, complete)) {
				continue;
			}

			if(pmo_xfercommand && strcmp(server->protocol, "file")) {
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
				strncpy(origCmd, pmo_xfercommand, sizeof(origCmd));
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
					fprintf(stderr, "error: could not chdir to %s\n", localpath);
					return(1);
				}
				/* execute the parsed command via /bin/sh -c */
				vprint("running command: %s\n", parsedCmd);
				ret = system(parsedCmd);
				if(ret == -1) {
					fprintf(stderr, "error running XferCommand: fork failed!\n");
					return(1);
				} else if(ret != 0) {
					/* download failed */
					vprint("XferCommand command returned non-zero status code (%d)\n", ret);
				} else {
					/* download was successful */
					complete = list_add(complete, fn);
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
				int j, filedone = 0;
				char *ptr;
				struct stat st;
				snprintf(output, PATH_MAX, "%s/%s.part", localpath, fn);
				strncpy(sync_fnm, fn, 24);
				/* drop filename extension */
				ptr = strstr(fn, PM_EXT_DB);
				if(ptr && (ptr-fn) < 24) {
					sync_fnm[ptr-fn] = '\0';
				}
				ptr = strstr(fn, PM_EXT_PKG);
				if(ptr && (ptr-fn) < 24) {
					sync_fnm[ptr-fn] = '\0';
				}
				for(j = strlen(sync_fnm); j < 24; j++) {
					sync_fnm[j] = ' ';
				}
				sync_fnm[24] = '\0';
				offset = 0;

				/* ETA setup */
				gettimeofday(&t0, NULL);
				t = t0;
				rate = 0;
				xfered1 = 0;
				eta_h = 0;
				eta_m = 0;
				eta_s = 0;

				if(!strcmp(server->protocol, "ftp") && !pmo_proxyhost) {
					if(!FtpSize(fn, &fsz, FTPLIB_IMAGE, control)) {
						fprintf(stderr, "warning: failed to get filesize for %s\n", fn);
					}
					/* check mtimes */
					if(mtime1 || mtime2) {
						char fmtime[64];
						if(!FtpModDate(fn, fmtime, sizeof(fmtime)-1, control)) {
							fprintf(stderr, "warning: failed to get mtime for %s\n", fn);
						} else {
							strtrim(fmtime);
							if(mtime1 && !strcmp(mtime1, fmtime)) {
								/* mtimes are identical, skip this file */
								vprint("mtimes are identical, skipping %s\n", fn);
								filedone = -1;
								complete = list_add(complete, fn);
							}
							if(mtime2) {								
								strncpy(mtime2, fmtime, 15); /* YYYYMMDDHHMMSS (=14b) */
								mtime2[14] = '\0';
							}
						}
					}
					if(!filedone) {
						if(!stat(output, &st)) {
							offset = (int)st.st_size;
							if(!FtpRestart(offset, control)) {
								fprintf(stderr, "warning: failed to resume download -- restarting\n");
								/* can't resume: */
								/* unlink the file in order to restart download from scratch */
								unlink(output);
							}
						}
						if(!FtpGet(output, fn, FTPLIB_IMAGE, control)) {
							fprintf(stderr, "\nfailed downloading %s from %s: %s\n",
									fn, server->server, FtpLastResponse(control));
							/* we leave the partially downloaded file in place so it can be resumed later */
						} else {
							filedone = 1;
						}
					}
				} else if(!strcmp(server->protocol, "http") || (pmo_proxyhost && strcmp(server->protocol, "file"))) {
					char src[PATH_MAX];
					char *host;
					unsigned port;
					if(!strcmp(server->protocol, "http") && !pmo_proxyhost) {
						/* HTTP servers hang up after each request (but not proxies), so
						 * we have to re-connect for each file.
						 */
						host = (pmo_proxyhost) ? pmo_proxyhost : server->server;
						port = (pmo_proxyhost) ? pmo_proxyport : 80;
						if(strchr(host, ':')) {
							vprint("Connecting to %s\n", host);
						} else {
							vprint("Connecting to %s:%u\n", host, port);
						}
						if(!HttpConnect(host, port, &control)) {
							fprintf(stderr, "error: cannot connect to %s\n", host);
							continue;
						}
						/* set up our progress bar's callback (and idle timeout) */
						if(strcmp(server->protocol, "file") && control) {
							FtpOptions(FTPLIB_CALLBACK, (long)log_progress, control);
							FtpOptions(FTPLIB_IDLETIME, (long)1000, control);
							FtpOptions(FTPLIB_CALLBACKARG, (long)&fsz, control);
							FtpOptions(FTPLIB_CALLBACKBYTES, (10*1024), control);
						}
					}

					if(!stat(output, &st)) {
						offset = (int)st.st_size;
					}
					if(!pmo_proxyhost) {
						snprintf(src, PATH_MAX, "%s%s", server->path, fn);
					} else {
						snprintf(src, PATH_MAX, "%s://%s%s%s", server->protocol, server->server, server->path, fn);
					}
					if(!HttpGet(server->server, output, src, &fsz, control, offset)) {
						fprintf(stderr, "\nfailed downloading %s from %s: %s\n",
								src, server->server, FtpLastResponse(control));
						/* we leave the partially downloaded file in place so it can be resumed later */
					} else {
						filedone = 1;
					}
				} else if(!strcmp(server->protocol, "file")) {
					char src[PATH_MAX];
					snprintf(src, PATH_MAX, "%s%s", server->path, fn);
					vprint("copying %s to %s/%s\n", src, localpath, fn);
					/* local repository, just copy the file */
					if(copyfile(src, output)) {
						fprintf(stderr, "failed copying %s\n", src);
					} else {
						filedone = 1;
					}
				}

				if(filedone > 0) {
					char completefile[PATH_MAX];
					if(!strcmp(server->protocol, "file")) {
						char out[56];
						printf(" %s [", sync_fnm);
						strncpy(out, server->path, 33);
						printf("%s", out);
						for(j = strlen(out); j < maxcols-64; j++) {
							printf(" ");
						}
						fputs("] 100%    LOCAL ", stdout);
					} else {
						log_progress(control, fsz-offset, &fsz);
					}
					complete = list_add(complete, fn);
					/* rename "output.part" file to "output" file */
					snprintf(completefile, PATH_MAX, "%s/%s", localpath, fn);
					rename(output, completefile);
				} else if(filedone < 0) {
					return(-1);
				}
				printf("\n");
				fflush(stdout);
			}
		}
		if(!pmo_xfercommand) {
			if(!strcmp(server->protocol, "ftp") && !pmo_proxyhost) {
				FtpQuit(control);
			} else if(!strcmp(server->protocol, "http") || (pmo_proxyhost && strcmp(server->protocol, "file"))) {
				HttpQuit(control);
			}
		}

		if(list_count(complete) == list_count(files)) {
			done = 1;
		}
	}

	return(!done);
}

char *fetch_pkgurl(char *target)
{
	char spath[PATH_MAX];
	char url[PATH_MAX];
	server_t *server;
	list_t *servers = NULL;
	list_t *files = NULL;
	char *host, *path, *fn;

	/* ORE
	do not download the file if it exists in the current dir */

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

	server = (server_t *)malloc(sizeof(server_t));
	if(server == NULL) {
		fprintf(stderr, "error: failed to allocate %d bytes\n", sizeof(server_t));
		return(NULL);
	}
	server->protocol = url;
	server->server = host;
	server->path = spath;
	servers = list_add(servers, server);

	files = list_add(files, fn);
	if(downloadfiles(servers, ".", files)) {
		fprintf(stderr, "error: failed to download %s\n", target);
		return(NULL);
	}

	FREELIST(servers);
	files->data = NULL;
	FREELIST(files);

	/* return the target with the raw filename, no URL */
	return(strndup(fn, PATH_MAX));
}

/* vim: set ts=2 sw=2 noet: */
