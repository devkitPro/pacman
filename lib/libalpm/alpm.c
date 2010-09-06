/*
 *  alpm.c
 *
 *  Copyright (c) 2006-2010 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2005 by Christian Hamar <krics@linuxforum.hu>
 *  Copyright (c) 2005, 2006 by Miklos Vajna <vmiklos@frugalware.org>
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

/* connection caching setup */
#ifdef HAVE_LIBFETCH
#include <fetch.h>
#endif

/* libalpm */
#include "alpm.h"
#include "alpm_list.h"
#include "handle.h"
#include "util.h"

/* Globals */
enum _pmerrno_t pm_errno SYMEXPORT;

/** \addtogroup alpm_interface Interface Functions
 * @brief Functions to initialize and release libalpm
 * @{
 */

/** Initializes the library.  This must be called before any other
 * functions are called.
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_initialize(void)
{
	ASSERT(handle == NULL, RET_ERR(PM_ERR_HANDLE_NOT_NULL, -1));

	handle = _alpm_handle_new();
	if(handle == NULL) {
		RET_ERR(PM_ERR_MEMORY, -1);
	}

#ifdef ENABLE_NLS
	bindtextdomain("libalpm", LOCALEDIR);
#endif

#ifdef HAVE_LIBFETCH
	fetchConnectionCacheInit(5, 1);
#endif

	return(0);
}

/** Release the library.  This should be the last alpm call you make.
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_release(void)
{
	ALPM_LOG_FUNC;

	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	if(alpm_db_unregister_all() == -1) {
		return(-1);
	}

	_alpm_handle_free(handle);
	handle = NULL;

#ifdef HAVE_LIBFETCH
	fetchConnectionCacheClose();
#endif

	return(0);
}

/** @} */

/** @defgroup alpm_misc Miscellaneous Functions
 * @brief Various libalpm functions
 */

/* Get the version of library */
const char SYMEXPORT *alpm_version(void) {
	return(LIB_VERSION);
}

/* vim: set ts=2 sw=2 noet: */
