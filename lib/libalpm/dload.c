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

static const char *get_filename(const char *url)
{
	char *filename = strrchr(url, '/');
	if(filename != NULL) {
		filename++;
	}
	return filename;
}

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
	struct dload_payload *payload = (struct dload_payload *)file;
	double current_size, total_size;

	/* SIGINT sent, abort by alerting curl */
	if(dload_interrupted) {
		return 1;
	}

	/* none of what follows matters if the front end has no callback */
	if(payload->handle->dlcb == NULL) {
		return 0;
	}

	current_size = payload->initial_size + dlnow;
	total_size = payload->initial_size + dltotal;

	if(DOUBLE_EQ(dltotal, 0) || DOUBLE_EQ(prevprogress, total_size)) {
		return 0;
	}

	/* initialize the progress bar here to avoid displaying it when
	 * a repo is up to date and nothing gets downloaded */
	if(DOUBLE_EQ(prevprogress, 0)) {
		payload->handle->dlcb(payload->filename, 0, (long)dltotal);
	}

	payload->handle->dlcb(payload->filename, (long)current_size, (long)total_size);

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

static size_t parse_headers(void *ptr, size_t size, size_t nmemb, void *user)
{
	size_t realsize = size * nmemb;
	const char *fptr, *endptr = NULL;
	const char * const cd_header = "Content-Disposition:";
	const char * const fn_key = "filename=";
	struct dload_payload *payload = (struct dload_payload *)user;

	if(_alpm_raw_ncmp(cd_header, ptr, strlen(cd_header)) == 0) {
		if((fptr = strstr(ptr, fn_key))) {
			fptr += strlen(fn_key);

			/* find the end of the field, which is either a semi-colon, or the end of
			 * the data. As per curl_easy_setopt(3), we cannot count on headers being
			 * null terminated, so we look for the closing \r\n */
			endptr = fptr + strcspn(fptr, ";\r\n") - 1;

			/* remove quotes */
			if(*fptr == '"' && *endptr == '"') {
				fptr++;
				endptr--;
			}

			STRNDUP(payload->cd_filename, fptr, endptr - fptr + 1,
					RET_ERR(payload->handle, ALPM_ERR_MEMORY, realsize));
		}
	}

	return realsize;
}

static int curl_download_internal(struct dload_payload *payload,
		const char *localpath, char **final_file)
{
	int ret = -1, should_unlink = 0;
	FILE *localf = NULL;
	const char *useragent;
	const char *open_mode = "wb";
	char *destfile = NULL, *tempfile = NULL, *effective_url;
	/* RFC1123 states applications should support this length */
	char hostname[256];
	char error_buffer[CURL_ERROR_SIZE];
	struct stat st;
	long timecond, remote_time = -1;
	double remote_size, bytes_dl;
	struct sigaction sig_pipe[2], sig_int[2];
	/* shortcut to our handle within the payload */
	alpm_handle_t *handle = payload->handle;
	handle->pm_errno = 0;

	if(!payload->filename) {
		payload->filename = get_filename(payload->fileurl);
	}
	if(!payload->filename || curl_gethost(payload->fileurl, hostname) != 0) {
		_alpm_log(handle, ALPM_LOG_ERROR, _("url '%s' is invalid\n"), payload->fileurl);
		RET_ERR(handle, ALPM_ERR_SERVER_BAD_URL, -1);
	}

	if(strlen(payload->filename) > 0 && strcmp(payload->filename, ".sig") != 0) {
		destfile = get_fullpath(localpath, payload->filename, "");
		tempfile = get_fullpath(localpath, payload->filename, ".part");
		if(!destfile || !tempfile) {
			goto cleanup;
		}
	} else {
		/* URL isn't to a file and ended with a slash */
		int fd;
		char randpath[PATH_MAX];

		/* we can't support resuming this kind of download, so a partial transfer
		 * will be destroyed */
		should_unlink = 1;

		/* create a random filename, which is opened with O_EXCL */
		snprintf(randpath, PATH_MAX, "%salpmtmp.XXXXXX", localpath);
		if((fd = mkstemp(randpath)) == -1 || !(localf = fdopen(fd, open_mode))) {
			unlink(randpath);
			close(fd);
			_alpm_log(handle, ALPM_LOG_ERROR,
					_("failed to create temporary file for download\n"));
			goto cleanup;
		}
		/* localf now points to our alpmtmp.XXXXXX */
		STRDUP(tempfile, randpath, RET_ERR(handle, ALPM_ERR_MEMORY, -1));
		payload->filename = strrchr(randpath, '/') + 1;
	}

	error_buffer[0] = '\0';

	/* the curl_easy handle is initialized with the alpm handle, so we only need
	 * to reset the curl handle set parameters for each time it's used. */
	curl_easy_reset(handle->curl);
	curl_easy_setopt(handle->curl, CURLOPT_URL, payload->fileurl);
	curl_easy_setopt(handle->curl, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(handle->curl, CURLOPT_ERRORBUFFER, error_buffer);
	curl_easy_setopt(handle->curl, CURLOPT_CONNECTTIMEOUT, 10L);
	curl_easy_setopt(handle->curl, CURLOPT_FILETIME, 1L);
	curl_easy_setopt(handle->curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(handle->curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(handle->curl, CURLOPT_PROGRESSFUNCTION, curl_progress);
	curl_easy_setopt(handle->curl, CURLOPT_PROGRESSDATA, (void *)payload);
	curl_easy_setopt(handle->curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
	curl_easy_setopt(handle->curl, CURLOPT_LOW_SPEED_TIME, 10L);
	curl_easy_setopt(handle->curl, CURLOPT_HEADERFUNCTION, parse_headers);
	curl_easy_setopt(handle->curl, CURLOPT_WRITEHEADER, (void *)payload);

	if(payload->max_size) {
		curl_easy_setopt(handle->curl, CURLOPT_MAXFILESIZE, payload->max_size);
	}

	useragent = getenv("HTTP_USER_AGENT");
	if(useragent != NULL) {
		curl_easy_setopt(handle->curl, CURLOPT_USERAGENT, useragent);
	}

	if(!payload->allow_resume && !payload->force && stat(destfile, &st) == 0) {
		/* start from scratch, but only download if our local is out of date. */
		curl_easy_setopt(handle->curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
		curl_easy_setopt(handle->curl, CURLOPT_TIMEVALUE, (long)st.st_mtime);
	} else if(stat(tempfile, &st) == 0 && payload->allow_resume) {
		/* a previous partial download exists, resume from end of file. */
		open_mode = "ab";
		curl_easy_setopt(handle->curl, CURLOPT_RESUME_FROM, (long)st.st_size);
		_alpm_log(handle, ALPM_LOG_DEBUG, "tempfile found, attempting continuation\n");
		payload->initial_size = (double)st.st_size;
	}

	if(localf == NULL) {
		localf = fopen(tempfile, open_mode);
		if(localf == NULL) {
			goto cleanup;
		}
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

	/* immediately unhook the progress callback */
	curl_easy_setopt(handle->curl, CURLOPT_NOPROGRESS, 1L);

	/* was it a success? */
	switch(handle->curlerr) {
		case CURLE_OK:
			break;
		case CURLE_ABORTED_BY_CALLBACK:
			goto cleanup;
		case CURLE_OPERATION_TIMEDOUT:
			dload_interrupted = 1;
			/* fallthrough */
		default:
			if(!payload->errors_ok) {
				handle->pm_errno = ALPM_ERR_LIBCURL;
				_alpm_log(handle, ALPM_LOG_ERROR, _("failed retrieving file '%s' from %s : %s\n"),
						payload->filename, hostname, error_buffer);
				if(!dload_interrupted) {
					unlink(tempfile);
				}
			} else {
				_alpm_log(handle, ALPM_LOG_DEBUG, "failed retrieving file '%s' from %s : %s\n",
						payload->filename, hostname, error_buffer);
			}
			goto cleanup;
	}

	/* retrieve info about the state of the transfer */
	curl_easy_getinfo(handle->curl, CURLINFO_FILETIME, &remote_time);
	curl_easy_getinfo(handle->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &remote_size);
	curl_easy_getinfo(handle->curl, CURLINFO_SIZE_DOWNLOAD, &bytes_dl);
	curl_easy_getinfo(handle->curl, CURLINFO_CONDITION_UNMET, &timecond);
	curl_easy_getinfo(handle->curl, CURLINFO_EFFECTIVE_URL, &effective_url);

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
		handle->pm_errno = ALPM_ERR_RETRIEVE;
		_alpm_log(handle, ALPM_LOG_ERROR, _("%s appears to be truncated: %jd/%jd bytes\n"),
				payload->filename, (intmax_t)bytes_dl, (intmax_t)remote_size);
		goto cleanup;
	}

	if(payload->cd_filename) {
		/* content-disposition header has a better name for our file */
		free(destfile);
		destfile = get_fullpath(localpath, payload->cd_filename, "");
	} else {
		const char *effective_filename = strrchr(effective_url, '/');
		if(effective_filename) {
			effective_filename++;

			/* if destfile was never set, we wrote to a tempfile. even if destfile is
			 * set, we may have followed some redirects and the effective url may
			 * have a better suggestion as to what to name our file. in either case,
			 * refactor destfile to this newly derived name. */
			if(!destfile || strcmp(effective_filename, strrchr(destfile, '/') + 1) != 0) {
				free(destfile);
				destfile = get_fullpath(localpath, effective_filename, "");
			}
		}
	}

	ret = 0;

cleanup:
	if(localf != NULL) {
		fclose(localf);
		utimes_long(tempfile, remote_time);
	}

	if(ret == 0) {
		if(rename(tempfile, destfile)) {
			_alpm_log(handle, ALPM_LOG_ERROR, _("could not rename %s to %s (%s)\n"),
					tempfile, destfile, strerror(errno));
			ret = -1;
		} else if(final_file) {
			*final_file = strdup(strrchr(destfile, '/') + 1);
		}
	}

	if(dload_interrupted && should_unlink) {
		unlink(tempfile);
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
 * @param payload the payload context
 * @param localpath the directory to save the file in
 * @param final_file the real name of the downloaded file (may be NULL)
 * @return 0 on success, -1 on error (pm_errno is set accordingly if errors_ok == 0)
 */
int _alpm_download(struct dload_payload *payload, const char *localpath,
		char **final_file)
{
	alpm_handle_t *handle = payload->handle;

	if(handle->fetchcb == NULL) {
#ifdef HAVE_LIBCURL
		return curl_download_internal(payload, localpath, final_file);
#else
		RET_ERR(handle, ALPM_ERR_EXTERNAL_DOWNLOAD, -1);
#endif
	} else {
		int ret = handle->fetchcb(payload->fileurl, localpath, payload->force);
		if(ret == -1 && !payload->errors_ok) {
			RET_ERR(handle, ALPM_ERR_EXTERNAL_DOWNLOAD, -1);
		}
		return ret;
	}
}

/** Fetch a remote pkg. */
char SYMEXPORT *alpm_fetch_pkgurl(alpm_handle_t *handle, const char *url)
{
	char *filepath;
	const char *cachedir;
	char *final_file = NULL;
	struct dload_payload *payload;
	int ret;

	CHECK_HANDLE(handle, return NULL);

	/* find a valid cache dir to download to */
	cachedir = _alpm_filecache_setup(handle);

	CALLOC(payload, 1, sizeof(*payload), RET_ERR(handle, ALPM_ERR_MEMORY, NULL));
	payload->handle = handle;
	payload->fileurl = strdup(url);
	payload->allow_resume = 1;

	/* download the file */
	ret = _alpm_download(payload, cachedir, &final_file);
	if(ret == -1) {
		_alpm_log(handle, ALPM_LOG_WARNING, _("failed to download %s\n"), url);
		return NULL;
	}
	_alpm_log(handle, ALPM_LOG_DEBUG, "successfully downloaded %s\n", url);

	/* attempt to download the signature */
	if(ret == 0 && (handle->siglevel & ALPM_SIG_PACKAGE)) {
		char *sig_final_file = NULL;
		size_t len;
		struct dload_payload *sig_payload;

		CALLOC(sig_payload, 1, sizeof(*sig_payload), RET_ERR(handle, ALPM_ERR_MEMORY, NULL));
		len = strlen(url) + 5;
		CALLOC(sig_payload->fileurl, len, sizeof(char), RET_ERR(handle, ALPM_ERR_MEMORY, NULL));
		snprintf(sig_payload->fileurl, len, "%s.sig", url);
		sig_payload->handle = handle;
		sig_payload->force = 1;
		sig_payload->errors_ok = (handle->siglevel & ALPM_SIG_PACKAGE_OPTIONAL);

		ret = _alpm_download(sig_payload, cachedir, &sig_final_file);
		if(ret == -1 && !sig_payload->errors_ok) {
			_alpm_log(handle, ALPM_LOG_WARNING, _("failed to download %s\n"), sig_payload->fileurl);
			/* Warn now, but don't return NULL. We will fail later during package
			 * load time. */
		} else if(ret == 0) {
			_alpm_log(handle, ALPM_LOG_DEBUG, "successfully downloaded %s\n", sig_payload->fileurl);
		}
		FREE(sig_final_file);
		_alpm_dload_payload_free(sig_payload);
	}

	/* we should be able to find the file the second time around */
	filepath = _alpm_filecache_find(handle, final_file);
	FREE(final_file);
	_alpm_dload_payload_free(payload);

	return filepath;
}

void _alpm_dload_payload_free(struct dload_payload *payload) {
	ASSERT(payload, return);

	FREE(payload->fileurl);
	FREE(payload->cd_filename);
	FREE(payload);
}

/* vim: set ts=2 sw=2 noet: */
