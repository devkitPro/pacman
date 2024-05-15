/*
 *  sandbox.c
 *
 *  Copyright (c) 2021-2024 Pacman Development Team <pacman-dev@lists.archlinux.org>
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

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include "alpm.h"
#include "log.h"
#include "sandbox.h"
#include "sandbox_fs.h"
#include "util.h"

int SYMEXPORT alpm_sandbox_setup_child(alpm_handle_t *handle, const char* sandboxuser, const char* sandbox_path)
{
	struct passwd const *pw = NULL;

	ASSERT(sandboxuser != NULL, return -1);
	ASSERT(getuid() == 0, return -1);
	ASSERT((pw = getpwnam(sandboxuser)), return -1);
	if(sandbox_path != NULL && !handle->disable_sandbox) {
		_alpm_sandbox_fs_restrict_writes_to(handle, sandbox_path);
	}
	ASSERT(setgid(pw->pw_gid) == 0, return -1);
	ASSERT(setgroups(0, NULL) == 0, return -1);
	ASSERT(setuid(pw->pw_uid) == 0, return -1);

	return 0;
}

static int should_retry(int errnum)
{
	return errnum == EINTR;
}

static int read_from_pipe(int fd, void *buf, size_t count)
{
	size_t nread = 0;

	ASSERT(count > 0, return -1);

	while(nread < count) {
		ssize_t r = read(fd, (char *)buf + nread, count-nread);
		if(r < 0) {
			if(!should_retry(errno)) {
				return -1;
			}
			continue;
		}
		if(r == 0) {
			/* we hit EOF unexpectedly - bail */
			return -1;
		}
		nread += r;
	}

	return 0;
}

static int write_to_pipe(int fd, const void *buf, size_t count)
{
	size_t nwrite = 0;

	ASSERT(count > 0, return -1);

	while(nwrite < count) {
		ssize_t r = write(fd, (char *)buf + nwrite, count-nwrite);
		if(r < 0) {
			if(!should_retry(errno)) {
				return -1;
			}
			continue;
		}
		nwrite += r;
	}

	return 0;
}

void _alpm_sandbox_cb_log(void *ctx, alpm_loglevel_t level, const char *fmt, va_list args)
{
	_alpm_sandbox_callback_t type = ALPM_SANDBOX_CB_LOG;
	_alpm_sandbox_callback_context *context = ctx;
	char *string = NULL;
	int string_size = 0;

	if(!context || context->callback_pipe == -1) {
		return;
	}

	/* compute the required size, as allowed by POSIX.1-2001 and C99 */
	/* first we need to copy the va_list as it will be consumed by the first call */
	va_list copy;
	va_copy(copy, args);
	string_size = vsnprintf(NULL, 0, fmt, copy);
	if(string_size <= 0) {
		va_end(copy);
		return;
	}
	MALLOC(string, string_size + 1, return);
	string_size = vsnprintf(string, string_size + 1, fmt, args);
	if(string_size > 0) {
		write_to_pipe(context->callback_pipe, &type, sizeof(type));
		write_to_pipe(context->callback_pipe, &level, sizeof(level));
		write_to_pipe(context->callback_pipe, &string_size, sizeof(string_size));
		write_to_pipe(context->callback_pipe, string, string_size);
	}
	va_end(copy);
	FREE(string);
}

void _alpm_sandbox_cb_dl(void *ctx, const char *filename, alpm_download_event_type_t event, void *data)
{
	_alpm_sandbox_callback_t type = ALPM_SANDBOX_CB_DOWNLOAD;
	_alpm_sandbox_callback_context *context = ctx;
	size_t filename_len;

	if(!context || context->callback_pipe == -1) {
		return;
	}

	ASSERT(filename != NULL, return);
	ASSERT(event == ALPM_DOWNLOAD_INIT || event == ALPM_DOWNLOAD_PROGRESS || event == ALPM_DOWNLOAD_RETRY || event == ALPM_DOWNLOAD_COMPLETED, return);

	filename_len = strlen(filename);

	write_to_pipe(context->callback_pipe, &type, sizeof(type));
	write_to_pipe(context->callback_pipe, &event, sizeof(event));
	switch(event) {
		case ALPM_DOWNLOAD_INIT:
			write_to_pipe(context->callback_pipe, data, sizeof(alpm_download_event_init_t));
			break;
		case ALPM_DOWNLOAD_PROGRESS:
			write_to_pipe(context->callback_pipe, data, sizeof(alpm_download_event_progress_t));
			break;
		case ALPM_DOWNLOAD_RETRY:
			write_to_pipe(context->callback_pipe, data, sizeof(alpm_download_event_retry_t));
			break;
		case ALPM_DOWNLOAD_COMPLETED:
			write_to_pipe(context->callback_pipe, data, sizeof(alpm_download_event_completed_t));
			break;
	}
	write_to_pipe(context->callback_pipe, &filename_len, sizeof(filename_len));
	write_to_pipe(context->callback_pipe, filename, filename_len);
}


bool _alpm_sandbox_process_cb_log(alpm_handle_t *handle, int callback_pipe) {
	alpm_loglevel_t level;
	char *string = NULL;
	int string_size = 0;

	ASSERT(read_from_pipe(callback_pipe, &level, sizeof(level)) != -1, return false);
	ASSERT(read_from_pipe(callback_pipe, &string_size, sizeof(string_size)) != -1, return false);

	MALLOC(string, string_size + 1, return false);

	ASSERT(read_from_pipe(callback_pipe, string, string_size) != -1, FREE(string); return false);
	string[string_size] = '\0';

	_alpm_log(handle, level, "%s", string);
	FREE(string);
	return true;
}

bool _alpm_sandbox_process_cb_download(alpm_handle_t *handle, int callback_pipe) {
	alpm_download_event_type_t type;
	char *filename = NULL;
	size_t filename_size, cb_data_size;
	union {
		alpm_download_event_init_t init;
		alpm_download_event_progress_t progress;
		alpm_download_event_retry_t retry;
		alpm_download_event_completed_t completed;
	} cb_data;

	ASSERT(read_from_pipe(callback_pipe, &type, sizeof(type)) != -1, return false);

	switch (type) {
		case ALPM_DOWNLOAD_INIT:
			cb_data_size = sizeof(alpm_download_event_init_t);
			ASSERT(read_from_pipe(callback_pipe, &cb_data.init, cb_data_size) != -1, return false);
			break;
		case ALPM_DOWNLOAD_PROGRESS:
			cb_data_size = sizeof(alpm_download_event_progress_t);
			ASSERT(read_from_pipe(callback_pipe, &cb_data.progress, cb_data_size) != -1, return false);
			break;
		case ALPM_DOWNLOAD_RETRY:
			cb_data_size = sizeof(alpm_download_event_retry_t);
			ASSERT(read_from_pipe(callback_pipe, &cb_data.retry, cb_data_size) != -1, return false);
			break;
		case ALPM_DOWNLOAD_COMPLETED:
			cb_data_size = sizeof(alpm_download_event_completed_t);
			ASSERT(read_from_pipe(callback_pipe, &cb_data.completed, cb_data_size) != -1, return false);
			break;
		default:
			return false;
	}

	ASSERT(read_from_pipe(callback_pipe, &filename_size, sizeof(filename_size)) != -1, return false);;

	MALLOC(filename, filename_size + 1, return false);

	ASSERT(read_from_pipe(callback_pipe, filename, filename_size) != -1, FREE(filename); return false);
	filename[filename_size] = '\0';

	handle->dlcb(handle->dlcb_ctx, filename, type, &cb_data);
	FREE(filename);
	return true;
}
