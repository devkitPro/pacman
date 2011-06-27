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

static const char *get_filename(const char *url)
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
	CALLOC(filepath, len, sizeof(char), return NULL);
	snprintf(filepath, len, "%s%s%s", path, filename, suffix);

	return filepath;
}

#define check_stop() if(dload_interrupted) { ret = -1; goto cleanup; }
enum sighandlers { OLD = 0, NEW = 1 };

static int dload_interrupted;
static void inthandler(int UNUSED signum)
{
	dload_interrupted = 1;
}

static int curl_progress(void *file, double dltotal, double dlnow,
		double UNUSED ultotal, double UNUSED ulnow)
{
	struct fileinfo *dlfile = (struct fileinfo *)file;
	double current_size, total_size;

	/* SIGINT sent, abort by alerting curl */
	if(dload_interrupted) {
		return 1;
	}

	/* none of what follows matters if the front end has no callback */
	if(dlfile->handle->dlcb == NULL) {
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
		dlfile->handle->dlcb(dlfile->filename, 0, (long)dltotal);
	}

	dlfile->handle->dlcb(dlfile->filename, (long)current_size, (long)total_size);

	prevprogress = current_size;

	return 0;
}

static int curl_gethost(const char *url, char *buffer)
{
	size_t hostlen;
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
			return 1;
		}
		snprintf(buffer, hostlen + 1, "%s", p);
	}

	return 0;
}

static int utimes_long(const char *path, long seconds)
{
	if(seconds != -1) {
		struct timeval tv[2];
		memset(&tv, 0, sizeof(tv));
		tv[0].tv_sec = tv[1].tv_sec = seconds;
		return utimes(path, tv);
	}
	return 0;
}


static int curl_download_internal(pmhandle_t *handle,
		const char *url, const char *localpath,
		int force, int allow_resume, int errors_ok)
{
	int ret = -1;
	FILE *localf = NULL;
	const char *useragent;
	const char *open_mode = "wb";
	char *destfile, *tempfile;
	/* RFC1123 states applications should support this length */
	char hostname[256];
	char error_buffer[CURL_ERROR_SIZE];
	struct stat st;
	long timecond, remote_time;
	double remote_size, bytes_dl;
	struct sigaction sig_pipe[2], sig_int[2];
	struct fileinfo dlfile;

	dlfile.handle = handle;
	dlfile.initial_size = 0.0;
	dlfile.filename = get_filename(url);
	if(!dlfile.filename || curl_gethost(url, hostname) != 0) {
		_alpm_log(handle, PM_LOG_ERROR, _("url '%s' is invalid\n"), url);
		RET_ERR(handle, PM_ERR_SERVER_BAD_URL, -1);
	}

	destfile = get_fullpath(localpath, dlfile.filename, "");
	tempfile = get_fullpath(localpath, dlfile.filename, ".part");
	if(!destfile || !tempfile) {
		goto cleanup;
	}

	error_buffer[0] = '\0';

	/* the curl_easy handle is initialized with the alpm handle, so we only need
	 * to reset the curl handle set parameters for each time it's used. */
	curl_easy_reset(handle->curl);
	curl_easy_setopt(handle->curl, CURLOPT_URL, url);
	curl_easy_setopt(handle->curl, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(handle->curl, CURLOPT_ERRORBUFFER, error_buffer);
	curl_easy_setopt(handle->curl, CURLOPT_CONNECTTIMEOUT, 10L);
	curl_easy_setopt(handle->curl, CURLOPT_FILETIME, 1L);
	curl_easy_setopt(handle->curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(handle->curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(handle->curl, CURLOPT_PROGRESSFUNCTION, curl_progress);
	curl_easy_setopt(handle->curl, CURLOPT_PROGRESSDATA, (void *)&dlfile);
	curl_easy_setopt(handle->curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
	curl_easy_setopt(handle->curl, CURLOPT_LOW_SPEED_TIME, 10L);

	useragent = getenv("HTTP_USER_AGENT");
	if(useragent != NULL) {
		curl_easy_setopt(handle->curl, CURLOPT_USERAGENT, useragent);
	}

	if(!allow_resume && !force && stat(destfile, &st) == 0) {
		/* start from scratch, but only download if our local is out of date. */
		curl_easy_setopt(handle->curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
		curl_easy_setopt(handle->curl, CURLOPT_TIMEVALUE, (long)st.st_mtime);
	} else if(stat(tempfile, &st) == 0 && allow_resume) {
		/* a previous partial download exists, resume from end of file. */
		open_mode = "ab";
		curl_easy_setopt(handle->curl, CURLOPT_RESUME_FROM, (long)st.st_size);
		_alpm_log(handle, PM_LOG_DEBUG, "tempfile found, attempting continuation");
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

	/* was it a success? */
	if(handle->curlerr == CURLE_ABORTED_BY_CALLBACK) {
		goto cleanup;
	} else if(handle->curlerr != CURLE_OK) {
		if(!errors_ok) {
			handle->pm_errno = PM_ERR_LIBCURL;
			_alpm_log(handle, PM_LOG_ERROR, _("failed retrieving file '%s' from %s : %s\n"),
					dlfile.filename, hostname, error_buffer);
		} else {
			_alpm_log(handle, PM_LOG_DEBUG, "failed retrieving file '%s' from %s : %s\n",
					dlfile.filename, hostname, error_buffer);
		}
		unlink(tempfile);
		goto cleanup;
	}

	/* retrieve info about the state of the transfer */
	curl_easy_getinfo(handle->curl, CURLINFO_FILETIME, &remote_time);
	curl_easy_getinfo(handle->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &remote_size);
	curl_easy_getinfo(handle->curl, CURLINFO_SIZE_DOWNLOAD, &bytes_dl);
	curl_easy_getinfo(handle->curl, CURLINFO_CONDITION_UNMET, &timecond);

	/* time condition was met and we didn't download anything. we need to
	 * clean up the 0 byte .part file that's left behind. */
	if(timecond == 1 && DOUBLE_EQ(bytes_dl, 0)) {
		ret = 1;
		unlink(tempfile);
		goto cleanup;
	}

	/* remote_size isn't necessarily the full size of the file, just what the
	 * server reported as remaining to download. compare it to what curl reported
	 * as actually being transferred during curl_easy_perform() */
	if(!DOUBLE_EQ(remote_size, -1) && !DOUBLE_EQ(bytes_dl, -1) &&
			!DOUBLE_EQ(bytes_dl, remote_size)) {
		handle->pm_errno = PM_ERR_RETRIEVE;
		_alpm_log(handle, PM_LOG_ERROR, _("%s appears to be truncated: %jd/%jd bytes\n"),
				dlfile.filename, (intmax_t)bytes_dl, (intmax_t)remote_size);
		goto cleanup;
	}

	ret = 0;

cleanup:
	if(localf != NULL) {
		fclose(localf);
		utimes_long(tempfile, remote_time);
	}

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

/** Download a file given by a URL to a local directory.
 * Does not overwrite an existing file if the download fails.
 * @param handle the context handle
 * @param url the file's URL
 * @param localpath the directory to save the file in
 * @param force force download even if there is an up-to-date local copy
 * @param allow_resume allow a partial download to be resumed
 * @param errors_ok do not log errors (but still return them)
 * @return 0 on success, -1 on error (pm_errno is set accordingly if errors_ok == 0)
 */
int _alpm_download(pmhandle_t *handle, const char *url, const char *localpath,
		int force, int allow_resume, int errors_ok)
{
	if(handle->fetchcb == NULL) {
#ifdef HAVE_LIBCURL
		return curl_download_internal(handle, url, localpath,
				force, allow_resume, errors_ok);
#else
		RET_ERR(handle, PM_ERR_EXTERNAL_DOWNLOAD, -1);
#endif
	} else {
		int ret = handle->fetchcb(url, localpath, force);
		if(ret == -1 && !errors_ok) {
			RET_ERR(handle, PM_ERR_EXTERNAL_DOWNLOAD, -1);
		}
		return ret;
	}
}

/** Fetch a remote pkg. */
char SYMEXPORT *alpm_fetch_pkgurl(pmhandle_t *handle, const char *url)
{
	char *filepath;
	const char *filename, *cachedir;
	int ret;

	CHECK_HANDLE(handle, return NULL);

	filename = get_filename(url);

	/* find a valid cache dir to download to */
	cachedir = _alpm_filecache_setup(handle);

	/* download the file */
	ret = _alpm_download(handle, url, cachedir, 0, 1, 0);
	if(ret == -1) {
		_alpm_log(handle, PM_LOG_WARNING, _("failed to download %s\n"), url);
		return NULL;
	}
	_alpm_log(handle, PM_LOG_DEBUG, "successfully downloaded %s\n", url);

	/* attempt to download the signature */
	if(ret == 0 && (handle->sigverify == PM_PGP_VERIFY_ALWAYS ||
				handle->sigverify == PM_PGP_VERIFY_OPTIONAL)) {
		char *sig_url;
		size_t len;
		int errors_ok = (handle->sigverify == PM_PGP_VERIFY_OPTIONAL);

		len = strlen(url) + 5;
		CALLOC(sig_url, len, sizeof(char), RET_ERR(handle, PM_ERR_MEMORY, NULL));
		snprintf(sig_url, len, "%s.sig", url);

		ret = _alpm_download(handle, sig_url, cachedir, 1, 0, errors_ok);
		if(ret == -1 && !errors_ok) {
			_alpm_log(handle, PM_LOG_WARNING, _("failed to download %s\n"), sig_url);
			/* Warn now, but don't return NULL. We will fail later during package
			 * load time. */
		} else if(ret == 0) {
			_alpm_log(handle, PM_LOG_DEBUG, "successfully downloaded %s\n", sig_url);
		}
		FREE(sig_url);
	}

	/* we should be able to find the file the second time around */
	filepath = _alpm_filecache_find(handle, filename);
	return filepath;
}

/* vim: set ts=2 sw=2 noet: */
