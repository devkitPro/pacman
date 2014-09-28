/*
 *  log.c
 *
 *  Copyright (c) 2006-2014 Pacman Development Team <pacman-dev@archlinux.org>
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

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

/* libalpm */
#include "log.h"
#include "handle.h"
#include "util.h"
#include "alpm.h"

/** \addtogroup alpm_log Logging Functions
 * @brief Functions to log using libalpm
 * @{
 */

/** A printf-like function for logging.
 * @param handle the context handle
 * @param prefix caller-specific prefix for the log
 * @param fmt output format
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_logaction(alpm_handle_t *handle, const char *prefix,
		const char *fmt, ...)
{
	int ret;
	va_list args;

	ASSERT(handle != NULL, return -1);

	/* check if the logstream is open already, opening it if needed */
	if(handle->logstream == NULL) {
		int fd;
		do {
			fd = open(handle->logfile, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC,
					0000);
		} while(fd == -1 && errno == EINTR);
		if(fd >= 0) {
			handle->logstream = fdopen(fd, "a");
		}
		/* if we couldn't open it, we have an issue */
		if(fd < 0 || handle->logstream == NULL) {
			if(errno == EACCES) {
				handle->pm_errno = ALPM_ERR_BADPERMS;
			} else if(errno == ENOENT) {
				handle->pm_errno = ALPM_ERR_NOT_A_DIR;
			} else {
				handle->pm_errno = ALPM_ERR_SYSTEM;
			}
			return -1;
		}
	}

	va_start(args, fmt);
	ret = _alpm_logaction(handle, prefix, fmt, args);
	va_end(args);

	return ret;
}

/** @} */

void _alpm_log(alpm_handle_t *handle, alpm_loglevel_t flag, const char *fmt, ...)
{
	va_list args;

	if(handle == NULL || handle->logcb == NULL) {
		return;
	}

	va_start(args, fmt);
	handle->logcb(flag, fmt, args);
	va_end(args);
}

/* vim: set noet: */
