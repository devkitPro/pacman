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

/* Return a 'struct url' for this server, for downloading 'filename'. */
static struct url *url_for_file(const char *url, const char *filename)
{
	struct url *ret = NULL;
	char *buf = NULL;
	int len;

	/* print url + filename into a buffer */
	len = strlen(url) + strlen(filename) + 2;
	CALLOC(buf, len, sizeof(char), RET_ERR(PM_ERR_MEMORY, NULL));
	snprintf(buf, len, "%s/%s", url, filename);

	ret = downloadParseURL(buf);
	FREE(buf);
	if(!ret) {
		_alpm_log(PM_LOG_ERROR, _("url '%s' is invalid\n"), buf);
		RET_ERR(PM_ERR_SERVER_BAD_URL, NULL);
	}

	/* if no URL scheme specified, assume HTTP */
	if(strlen(ret->scheme) == 0) {
		_alpm_log(PM_LOG_WARNING, _("url scheme not specified, assuming HTTP\n"));
		strcpy(ret->scheme, SCHEME_HTTP);
	}
	/* add a user & password for anonymous FTP */
	if(strcmp(ret->scheme,SCHEME_FTP) == 0 && strlen(ret->user) == 0) {
		strcpy(ret->user, "anonymous");
		strcpy(ret->pwd, "libalpm@guest");
	}

	return(ret);
}

/* TODO temporary private declaration */
int _alpm_downloadfiles_forreal(alpm_list_t *servers, const char *localpath,
	alpm_list_t *files, time_t mtime1, time_t *mtime2);


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
			files, mtimeold, mtimenew);

	/* free list (data was NOT duplicated) */
	alpm_list_free(files);
	return(ret);
}

int _alpm_download_files(alpm_list_t *files,
		alpm_list_t *servers, const char *localpath)
{
	int ret;

	ret = _alpm_downloadfiles_forreal(servers, localpath,
			files, 0, NULL);

	return(ret);
}


/*
 * This is the real downloadfiles, used directly by sync_synctree() to check
 * modtimes on remote files.
 *   - if mtime1 is non-NULL, then only download files if they are different
 *     than mtime1.
 *   - if *mtime2 is non-NULL, it will be filled with the mtime of the remote
 *     file.
 *
 * RETURN:  0 for successful download
 *          1 if the mtimes are identical
 *         -1 on error
 */
int _alpm_downloadfiles_forreal(alpm_list_t *servers, const char *localpath,
	alpm_list_t *files, time_t mtime1, time_t *mtime2)
{
	int dl_thisfile = 0;
	alpm_list_t *lp;
	alpm_list_t *complete = NULL;
	alpm_list_t *i;
	int ret = -1;
	char *pkgname = NULL;

	ALPM_LOG_FUNC;

	if(files == NULL) {
		return(0);
	}

	for(i = servers; i; i = i->next) {
		const char *server = i->data;

		/* get each file in the list */
		for(lp = files; lp; lp = lp->next) {
			struct url *fileurl = NULL;
			char realfile[PATH_MAX];
			char output[PATH_MAX];
			char *fn = (char *)lp->data;

			fileurl = url_for_file(server, fn);
			if(!fileurl) {
				goto cleanup;
			}

			/* pass the raw filename for passing to the callback function */
			FREE(pkgname);
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
					ret = 1;
					goto cleanup;
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
						goto cleanup;
					}
				}

				/* Progress 0 - initialize */
				if(handle->dlcb) {
					handle->dlcb(pkgname, 0, ust.size);
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
						goto cleanup;
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
							goto cleanup;
						}
					}

					if(nwritten != nread) {

					}
					dl_thisfile += nread;

					if(handle->dlcb) {
						handle->dlcb(pkgname, dl_thisfile, ust.size);
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
					pm_errno = PM_ERR_CONNECT_FAILED;
					goto cleanup;
				}
				/* execute the parsed command via /bin/sh -c */
				_alpm_log(PM_LOG_DEBUG, "running command: %s\n", parsedCmd);
				ret = system(parsedCmd);
				if(ret == -1) {
					_alpm_log(PM_LOG_WARNING, _("running XferCommand: fork failed!\n"));
					pm_errno = PM_ERR_FORK_FAILED;
					goto cleanup;
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
		}

		if(alpm_list_count(complete) == alpm_list_count(files)) {
			ret = 0;
			goto cleanup;
		}
	}

cleanup:
	FREE(pkgname);
	alpm_list_free(complete);

	return(ret);
}

/** Fetch a remote pkg.
 * @param url URL of the package to download
 * @return the downloaded filepath on success, NULL on error
 * @addtogroup alpm_misc
 */
char SYMEXPORT *alpm_fetch_pkgurl(const char *url)
{
	/* TODO this method will not work at all right now */
	char *filename, *filepath;
	const char *cachedir;

	ALPM_LOG_FUNC;

	filename = NULL;

	/* find a valid cache dir to download to */
	cachedir = _alpm_filecache_setup();

	/* download the file */
	if(_alpm_download_single_file(NULL, NULL, cachedir, 0, NULL)) {
		_alpm_log(PM_LOG_WARNING, _("failed to download %s\n"), url);
		return(NULL);
	}
	_alpm_log(PM_LOG_DEBUG, "successfully downloaded %s\n", url);

	/* we should be able to find the file the second time around */
	filepath = _alpm_filecache_find(filename);
	return(filepath);
}

/* vim: set ts=2 sw=2 noet: */
