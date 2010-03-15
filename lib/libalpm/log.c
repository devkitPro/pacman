/*
 *  log.c
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

#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

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
 * @param fmt output format
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_logaction(char *fmt, ...)
{
	int ret;
	va_list args;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	/* check if the logstream is open already, opening it if needed */
	if(handle->logstream == NULL) {
		handle->logstream = fopen(handle->logfile, "a");
		/* if we couldn't open it, we have an issue */
		if(handle->logstream == NULL) {
			if(errno == EACCES) {
				pm_errno = PM_ERR_BADPERMS;
			} else if(errno == ENOENT) {
				pm_errno = PM_ERR_NOT_A_DIR;
			} else {
				pm_errno = PM_ERR_SYSTEM;
			}
		return(-1);
		}
	}

	va_start(args, fmt);
	ret = _alpm_logaction(handle->usesyslog, handle->logstream, fmt, args);
	va_end(args);

	/* TODO	We should add a prefix to log strings depending on who called us.
	 * If logaction was called by the frontend:
	 *   USER: <the frontend log>
	 * and if called internally:
	 *   ALPM: <the library log>
	 * Moreover, the frontend should be able to choose its prefix
	 * (USER by default?):
	 *   pacman: "PACMAN"
	 *   kpacman: "KPACMAN"
	 * This would allow us to share the log file between several frontends
	 * and know who does what */
	return(ret);
}

/** @} */

void _alpm_log(pmloglevel_t flag, char *fmt, ...)
{
	va_list args;
	alpm_cb_log logcb = alpm_option_get_logcb();

	if(logcb == NULL) {
		return;
	}

	va_start(args, fmt);
	logcb(flag, fmt, args);
	va_end(args);
}

/* vim: set ts=2 sw=2 noet: */
