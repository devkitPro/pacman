/*
 *  trans.c
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <errno.h>

/* libalpm */
#include "trans.h"
#include "alpm_list.h"
#include "package.h"
#include "util.h"
#include "log.h"
#include "handle.h"
#include "add.h"
#include "remove.h"
#include "sync.h"
#include "alpm.h"
#include "deps.h"

/** \addtogroup alpm_trans Transaction Functions
 * @brief Functions to manipulate libalpm transactions
 * @{
 */

/** Initialize the transaction.
 * @param flags flags of the transaction (like nodeps, etc)
 * @param event event callback function pointer
 * @param conv question callback function pointer
 * @param progress progress callback function pointer
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_trans_init(pmtransflag_t flags,
                              alpm_trans_cb_event event, alpm_trans_cb_conv conv,
                              alpm_trans_cb_progress progress)
{
	pmtrans_t *trans;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	ASSERT(handle->trans == NULL, RET_ERR(PM_ERR_TRANS_NOT_NULL, -1));

	/* lock db */
	if(!(flags & PM_TRANS_FLAG_NOLOCK)) {
		handle->lckfd = _alpm_lckmk();
		if(handle->lckfd == -1) {
			RET_ERR(PM_ERR_HANDLE_LOCK, -1);
		}
	}

	trans = _alpm_trans_new();
	if(trans == NULL) {
		RET_ERR(PM_ERR_MEMORY, -1);
	}

	trans->flags = flags;
	trans->cb_event = event;
	trans->cb_conv = conv;
	trans->cb_progress = progress;
	trans->state = STATE_INITIALIZED;

	handle->trans = trans;

	return(0);
}

static alpm_list_t *check_arch(alpm_list_t *pkgs)
{
	alpm_list_t *i;
	alpm_list_t *invalid = NULL;

	const char *arch = alpm_option_get_arch();
	if(!arch) {
		return(NULL);
	}
	for(i = pkgs; i; i = i->next) {
		pmpkg_t *pkg = i->data;
		const char *pkgarch = alpm_pkg_get_arch(pkg);
		if(strcmp(pkgarch,arch) != 0 && strcmp(pkgarch,"any") != 0) {
			char *string;
			const char *pkgname = alpm_pkg_get_name(pkg);
			const char *pkgver = alpm_pkg_get_version(pkg);
			size_t len = strlen(pkgname) + strlen(pkgver) + strlen(pkgarch) + 3;
			MALLOC(string, len, RET_ERR(PM_ERR_MEMORY, invalid));
			sprintf(string, "%s-%s-%s", pkgname, pkgver, pkgarch);
			invalid = alpm_list_add(invalid, string);
		}
	}
	return(invalid);
}

/** Prepare a transaction.
 * @param data the address of an alpm_list where detailed description
 * of an error can be dumped (ie. list of conflicting files)
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_trans_prepare(alpm_list_t **data)
{
	pmtrans_t *trans;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	ASSERT(data != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	trans = handle->trans;

	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_INITIALIZED, RET_ERR(PM_ERR_TRANS_NOT_INITIALIZED, -1));

	/* If there's nothing to do, return without complaining */
	if(trans->add == NULL && trans->remove == NULL) {
		return(0);
	}

	alpm_list_t *invalid = check_arch(trans->add);
	if(invalid) {
		if(data) {
			*data = invalid;
		}
		RET_ERR(PM_ERR_PKG_INVALID_ARCH, -1);
	}

	if(trans->add == NULL) {
		if(_alpm_remove_prepare(trans, handle->db_local, data) == -1) {
			/* pm_errno is set by _alpm_remove_prepare() */
			return(-1);
		}
	}	else {
		if(_alpm_sync_prepare(trans, handle->db_local, handle->dbs_sync, data) == -1) {
			/* pm_errno is set by _alpm_sync_prepare() */
			return(-1);
		}
	}

	trans->state = STATE_PREPARED;

	return(0);
}

/** Commit a transaction.
 * @param data the address of an alpm_list where detailed description
 * of an error can be dumped (ie. list of conflicting files)
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_trans_commit(alpm_list_t **data)
{
	pmtrans_t *trans;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	trans = handle->trans;

	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_PREPARED, RET_ERR(PM_ERR_TRANS_NOT_PREPARED, -1));

	ASSERT(!(trans->flags & PM_TRANS_FLAG_NOLOCK), RET_ERR(PM_ERR_TRANS_NOT_LOCKED, -1));

	/* If there's nothing to do, return without complaining */
	if(trans->add == NULL && trans->remove == NULL) {
		return(0);
	}

	trans->state = STATE_COMMITING;

	if(trans->add == NULL) {
		if(_alpm_remove_packages(trans, handle->db_local) == -1) {
			/* pm_errno is set by _alpm_remove_commit() */
			return(-1);
		}
	} else {
		if(_alpm_sync_commit(trans, handle->db_local, data) == -1) {
			/* pm_errno is set by _alpm_sync_commit() */
			return(-1);
		}
	}

	trans->state = STATE_COMMITED;

	return(0);
}

/** Interrupt a transaction.
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_trans_interrupt()
{
	pmtrans_t *trans;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	trans = handle->trans;
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_COMMITING || trans->state == STATE_INTERRUPTED,
			RET_ERR(PM_ERR_TRANS_TYPE, -1));

	trans->state = STATE_INTERRUPTED;

	return(0);
}

/** Release a transaction.
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_trans_release()
{
	pmtrans_t *trans;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	trans = handle->trans;
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state != STATE_IDLE, RET_ERR(PM_ERR_TRANS_NULL, -1));

	int nolock_flag = trans->flags & PM_TRANS_FLAG_NOLOCK;

	_alpm_trans_free(trans);
	handle->trans = NULL;

	/* unlock db */
	if(!nolock_flag) {
		if(handle->lckfd != -1) {
			int fd;
			do {
				fd = close(handle->lckfd);
			} while(fd == -1 && errno == EINTR);
			handle->lckfd = -1;
		}
		if(_alpm_lckrm()) {
			_alpm_log(PM_LOG_WARNING, _("could not remove lock file %s\n"),
					alpm_option_get_lockfile());
			alpm_logaction("warning: could not remove lock file %s\n",
					alpm_option_get_lockfile());
		}
	}

	return(0);
}

/** @} */

pmtrans_t *_alpm_trans_new()
{
	pmtrans_t *trans;

	ALPM_LOG_FUNC;

	CALLOC(trans, 1, sizeof(pmtrans_t), RET_ERR(PM_ERR_MEMORY, NULL));
	trans->state = STATE_IDLE;

	return(trans);
}

void _alpm_trans_free(pmtrans_t *trans)
{
	ALPM_LOG_FUNC;

	if(trans == NULL) {
		return;
	}

	alpm_list_free_inner(trans->add, (alpm_list_fn_free)_alpm_pkg_free_trans);
	alpm_list_free(trans->add);
	alpm_list_free_inner(trans->remove, (alpm_list_fn_free)_alpm_pkg_free);
	alpm_list_free(trans->remove);

	FREELIST(trans->skip_add);
	FREELIST(trans->skip_remove);

	FREE(trans);
}

/* A cheap grep for text files, returns 1 if a substring
 * was found in the text file fn, 0 if it wasn't
 */
static int grep(const char *fn, const char *needle)
{
	FILE *fp;

	if((fp = fopen(fn, "r")) == NULL) {
		return(0);
	}
	while(!feof(fp)) {
		char line[1024];
		if(fgets(line, sizeof(line), fp) == NULL) {
			continue;
		}
		/* TODO: this will not work if the search string
		 * ends up being split across line reads */
		if(strstr(line, needle)) {
			fclose(fp);
			return(1);
		}
	}
	fclose(fp);
	return(0);
}

int _alpm_runscriptlet(const char *root, const char *installfn,
											 const char *script, const char *ver,
											 const char *oldver, pmtrans_t *trans)
{
	char scriptfn[PATH_MAX];
	char cmdline[PATH_MAX];
	char tmpdir[PATH_MAX];
	char *argv[] = { "sh", "-c", cmdline, NULL };
	char *scriptpath;
	int clean_tmpdir = 0;
	int retval = 0;

	ALPM_LOG_FUNC;

	if(access(installfn, R_OK)) {
		/* not found */
		_alpm_log(PM_LOG_DEBUG, "scriptlet '%s' not found\n", installfn);
		return(0);
	}

	/* creates a directory in $root/tmp/ for copying/extracting the scriptlet */
	snprintf(tmpdir, PATH_MAX, "%stmp/", root);
	if(access(tmpdir, F_OK) != 0) {
		_alpm_makepath_mode(tmpdir, 01777);
	}
	snprintf(tmpdir, PATH_MAX, "%stmp/alpm_XXXXXX", root);
	if(mkdtemp(tmpdir) == NULL) {
		_alpm_log(PM_LOG_ERROR, _("could not create temp directory\n"));
		return(1);
	} else {
		clean_tmpdir = 1;
	}

	/* either extract or copy the scriptlet */
	snprintf(scriptfn, PATH_MAX, "%s/.INSTALL", tmpdir);
	if(strcmp(script, "pre_upgrade") == 0 || strcmp(script, "pre_install") == 0) {
		if(_alpm_unpack_single(installfn, tmpdir, ".INSTALL")) {
			retval = 1;
		}
	} else {
		if(_alpm_copyfile(installfn, scriptfn)) {
			_alpm_log(PM_LOG_ERROR, _("could not copy tempfile to %s (%s)\n"), scriptfn, strerror(errno));
			retval = 1;
		}
	}
	if(retval == 1) {
		goto cleanup;
	}

	/* chop off the root so we can find the tmpdir in the chroot */
	scriptpath = scriptfn + strlen(root) - 1;

	if(!grep(scriptfn, script)) {
		/* script not found in scriptlet file */
		goto cleanup;
	}

	if(oldver) {
		snprintf(cmdline, PATH_MAX, ". %s; %s %s %s",
				scriptpath, script, ver, oldver);
	} else {
		snprintf(cmdline, PATH_MAX, ". %s; %s %s",
				scriptpath, script, ver);
	}

	_alpm_log(PM_LOG_DEBUG, "executing \"%s\"\n", cmdline);

	retval = _alpm_run_chroot(root, "/bin/sh", argv);

cleanup:
	if(clean_tmpdir && _alpm_rmrf(tmpdir)) {
		_alpm_log(PM_LOG_WARNING, _("could not remove tmpdir %s\n"), tmpdir);
	}

	return(retval);
}

int SYMEXPORT alpm_trans_get_flags()
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(-1));
	ASSERT(handle->trans != NULL, return(-1));

	return handle->trans->flags;
}

alpm_list_t SYMEXPORT * alpm_trans_get_add()
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(handle->trans != NULL, return(NULL));

	return handle->trans->add;
}

alpm_list_t SYMEXPORT * alpm_trans_get_remove()
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(handle->trans != NULL, return(NULL));

	return handle->trans->remove;
}
/* vim: set ts=2 sw=2 noet: */
