/*
 *  alpm.c
 *
 *  Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
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

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif

/* libalpm */
#include "alpm.h"
#include "alpm_list.h"
#include "handle.h"
#include "log.h"
#include "util.h"

/* Globals */
enum _pmerrno_t pm_errno SYMEXPORT;
extern pmhandle_t *handle;

/** \addtogroup alpm_interface Interface Functions
 * @brief Functions to initialize and release libalpm
 * @{
 */

/** Initializes the library.  This must be called before any other
 * functions are called.
 * @param root the root path for all filesystem operations
 * @param dbpath the absolute path to the libalpm database
 * @param err an optional variable to hold any error return codes
 * @return a context handle on success, NULL on error, err will be set if provided
 */
pmhandle_t SYMEXPORT *alpm_initialize(const char *root, const char *dbpath,
		enum _pmerrno_t *err)
{
	enum _pmerrno_t myerr;
	const char *lf = "db.lck";
	size_t lockfilelen;
	pmhandle_t *myhandle = _alpm_handle_new();

	if(myhandle == NULL) {
		myerr = PM_ERR_MEMORY;
		goto cleanup;
	}
	if((myerr = _alpm_set_directory_option(root, &(myhandle->root), 1))) {
		goto cleanup;
	}
	if((myerr = _alpm_set_directory_option(dbpath, &(myhandle->dbpath), 1))) {
		goto cleanup;
	}

	lockfilelen = strlen(myhandle->dbpath) + strlen(lf) + 1;
	myhandle->lockfile = calloc(lockfilelen, sizeof(char));
	snprintf(myhandle->lockfile, lockfilelen, "%s%s", myhandle->dbpath, lf);

	if(_alpm_db_register_local(myhandle) == NULL) {
		myerr = PM_ERR_DB_CREATE;
		goto cleanup;
	}

#ifdef ENABLE_NLS
	bindtextdomain("libalpm", LOCALEDIR);
#endif

#ifdef HAVE_LIBCURL
	curl_global_init(CURL_GLOBAL_SSL);
	myhandle->curl = curl_easy_init();
#endif

	/* TODO temporary until global var removed */
	handle = myhandle;
	return myhandle;

cleanup:
	_alpm_handle_free(myhandle);
	if(err && myerr) {
		*err = myerr;
	}
	return NULL;
}

/** Release the library.  This should be the last alpm call you make.
 * After this returns, handle should be considered invalid and cannot be reused
 * in any way.
 * @param handle the context handle
 * @return 0 on success, -1 on error
 */
int SYMEXPORT alpm_release(pmhandle_t *myhandle)
{
	pmdb_t *db;

	ASSERT(myhandle != NULL, return -1);

	/* close local database */
	db = myhandle->db_local;
	if(db) {
		db->ops->unregister(db);
		myhandle->db_local = NULL;
	}

	if(alpm_db_unregister_all(myhandle) == -1) {
		return -1;
	}

	_alpm_handle_free(myhandle);
	myhandle = NULL;
	/* TODO temporary until global var removed */
	handle = NULL;

#ifdef HAVE_LIBCURL
	curl_global_cleanup();
#endif

	return 0;
}

/** @} */

/** @defgroup alpm_misc Miscellaneous Functions
 * @brief Various libalpm functions
 */

/* Get the version of library */
const char SYMEXPORT *alpm_version(void) {
	return LIB_VERSION;
}

/* vim: set ts=2 sw=2 noet: */
