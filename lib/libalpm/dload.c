/*
 *  download.c
 *
 *  Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
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

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif

/* libalpm */
#include "dload.h"
#include "alpm_list.h"
#include "alpm.h"
#include "log.h"
#include "util.h"
#include "handle.h"

#ifdef HAVE_LIBCURL
static double prevprogress; /* last download amount */
#endif

static char *get_filename(const char *url)
{
	char *filename = strrchr(url, '/');
	if(filename != NULL) {
		filename++;
	}
	return filename;
}

#ifdef HAVE_LIBCURL
static char *get_fullpath(const char *path, const char *filename,
		const char *suffix)
{
	char *filepath;
	/* len = localpath len + filename len + suffix len + null */
	size_t len = strlen(path) + strlen(filename) + strlen(suffix) + 1;
	CALLOC(filepath, len, sizeof(char), RET_ERR(PM_ERR_MEMORY, NULL));
	snprintf(filepath, len, "%s%s%s", path, filename, suffix);

	return filepath;
}

#define check_stop() if(dload_interrupted) { ret = -1; goto cleanup; }
enum sighandlers { OLD = 0, NEW = 1 };

int dload_interrupted;
static void inthandler(int signum)
{
	dload_interrupted = 1;
}

static int curl_progress(void *file, double dltotal, double dlnow,
		double ultotal, double ulnow)
{
	struct fileinfo *dlfile = (struct fileinfo *)file;
	double current_size, total_size;

	/* unused parameters */
	(void)ultotal;
	(void)ulnow;

	/* SIGINT sent, abort by alerting curl */
	if(dload_interrupted) {
		return 1;
	}

	/* none of what follows matters if the front end has no callback */
	if(handle->dlcb == NULL) {
		return 0;
	}

	current_size = dlfile->initial_size + dlnow;
	total_size = dlfile->initial_size + dltotal;

	if(DOUBLE_EQ(dltotal, 0) || DOUBLE_EQ(prevprogress, total_size)) {
		return 0;
	}

	/* initialize the progress bar here to avoid displaying it when
	 * a repo is up to date and nothing gets downloaded */
	if(DOUBLE_EQ(prevprogress, 0)) {
		handle->dlcb(dlfile->filename, 0, (long)dltotal);
	}

	handle->dlcb(dlfile->filename, (long)current_size, (long)total_size);

	prevprogress = current_size;

	return 0;
}

static int curl_gethost(const char *url, char *buffer)
{
	int hostlen;
	char *p;

	if(strncmp(url, "file://", 7) == 0) {
		strcpy(buffer, _("disk"));
	} else {
		p = strstr(url, "//");
		if(!p) {
			return 1;
		}
		p += 2; /* jump over the found // */
		hostlen = strcspn(p, "/");
		if(hostlen > 255) {
			/* buffer overflow imminent */
			_alpm_log(PM_LOG_ERROR, _("buffer overflow detected"));
			return 1;
		}
		snprintf(buffer, hostlen + 1, "%s", p);
	}

	return 0;
}

static int utimes_long(const char *path, long time)
{
	if(time != -1) {
		struct timeval tv[2];
		memset(&tv, 0, sizeof(tv));
		tv[0].tv_sec = tv[1].tv_sec = time;
		return utimes(path, tv);
	}
	return 0;
}


static int curl_download_internal(const char *url, const char *localpath,
		int force)
{
	int ret = -1;
	FILE *localf = NULL;
	const char *open_mode, *useragent;
	char *destfile, *tempfile;
	char hostname[256]; /* RFC1123 states applications should support this length */
	struct stat st;
	long httpresp, timecond, remote_time;
	double remote_size, bytes_dl;
	struct sigaction sig_pipe[2], sig_int[2];
	struct fileinfo dlfile;

	dlfile.initial_size = 0.0;
	dlfile.filename = get_filename(url);
	if(!dlfile.filename || curl_gethost(url, hostname) != 0) {
		_alpm_log(PM_LOG_ERROR, _("url '%s' is invalid\n"), url);
		RET_ERR(PM_ERR_SERVER_BAD_URL, -1);
	}

	destfile = get_fullpath(localpath, dlfile.filename, "");
	tempfile = get_fullpath(localpath, dlfile.filename, ".part");
	if(!destfile || !tempfile) {
		goto cleanup;
	}

	/* the curl_easy handle is initialized with the alpm handle, so we only need
	 * to reset the curl handle set parameters for each time it's used. */
	curl_easy_reset(handle->curl);
	curl_easy_setopt(handle->curl, CURLOPT_URL, url);
	curl_easy_setopt(handle->curl, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(handle->curl, CURLOPT_CONNECTTIMEOUT, 10L);
	curl_easy_setopt(handle->curl, CURLOPT_FILETIME, 1L);
	curl_easy_setopt(handle->curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(handle->curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(handle->curl, CURLOPT_PROGRESSFUNCTION, curl_progress);
	curl_easy_setopt(handle->curl, CURLOPT_PROGRESSDATA, (void*)&dlfile);

	useragent = getenv("HTTP_USER_AGENT");
	if (useragent != NULL) {
		curl_easy_setopt(handle->curl, CURLOPT_USERAGENT, useragent);
	}

	/* TODO: no assuming here. the calling function should tell us what's kosher */
	if(!force && stat(destfile, &st) == 0) {
		/* assume its a sync, so we're starting from scratch. but, only download
		 * our local is out of date. */
		curl_easy_setopt(handle->curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
		curl_easy_setopt(handle->curl, CURLOPT_TIMEVALUE, (long)st.st_mtime);
	} else if(stat(tempfile, &st) == 0 && st.st_size > 0) {
		/* assume its a partial package download. we do not support resuming of
		 * transfers on partially downloaded sync DBs. */
		open_mode = "ab";
		curl_easy_setopt(handle->curl, CURLOPT_RESUME_FROM, (long)st.st_size);
		_alpm_log(PM_LOG_DEBUG, "tempfile found, attempting continuation");
		dlfile.initial_size = (double)st.st_size;
	}

	localf = fopen(tempfile, open_mode);
	if(localf == NULL) {
		goto cleanup;
	}

	curl_easy_setopt(handle->curl, CURLOPT_WRITEDATA, localf);

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

	/* Progress 0 - initialize */
	prevprogress = 0;

	/* perform transfer */
	handle->curlerr = curl_easy_perform(handle->curl);

	/* retrieve info about the state of the transfer */
	curl_easy_getinfo(handle->curl, CURLINFO_RESPONSE_CODE, &httpresp);
	curl_easy_getinfo(handle->curl, CURLINFO_FILETIME, &remote_time);
	curl_easy_getinfo(handle->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &remote_size);
	curl_easy_getinfo(handle->curl, CURLINFO_SIZE_DOWNLOAD, &bytes_dl);
	curl_easy_getinfo(handle->curl, CURLINFO_CONDITION_UNMET, &timecond);

	/* time condition was met and we didn't download anything. we need to
	 * clean up the 0 byte .part file that's left behind. */
	if(DOUBLE_EQ(bytes_dl, 0) && timecond == 1) {
		ret = 1;
		unlink(tempfile);
		goto cleanup;
	}

	if(handle->curlerr == CURLE_ABORTED_BY_CALLBACK) {
		goto cleanup;
	} else if(handle->curlerr != CURLE_OK) {
		pm_errno = PM_ERR_LIBCURL;
		_alpm_log(PM_LOG_ERROR, _("failed retrieving file '%s' from %s : %s\n"),
				dlfile.filename, hostname, curl_easy_strerror(handle->curlerr));
		unlink(tempfile);
		goto cleanup;
	}

	/* remote_size isn't necessarily the full size of the file, just what the
	 * server reported as remaining to download. compare it to what curl reported
	 * as actually being transferred during curl_easy_perform() */
	if(!DOUBLE_EQ(remote_size, -1) && !DOUBLE_EQ(bytes_dl, -1) &&
			!DOUBLE_EQ(bytes_dl, remote_size)) {
		pm_errno = PM_ERR_RETRIEVE;
		_alpm_log(PM_LOG_ERROR, _("%s appears to be truncated: %jd/%jd bytes\n"),
				dlfile.filename, (intmax_t)bytes_dl, (intmax_t)remote_size);
		goto cleanup;
	}

	ret = 0;

cleanup:
	if(localf != NULL) {
		fclose(localf);
		utimes_long(tempfile, remote_time);
	}

	/* TODO: A signature download will need to return success here as well before
	 * we're willing to rotate the new file into place. */
	if(ret == 0) {
		rename(tempfile, destfile);
	}

	FREE(tempfile);
	FREE(destfile);

	/* restore the old signal handlers */
	sigaction(SIGINT, &sig_int[OLD], NULL);
	sigaction(SIGPIPE, &sig_pipe[OLD], NULL);
	/* if we were interrupted, trip the old handler */
	if(dload_interrupted) {
		raise(SIGINT);
	}

	return ret;
}
#endif

static int download(const char *url, const char *localpath,
		int force)
{
	if(handle->fetchcb == NULL) {
#ifdef HAVE_LIBCURL
		return curl_download_internal(url, localpath, force);
#else
		RET_ERR(PM_ERR_EXTERNAL_DOWNLOAD, -1);
#endif
	} else {
		int ret = handle->fetchcb(url, localpath, force);
		if(ret == -1) {
			RET_ERR(PM_ERR_EXTERNAL_DOWNLOAD, -1);
		}
		return ret;
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

	return ret;
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

	return ret;
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
		return NULL;
	}
	_alpm_log(PM_LOG_DEBUG, "successfully downloaded %s\n", url);

	/* we should be able to find the file the second time around */
	filepath = _alpm_filecache_find(filename);
	return filepath;
}

/* vim: set ts=2 sw=2 noet: */
