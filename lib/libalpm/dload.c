/*
 *  download.c
 *
 *  Copyright (c) 2006-2009 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
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
#include <signal.h>
#include <limits.h>
/* the following two are needed on BSD for libfetch */
#if defined(HAVE_SYS_SYSLIMITS_H)
#include <sys/syslimits.h> /* PATH_MAX */
#endif
#if defined(HAVE_SYS_PARAM_H)
#include <sys/param.h> /* MAXHOSTNAMELEN */
#endif

#if defined(INTERNAL_DOWNLOAD)
#include <fetch.h>
#endif

/* libalpm */
#include "dload.h"
#include "alpm_list.h"
#include "alpm.h"
#include "log.h"
#include "util.h"
#include "handle.h"

static char *get_filename(const char *url) {
	char *filename = strrchr(url, '/');
	if(filename != NULL) {
		filename++;
	}
	return(filename);
}

#if defined(INTERNAL_DOWNLOAD)
static char *get_destfile(const char *path, const char *filename) {
	char *destfile;
	/* len = localpath len + filename len + null */
	int len = strlen(path) + strlen(filename) + 1;
	CALLOC(destfile, len, sizeof(char), RET_ERR(PM_ERR_MEMORY, NULL));
	snprintf(destfile, len, "%s%s", path, filename);

	return(destfile);
}

static char *get_tempfile(const char *path, const char *filename) {
	char *tempfile;
	/* len = localpath len + filename len + '.part' len + null */
	int len = strlen(path) + strlen(filename) + 6;
	CALLOC(tempfile, len, sizeof(char), RET_ERR(PM_ERR_MEMORY, NULL));
	snprintf(tempfile, len, "%s%s.part", path, filename);

	return(tempfile);
}

/* Build a 'struct url' from an url. */
static struct url *url_for_string(const char *url)
{
	struct url *ret = NULL;
	ret = fetchParseURL(url);
	if(!ret) {
		_alpm_log(PM_LOG_ERROR, _("url '%s' is invalid\n"), url);
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

static int download_internal(const char *url, const char *localpath,
		time_t mtimeold, time_t *mtimenew) {
	fetchIO *dlf = NULL;
	FILE *localf = NULL;
	struct url_stat ust;
	struct stat st;
	int chk_resume = 0, ret = 0;
	size_t dl_thisfile = 0, nread = 0;
	char *tempfile, *destfile, *filename;
	struct sigaction new_action, old_action;
	struct url *fileurl = url_for_string(url);
	char buffer[PM_DLBUF_LEN];

	if(!fileurl) {
		return(-1);
	}

	filename = get_filename(url);
	if(!filename) {
		return(-1);
	}
	destfile = get_destfile(localpath, filename);
	tempfile = get_tempfile(localpath, filename);

	if(mtimeold) {
		fileurl->last_modified = mtimeold;
	}

	/* pass the raw filename for passing to the callback function */
	_alpm_log(PM_LOG_DEBUG, "using '%s' for download progress\n", filename);

	if(stat(tempfile, &st) == 0 && st.st_size > 0) {
		_alpm_log(PM_LOG_DEBUG, "existing file found, using it\n");
		fileurl->offset = (off_t)st.st_size;
		dl_thisfile = st.st_size;
		localf = fopen(tempfile, "ab");
		chk_resume = 1;
	} else {
		fileurl->offset = (off_t)0;
		dl_thisfile = 0;
	}

	/* print proxy info for debug purposes */
	_alpm_log(PM_LOG_DEBUG, "HTTP_PROXY: %s\n", getenv("HTTP_PROXY"));
	_alpm_log(PM_LOG_DEBUG, "http_proxy: %s\n", getenv("http_proxy"));
	_alpm_log(PM_LOG_DEBUG, "FTP_PROXY:  %s\n", getenv("FTP_PROXY"));
	_alpm_log(PM_LOG_DEBUG, "ftp_proxy:  %s\n", getenv("ftp_proxy"));

	/* libfetch does not reset the error code */
	fetchLastErrCode = 0;

	/* 10s timeout - TODO make a config option */
	fetchTimeout = 10000;

	/* ignore any SIGPIPE signals- these may occur if our FTP socket dies or
	 * something along those lines. Store the old signal handler first. */
	new_action.sa_handler = SIG_IGN;
	sigemptyset(&new_action.sa_mask);
	sigaction(SIGPIPE, NULL, &old_action);
	sigaction(SIGPIPE, &new_action, NULL);

	dlf = fetchXGet(fileurl, &ust, (handle->nopassiveftp ? "i" : "pi"));

	if(fetchLastErrCode == FETCH_UNCHANGED) {
		_alpm_log(PM_LOG_DEBUG, "mtimes are identical, skipping %s\n", filename);
		ret = 1;
		goto cleanup;
	}

	if(fetchLastErrCode != 0 || dlf == NULL) {
		const char *host = _("disk");
		if(strcmp(SCHEME_FILE, fileurl->scheme) != 0) {
			host = fileurl->host;
		}
		pm_errno = PM_ERR_LIBFETCH;
		_alpm_log(PM_LOG_ERROR, _("failed retrieving file '%s' from %s : %s\n"),
				filename, host, fetchLastErrString);
		ret = -1;
		goto cleanup;
	} else {
		_alpm_log(PM_LOG_DEBUG, "connected to %s successfully\n", fileurl->host);
	}

	if(ust.mtime && mtimenew) {
		*mtimenew = ust.mtime;
	}

	if(chk_resume && fileurl->offset == 0) {
		_alpm_log(PM_LOG_WARNING, _("cannot resume download, starting over\n"));
		if(localf != NULL) {
			fclose(localf);
			localf = NULL;
		}
	}

	if(localf == NULL) {
		_alpm_rmrf(tempfile);
		fileurl->offset = (off_t)0;
		dl_thisfile = 0;
		localf = fopen(tempfile, "wb");
		if(localf == NULL) { /* still null? */
			_alpm_log(PM_LOG_ERROR, _("cannot write to file '%s'\n"), tempfile);
			ret = -1;
			goto cleanup;
		}
	}

	/* Progress 0 - initialize */
	if(handle->dlcb) {
		handle->dlcb(filename, 0, ust.size);
	}

	while((nread = fetchIO_read(dlf, buffer, PM_DLBUF_LEN)) > 0) {
		size_t nwritten = 0;
		nwritten = fwrite(buffer, 1, nread, localf);
		if((nwritten != nread) || ferror(localf)) {
			_alpm_log(PM_LOG_ERROR, _("error writing to file '%s': %s\n"),
					destfile, strerror(errno));
			ret = -1;
			goto cleanup;
		}
		dl_thisfile += nread;

		if(handle->dlcb) {
			handle->dlcb(filename, dl_thisfile, ust.size);
		}
	}

	/* did the transfer complete normally? */
	if (ust.size != -1 && dl_thisfile < ust.size) {
		pm_errno = PM_ERR_LIBFETCH;
		_alpm_log(PM_LOG_ERROR, _("%s appears to be truncated: %jd/%jd bytes\n"),
				filename, (intmax_t)dl_thisfile, (intmax_t)ust.size);
		ret = -1;
		goto cleanup;
	}

	/* probably safer to close the file descriptors now before renaming the file,
	 * for example to make sure the buffers are flushed.
	 */
	fclose(localf);
	localf = NULL;
	fetchIO_close(dlf);
	dlf = NULL;

	rename(tempfile, destfile);
	ret = 0;

cleanup:
	/* restore any existing SIGPIPE signal handler */
	sigaction(SIGPIPE, &old_action, NULL);

	FREE(tempfile);
	FREE(destfile);
	if(localf != NULL) {
		fclose(localf);
	}
	if(dlf != NULL) {
		fetchIO_close(dlf);
	}
	fetchFreeURL(fileurl);
	return(ret);
}
#endif

static int download(const char *url, const char *localpath,
		time_t mtimeold, time_t *mtimenew) {
	if(handle->fetchcb == NULL) {
#if defined(INTERNAL_DOWNLOAD)
		return(download_internal(url, localpath, mtimeold, mtimenew));
#else
		RET_ERR(PM_ERR_EXTERNAL_DOWNLOAD, -1);
#endif
	} else {
		int ret = handle->fetchcb(url, localpath, mtimeold, mtimenew);
		if(ret == -1) {
			RET_ERR(PM_ERR_EXTERNAL_DOWNLOAD, -1);
		}
		return(ret);
	}
}

/*
 * Download a single file
 *   - if mtimeold is non-NULL, then only download the file if it's different
 *     than mtimeold.
 *   - if *mtimenew is non-NULL, it will be filled with the mtime of the remote
 *     file.
 *   - servers must be a list of urls WITHOUT trailing slashes.
 *
 * RETURN:  0 for successful download
 *          1 if the mtimes are identical
 *         -1 on error
 */
int _alpm_download_single_file(const char *filename,
		alpm_list_t *servers, const char *localpath,
		time_t mtimeold, time_t *mtimenew)
{
	alpm_list_t *i;
	int ret = -1;

	ASSERT(servers != NULL, RET_ERR(PM_ERR_SERVER_NONE, -1));

	for(i = servers; i; i = i->next) {
		const char *server = i->data;
		char *fileurl = NULL;
		int len;

		/* print server + filename into a buffer */
		len = strlen(server) + strlen(filename) + 2;
		CALLOC(fileurl, len, sizeof(char), RET_ERR(PM_ERR_MEMORY, -1));
		snprintf(fileurl, len, "%s/%s", server, filename);

		ret = download(fileurl, localpath, mtimeold, mtimenew);
		FREE(fileurl);
		if(ret != -1) {
			break;
		}
	}

	return(ret);
}

int _alpm_download_files(alpm_list_t *files,
		alpm_list_t *servers, const char *localpath)
{
	int ret = 0;
	alpm_list_t *lp;

	for(lp = files; lp; lp = lp->next) {
		char *filename = lp->data;
		if(_alpm_download_single_file(filename, servers,
					localpath, 0, NULL) == -1) {
			ret++;
		}
	}

	return(ret);
}

/** Fetch a remote pkg.
 * @param url URL of the package to download
 * @return the downloaded filepath on success, NULL on error
 * @addtogroup alpm_misc
 */
char SYMEXPORT *alpm_fetch_pkgurl(const char *url)
{
	char *filename, *filepath;
	const char *cachedir;
	int ret;

	ALPM_LOG_FUNC;

	filename = get_filename(url);

	/* find a valid cache dir to download to */
	cachedir = _alpm_filecache_setup();

	/* download the file */
	ret = download(url, cachedir, 0, NULL);
	if(ret == -1) {
		_alpm_log(PM_LOG_WARNING, _("failed to download %s\n"), url);
		return(NULL);
	}
	_alpm_log(PM_LOG_DEBUG, "successfully downloaded %s\n", url);

	/* we should be able to find the file the second time around */
	filepath = _alpm_filecache_find(filename);
	return(filepath);
}

/* vim: set ts=2 sw=2 noet: */
