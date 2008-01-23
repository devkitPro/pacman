/*
 *  download.c
 *
 *  Copyright (c) 2002-2008 by Judd Vinet <jvinet@zeroflux.org>
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
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <download.h> /* libdownload */

/* libalpm */
#include "dload.h"
#include "alpm_list.h"
#include "alpm.h"
#include "log.h"
#include "util.h"
#include "error.h"
#include "handle.h"
#include "server.h"

/* remove filename info from "s_url->doc" and return it */
static char *strip_filename(pmserver_t *server)
{
	char *p = NULL, *fname = NULL;
	if(!server) {
		return(NULL);
	}

	p = strrchr(server->s_url->doc, '/');
	if(p && *(++p)) {
		fname = strdup(p);
		_alpm_log(PM_LOG_DEBUG, "stripping '%s' from '%s'\n",
				fname, server->s_url->doc);
		*p = 0;
	}

	/* s_url->doc now contains ONLY path information.  return value
	 * if the file information from the original URL */
	return(fname);
}


/* Return a 'struct url' for this server, for downloading 'filename'. */
static struct url *url_for_file(pmserver_t *server, const char *filename)
{
	struct url *ret = NULL;
	char *doc = NULL;
	int doclen = 0;

	doclen = strlen(server->s_url->doc) + strlen(filename) + 2;
	CALLOC(doc, doclen, sizeof(char), RET_ERR(PM_ERR_MEMORY, NULL));

	snprintf(doc, doclen, "%s/%s", server->s_url->doc, filename);
	ret = downloadMakeURL(server->s_url->scheme,
												server->s_url->host,
												server->s_url->port,
												doc,
												server->s_url->user,
												server->s_url->pwd);
	FREE(doc);
	return(ret);
}

/* TODO temporary private declaration */
int _alpm_downloadfiles_forreal(alpm_list_t *servers, const char *localpath,
	alpm_list_t *files, time_t mtime1, time_t *mtime2, int *dl_total,
	unsigned long totalsize);


/* TODO implement these as real functions */
int _alpm_download_single_file(const char *filename,
		alpm_list_t *servers, const char *localpath,
		time_t mtimeold, time_t *mtimenew)
{
	alpm_list_t *files = NULL;
	int ret;

	/* make a temp one element list */
	files = alpm_list_add(files, (char*)filename);

	ret = _alpm_downloadfiles_forreal(servers, localpath,
			files, mtimeold, mtimenew, NULL, 0);

	/* free list (data was NOT duplicated) */
	alpm_list_free(files);
	return(ret);
}

int _alpm_download_files(alpm_list_t *files,
		alpm_list_t *servers, const char *localpath)
{
	int ret;

	ret = _alpm_downloadfiles_forreal(servers, localpath,
			files, 0, NULL, NULL, 0);

	return(ret);
}


/*
 * This is the real downloadfiles, used directly by sync_synctree() to check
 * modtimes on remote files.
 *   - if mtime1 is non-NULL, then only download files if they are different
 *     than mtime1.
 *   - if *mtime2 is non-NULL, it will be filled with the mtime of the remote
 *     file.
 *   - if *dl_total is non-NULL, then it will be used as the starting
 *     download amount when TotalDownload is set. It will also be
 *     set to the final download amount for the calling function to use.
 *   - totalsize is the total download size for use when TotalDownload
 *     is set. Use 0 if the total download size is not known.
 *
 * RETURN:  0 for successful download
 *          1 if the mtimes are identical
 *         -1 on error
 */
int _alpm_downloadfiles_forreal(alpm_list_t *servers, const char *localpath,
	alpm_list_t *files, time_t mtime1, time_t *mtime2, int *dl_total,
	unsigned long totalsize)
{
	int dl_thisfile = 0;
	alpm_list_t *lp;
	int done = 0;
	alpm_list_t *complete = NULL;
	alpm_list_t *i;

	ALPM_LOG_FUNC;

	if(files == NULL) {
		return(0);
	}

	for(i = servers; i && !done; i = i->next) {
		pmserver_t *server = i->data;

		/* get each file in the list */
		for(lp = files; lp; lp = lp->next) {
			struct url *fileurl = NULL;
			char realfile[PATH_MAX];
			char output[PATH_MAX];
			char *fn = (char *)lp->data;
			char *pkgname;

			fileurl = url_for_file(server, fn);
			if(!fileurl) {
				return(-1);
			}

			/* pass the raw filename for passing to the callback function */
			STRDUP(pkgname, fn, (void)0);
			_alpm_log(PM_LOG_DEBUG, "using '%s' for download progress\n", pkgname);

			snprintf(realfile, PATH_MAX, "%s%s", localpath, fn);
			snprintf(output, PATH_MAX, "%s%s.part", localpath, fn);

			if(alpm_list_find_str(complete, fn)) {
				continue;
			}

			if(!handle->xfercommand
					|| !strcmp(fileurl->scheme, "file")) {
				FILE *dlf, *localf = NULL;
				struct url_stat ust;
				struct stat st;
				int chk_resume = 0;

				if(stat(output, &st) == 0 && st.st_size > 0) {
					_alpm_log(PM_LOG_DEBUG, "existing file found, using it\n");
					fileurl->offset = (off_t)st.st_size;
					dl_thisfile = st.st_size;
					if (dl_total != NULL) {
						*dl_total += st.st_size;
					}
					localf = fopen(output, "a");
					chk_resume = 1;
				} else {
					fileurl->offset = (off_t)0;
					dl_thisfile = 0;
				}

				/* libdownload does not reset the error code, reset it in
				 * the case of previous errors */
				downloadLastErrCode = 0;

				/* 10s timeout - TODO make a config option */
				downloadTimeout = 10000;

				dlf = downloadXGet(fileurl, &ust, (handle->nopassiveftp ? "" : "p"));

				if(downloadLastErrCode != 0 || dlf == NULL) {
					const char *host = _("disk");
					if(strcmp(SCHEME_FILE, fileurl->scheme) != 0) {
						host = fileurl->host;
					}
					_alpm_log(PM_LOG_ERROR, _("failed retrieving file '%s' from %s : %s\n"),
										fn, host, downloadLastErrString);
					if(localf != NULL) {
						fclose(localf);
					}
					/* try the next server */
					downloadFreeURL(fileurl);
					continue;
				} else {
						_alpm_log(PM_LOG_DEBUG, "connected to %s successfully\n", fileurl->host);
				}

				if(ust.mtime && mtime1 && ust.mtime == mtime1) {
					_alpm_log(PM_LOG_DEBUG, "mtimes are identical, skipping %s\n", fn);
					complete = alpm_list_add(complete, fn);
					if(localf != NULL) {
						fclose(localf);
					}
					if(dlf != NULL) {
						fclose(dlf);
					}
					downloadFreeURL(fileurl);
					return(1);
				}

				if(ust.mtime && mtime2) {
					*mtime2 = ust.mtime;
				}

				if(chk_resume && fileurl->offset == 0) {
					_alpm_log(PM_LOG_WARNING, _("cannot resume download, starting over\n"));
					if(localf != NULL) {
						fclose(localf);
						localf = NULL;
					}
				}

				if(localf == NULL) {
					_alpm_rmrf(output);
					fileurl->offset = (off_t)0;
					dl_thisfile = 0;
					localf = fopen(output, "w");
					if(localf == NULL) { /* still null? */
						_alpm_log(PM_LOG_ERROR, _("cannot write to file '%s'\n"), output);
						if(dlf != NULL) {
							fclose(dlf);
						}
						downloadFreeURL(fileurl);
						return(-1);
					}
				}

				/* Progress 0 - initialize */
				if(handle->dlcb) {
					handle->dlcb(pkgname, 0, ust.size, dl_total ? *dl_total : 0,
							totalsize);
				}

				int nread = 0;
				char buffer[PM_DLBUF_LEN];
				while((nread = fread(buffer, 1, PM_DLBUF_LEN, dlf)) > 0) {
					if(ferror(dlf)) {
						_alpm_log(PM_LOG_ERROR, _("error downloading '%s': %s\n"),
											fn, downloadLastErrString);
						fclose(localf);
						fclose(dlf);
						downloadFreeURL(fileurl);
						return(-1);
					}

					int nwritten = 0;
					while(nwritten < nread) {
						nwritten += fwrite(buffer, 1, (nread - nwritten), localf);
						if(ferror(localf)) {
							_alpm_log(PM_LOG_ERROR, _("error writing to file '%s': %s\n"),
												realfile, strerror(errno));
							fclose(localf);
							fclose(dlf);
							downloadFreeURL(fileurl);
							return(-1);
						}
					}

					if(nwritten != nread) {

					}
					dl_thisfile += nread;
					if (dl_total != NULL) {
						*dl_total += nread;
					}

					if(handle->dlcb) {
						handle->dlcb(pkgname, dl_thisfile, ust.size,
								dl_total ? *dl_total : 0, totalsize);
					}
				}

				downloadFreeURL(fileurl);
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
				snprintf(url, PATH_MAX, "%s://%s%s", fileurl->scheme,
						fileurl->host, fileurl->doc);
				/* we don't need this anymore */
				downloadFreeURL(fileurl);

				/* replace all occurrences of %o with fn.part */
				strncpy(origCmd, handle->xfercommand, sizeof(origCmd));
				ptr1 = origCmd;
				while((ptr2 = strstr(ptr1, "%o"))) {
					usepart = 1;
					ptr2[0] = '\0';
					strcat(parsedCmd, ptr1);
					strcat(parsedCmd, output);
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
				_alpm_log(PM_LOG_DEBUG, "running command: %s\n", parsedCmd);
				ret = system(parsedCmd);
				if(ret == -1) {
					_alpm_log(PM_LOG_WARNING, _("running XferCommand: fork failed!\n"));
					return(PM_ERR_FORK_FAILED);
				} else if(ret != 0) {
					/* download failed */
					_alpm_log(PM_LOG_DEBUG, "XferCommand command returned non-zero status code (%d)\n", ret);
				} else {
					/* download was successful */
					complete = alpm_list_add(complete, fn);
					if(usepart) {
						rename(output, realfile);
					}
				}
				chdir(cwd);
			}
			FREE(pkgname);
		}

		if(alpm_list_count(complete) == alpm_list_count(files)) {
			done = 1;
		}
	}
	alpm_list_free(complete);

	return(done ? 0 : -1);
}

/** Fetch a remote pkg.
 * @param url URL of the package to download
 * @return the downloaded filepath on success, NULL on error
 * @addtogroup alpm_misc
 */
char SYMEXPORT *alpm_fetch_pkgurl(const char *url)
{
	pmserver_t *server;
	char *filename, *filepath;
	const char *cachedir;

	ALPM_LOG_FUNC;

	if(strstr(url, "://") == NULL) {
		_alpm_log(PM_LOG_DEBUG, "Invalid URL passed to alpm_fetch_pkgurl\n");
		return(NULL);
	}

	server = _alpm_server_new(url);
	if(!server) {
		return(NULL);
	}

	/* strip path information from the filename */
	filename = strip_filename(server);
	if(!filename) {
		_alpm_log(PM_LOG_ERROR, _("URL does not contain a file for download\n"));
		return(NULL);
	}

	/* find a valid cache dir to download to */
	cachedir = _alpm_filecache_setup();

	/* TODO this seems like needless complexity just to download one file */
	alpm_list_t *servers = alpm_list_add(NULL, server);

	/* download the file */
	if(_alpm_download_single_file(filename, servers, cachedir, 0, NULL)) {
		_alpm_log(PM_LOG_WARNING, _("failed to download %s\n"), url);
		return(NULL);
	}
	_alpm_log(PM_LOG_DEBUG, "successfully downloaded %s\n", filename);
	alpm_list_free(servers);
	_alpm_server_free(server);

	/* we should be able to find the file the second time around */
	filepath = _alpm_filecache_find(filename);
	return(filepath);
}

/* vim: set ts=2 sw=2 noet: */
