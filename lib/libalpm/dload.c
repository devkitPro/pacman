/*
 *  download.c
 *
 *  Copyright (c) 2006-2010 Pacman Development Team <pacman-dev@archlinux.org>
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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <limits.h>
/* the following two are needed on BSD for libfetch */
#if defined(HAVE_SYS_SYSLIMITS_H)
#include <sys/syslimits.h> /* PATH_MAX */
#endif
#if defined(HAVE_SYS_PARAM_H)
#include <sys/param.h> /* MAXHOSTNAMELEN */
#endif

#ifdef HAVE_LIBFETCH
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

#ifdef HAVE_LIBFETCH
static char *get_destfile(const char *path, const char *filename) {
	char *destfile;
	/* len = localpath len + filename len + null */
	size_t len = strlen(path) + strlen(filename) + 1;
	CALLOC(destfile, len, sizeof(char), RET_ERR(PM_ERR_MEMORY, NULL));
	snprintf(destfile, len, "%s%s", path, filename);

	return(destfile);
}

static char *get_tempfile(const char *path, const char *filename) {
	char *tempfile;
	/* len = localpath len + filename len + '.part' len + null */
	size_t len = strlen(path) + strlen(filename) + 6;
	CALLOC(tempfile, len, sizeof(char), RET_ERR(PM_ERR_MEMORY, NULL));
	snprintf(tempfile, len, "%s%s.part", path, filename);

	return(tempfile);
}

static const char *gethost(struct url *fileurl)
{
	const char *host = _("disk");
	if(strcmp(SCHEME_FILE, fileurl->scheme) != 0) {
		host = fileurl->host;
	}
	return(host);
}

int dload_interrupted;
static RETSIGTYPE inthandler(int signum)
{
	dload_interrupted = 1;
}

#define check_stop() if(dload_interrupted) { ret = -1; goto cleanup; }
enum sighandlers { OLD = 0, NEW = 1 };

static int download_internal(const char *url, const char *localpath,
		int force) {
	FILE *localf = NULL;
	struct stat st;
	int ret = 0;
	off_t dl_thisfile = 0;
	ssize_t nread = 0;
	char *tempfile, *destfile, *filename;
	struct sigaction sig_pipe[2], sig_int[2];

	off_t local_size = 0;
	time_t local_time = 0;

	struct url *fileurl;
	struct url_stat ust;
	fetchIO *dlf = NULL;

	char buffer[PM_DLBUF_LEN];

	filename = get_filename(url);
	if(!filename) {
		_alpm_log(PM_LOG_ERROR, _("url '%s' is invalid\n"), url);
		RET_ERR(PM_ERR_SERVER_BAD_URL, -1);
	}

	fileurl = fetchParseURL(url);
	if(!fileurl) {
		_alpm_log(PM_LOG_ERROR, _("url '%s' is invalid\n"), url);
		RET_ERR(PM_ERR_LIBFETCH, -1);
	}

	destfile = get_destfile(localpath, filename);
	tempfile = get_tempfile(localpath, filename);

	if(stat(tempfile, &st) == 0 && st.st_size > 0) {
		_alpm_log(PM_LOG_DEBUG, "tempfile found, attempting continuation\n");
		local_time = fileurl->last_modified = st.st_mtime;
		local_size = fileurl->offset = (off_t)st.st_size;
		dl_thisfile = st.st_size;
		localf = fopen(tempfile, "ab");
	} else if(!force && stat(destfile, &st) == 0 && st.st_size > 0) {
		_alpm_log(PM_LOG_DEBUG, "destfile found, using mtime only\n");
		local_time = fileurl->last_modified = st.st_mtime;
		local_size = /* no fu->off here */ (off_t)st.st_size;
	} else {
		_alpm_log(PM_LOG_DEBUG, "no file found matching criteria, starting from scratch\n");
	}

	/* pass the raw filename for passing to the callback function */
	_alpm_log(PM_LOG_DEBUG, "using '%s' for download progress\n", filename);

	/* print proxy info for debug purposes */
	_alpm_log(PM_LOG_DEBUG, "HTTP_PROXY: %s\n", getenv("HTTP_PROXY"));
	_alpm_log(PM_LOG_DEBUG, "http_proxy: %s\n", getenv("http_proxy"));
	_alpm_log(PM_LOG_DEBUG, "FTP_PROXY:  %s\n", getenv("FTP_PROXY"));
	_alpm_log(PM_LOG_DEBUG, "ftp_proxy:  %s\n", getenv("ftp_proxy"));

	/* 10s timeout */
	fetchTimeout = 10;

	/* ignore any SIGPIPE signals- these may occur if our FTP socket dies or
	 * something along those lines. Store the old signal handler first. */
	sig_pipe[NEW].sa_handler = SIG_IGN;
	sigemptyset(&sig_pipe[NEW].sa_mask);
	sig_pipe[NEW].sa_flags = 0;
	sigaction(SIGPIPE, NULL, &sig_pipe[OLD]);
	sigaction(SIGPIPE, &sig_pipe[NEW], NULL);

	dload_interrupted = 0;
	sig_int[NEW].sa_handler = &inthandler;
	sigemptyset(&sig_int[NEW].sa_mask);
	sig_int[NEW].sa_flags = 0;
	sigaction(SIGINT, NULL, &sig_int[OLD]);
	sigaction(SIGINT, &sig_int[NEW], NULL);

	/* NOTE: libfetch does not reset the error code, be sure to do it before
	 * calls into the library */

	/* find out the remote size *and* mtime in one go. there is a lot of
	 * trouble in trying to do both size and "if-modified-since" logic in a
	 * non-stat request, so avoid it. */
	fetchLastErrCode = 0;
	if(fetchStat(fileurl, &ust, "") == -1) {
		pm_errno = PM_ERR_LIBFETCH;
		_alpm_log(PM_LOG_ERROR, _("failed retrieving file '%s' from %s : %s\n"),
				filename, gethost(fileurl), fetchLastErrString);
		ret = -1;
		goto cleanup;
	}
	check_stop();

	_alpm_log(PM_LOG_DEBUG, "ust.mtime: %ld local_time: %ld compare: %ld\n",
			ust.mtime, local_time, local_time - ust.mtime);
	_alpm_log(PM_LOG_DEBUG, "ust.size: %jd local_size: %jd compare: %jd\n",
			(intmax_t)ust.size, (intmax_t)local_size, (intmax_t)(local_size - ust.size));
	if(!force && ust.mtime && ust.mtime == local_time
			&& ust.size && ust.size == local_size) {
		/* the remote time and size values agreed with what we have, so move on
		 * because there is nothing more to do. */
		_alpm_log(PM_LOG_DEBUG, "files are identical, skipping %s\n", filename);
		ret = 1;
		goto cleanup;
	}
	if(!ust.mtime || ust.mtime != local_time) {
		_alpm_log(PM_LOG_DEBUG, "mtimes were different or unavailable, downloading %s from beginning\n", filename);
		fileurl->offset = 0;
	}

	fetchLastErrCode = 0;
	dlf = fetchGet(fileurl, "");
	check_stop();

	if(fetchLastErrCode != 0 || dlf == NULL) {
		pm_errno = PM_ERR_LIBFETCH;
		_alpm_log(PM_LOG_ERROR, _("failed retrieving file '%s' from %s : %s\n"),
				filename, gethost(fileurl), fetchLastErrString);
		ret = -1;
		goto cleanup;
	} else {
		_alpm_log(PM_LOG_DEBUG, "connected to %s successfully\n", fileurl->host);
	}

	if(localf && fileurl->offset == 0) {
		_alpm_log(PM_LOG_WARNING, _("resuming download of %s not possible; starting over\n"), filename);
		fclose(localf);
		localf = NULL;
	} else if(fileurl->offset) {
		_alpm_log(PM_LOG_DEBUG, "resuming download at position %jd\n", (intmax_t)fileurl->offset);
	}


	if(localf == NULL) {
		_alpm_rmrf(tempfile);
		fileurl->offset = (off_t)0;
		dl_thisfile = 0;
		localf = fopen(tempfile, "wb");
		if(localf == NULL) { /* still null? */
			pm_errno = PM_ERR_RETRIEVE;
			_alpm_log(PM_LOG_ERROR, _("error writing to file '%s': %s\n"),
					tempfile, strerror(errno));
			ret = -1;
			goto cleanup;
		}
	}

	/* Progress 0 - initialize */
	if(handle->dlcb) {
		handle->dlcb(filename, 0, ust.size);
	}

	while((nread = fetchIO_read(dlf, buffer, PM_DLBUF_LEN)) > 0) {
		check_stop();
		size_t nwritten = 0;
		nwritten = fwrite(buffer, 1, nread, localf);
		if((nwritten != (size_t)nread) || ferror(localf)) {
			pm_errno = PM_ERR_RETRIEVE;
			_alpm_log(PM_LOG_ERROR, _("error writing to file '%s': %s\n"),
					tempfile, strerror(errno));
			ret = -1;
			goto cleanup;
		}
		dl_thisfile += nread;

		if(handle->dlcb) {
			handle->dlcb(filename, dl_thisfile, ust.size);
		}
	}

	/* did the transfer complete normally? */
	if (nread == -1) {
		/* not PM_ERR_LIBFETCH here because libfetch error string might be empty */
		pm_errno = PM_ERR_RETRIEVE;
		_alpm_log(PM_LOG_ERROR, _("failed retrieving file '%s' from %s\n"),
				filename, gethost(fileurl));
		ret = -1;
		goto cleanup;
	}

	if (ust.size != -1 && dl_thisfile < ust.size) {
		pm_errno = PM_ERR_RETRIEVE;
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

	/* set the times on the file to the same as that of the remote file */
	if(ust.mtime) {
		struct timeval tv[2];
		memset(&tv, 0, sizeof(tv));
		tv[0].tv_sec = ust.atime;
		tv[1].tv_sec = ust.mtime;
		utimes(tempfile, tv);
	}
	rename(tempfile, destfile);
	ret = 0;

cleanup:
	FREE(tempfile);
	FREE(destfile);
	if(localf != NULL) {
		/* if we still had a local file open, we got interrupted. set the mtimes on
		 * the file accordingly. */
		fflush(localf);
		if(ust.mtime) {
			struct timeval tv[2];
			memset(&tv, 0, sizeof(tv));
			tv[0].tv_sec = ust.atime;
			tv[1].tv_sec = ust.mtime;
			futimes(fileno(localf), tv);
		}
		fclose(localf);
	}
	if(dlf != NULL) {
		fetchIO_close(dlf);
	}
	fetchFreeURL(fileurl);

	/* restore the old signal handlers */
	sigaction(SIGINT, &sig_int[OLD], NULL);
	sigaction(SIGPIPE, &sig_pipe[OLD], NULL);
	/* if we were interrupted, trip the old handler */
	if(dload_interrupted) {
		raise(SIGINT);
	}

	return(ret);
}
#endif

static int download(const char *url, const char *localpath,
		int force) {
	if(handle->fetchcb == NULL) {
#ifdef HAVE_LIBFETCH
		return(download_internal(url, localpath, force));
#else
		RET_ERR(PM_ERR_EXTERNAL_DOWNLOAD, -1);
#endif
	} else {
		int ret = handle->fetchcb(url, localpath, force);
		if(ret == -1) {
			RET_ERR(PM_ERR_EXTERNAL_DOWNLOAD, -1);
		}
		return(ret);
	}
}

/*
 * Download a single file
 *   - servers must be a list of urls WITHOUT trailing slashes.
 *
 * RETURN:  0 for successful download
 *          1 if the files are identical
 *         -1 on error
 */
int _alpm_download_single_file(const char *filename,
		alpm_list_t *servers, const char *localpath,
		int force)
{
	alpm_list_t *i;
	int ret = -1;

	ASSERT(servers != NULL, RET_ERR(PM_ERR_SERVER_NONE, -1));

	for(i = servers; i; i = i->next) {
		const char *server = i->data;
		char *fileurl = NULL;
		size_t len;

		/* print server + filename into a buffer */
		len = strlen(server) + strlen(filename) + 2;
		CALLOC(fileurl, len, sizeof(char), RET_ERR(PM_ERR_MEMORY, -1));
		snprintf(fileurl, len, "%s/%s", server, filename);

		ret = download(fileurl, localpath, force);
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
					localpath, 0) == -1) {
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
	ret = download(url, cachedir, 0);
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
