/*
 *  alpm.c
 * 
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
 *  USA.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h> /* PATH_MAX */
#include <stdarg.h>

/* libalpm */
#include "alpm.h"
#include "alpm_list.h"
#include "error.h"
#include "handle.h"
#include "util.h"

#define min(X, Y)  ((X) < (Y) ? (X) : (Y))

/* Globals */
pmhandle_t *handle = NULL;
enum _pmerrno_t pm_errno SYMEXPORT;

/** \addtogroup alpm_interface Interface Functions
 * @brief Functions to initialize and release libalpm
 * @{
 */

/** Initializes the library.  This must be called before any other
 * functions are called.
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_initialize()
{
	ASSERT(handle == NULL, RET_ERR(PM_ERR_HANDLE_NOT_NULL, -1));

	handle = _alpm_handle_new();
	if(handle == NULL) {
		RET_ERR(PM_ERR_MEMORY, -1);
	}

	return(0);
}

/** Release the library.  This should be the last alpm call you make.
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_release()
{
	int dbs_left = 0;

	ALPM_LOG_FUNC;

	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	/* close local database */
	if(handle->db_local) {
		alpm_db_unregister(handle->db_local);
		handle->db_local = NULL;
	}
	/* and also sync ones */
	while((dbs_left = alpm_list_count(handle->dbs_sync)) > 0) {
		pmdb_t *db = (pmdb_t *)handle->dbs_sync->data;
		_alpm_log(PM_LOG_DEBUG, _("removing DB %s, %d remaining..."), db->treename, dbs_left);
		alpm_db_unregister(db);
		db = NULL;
	}

	_alpm_handle_free(handle);

	return(0);
}

/** @} */

/** @defgroup alpm_misc Miscellaneous Functions
 * @brief Various libalpm functions
 */

/* vim: set ts=2 sw=2 noet: */
