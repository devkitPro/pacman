/*
 *  trans.c
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>

/* libalpm */
#include "trans.h"
#include "alpm_list.h"
#include "package.h"
#include "util.h"
#include "log.h"
#include "handle.h"
#include "remove.h"
#include "sync.h"
#include "alpm.h"

/** \addtogroup alpm_trans Transaction Functions
 * @brief Functions to manipulate libalpm transactions
 * @{
 */

/* Create a lock file */
static int make_lock(pmhandle_t *handle)
{
	int fd;
	char *dir, *ptr;

	ASSERT(handle->lockfile != NULL, return -1);

	/* create the dir of the lockfile first */
	dir = strdup(handle->lockfile);
	ptr = strrchr(dir, '/');
	if(ptr) {
		*ptr = '\0';
	}
	if(_alpm_makepath(dir)) {
		FREE(dir);
		return -1;
	}
	FREE(dir);

	do {
		fd = open(handle->lockfile, O_WRONLY | O_CREAT | O_EXCL, 0000);
	} while(fd == -1 && errno == EINTR);
	if(fd > 0) {
		FILE *f = fdopen(fd, "w");
		fprintf(f, "%ld\n", (long)getpid());
		fflush(f);
		fsync(fd);
		handle->lckstream = f;
		return 0;
	}
	return -1;
}

/* Remove a lock file */
static int remove_lock(pmhandle_t *handle)
{
	if(handle->lckstream != NULL) {
		fclose(handle->lckstream);
		handle->lckstream = NULL;
	}
	if(unlink(handle->lockfile) == -1 && errno != ENOENT) {
		return -1;
	}
	return 0;
}

/** Initialize the transaction. */
int SYMEXPORT alpm_trans_init(pmhandle_t *handle, pmtransflag_t flags,
		alpm_trans_cb_event event, alpm_trans_cb_conv conv,
		alpm_trans_cb_progress progress)
{
	pmtrans_t *trans;
	const int required_db_version = 2;
	int db_version;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	ASSERT(handle->trans == NULL, RET_ERR(PM_ERR_TRANS_NOT_NULL, -1));

	/* lock db */
	if(!(flags & PM_TRANS_FLAG_NOLOCK)) {
		if(make_lock(handle)) {
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

	/* check database version */
	db_version = _alpm_db_version(handle->db_local);
	if(db_version < required_db_version) {
		_alpm_log(PM_LOG_ERROR,
				_("%s database version is too old\n"), handle->db_local->treename);
		remove_lock(handle);
		_alpm_trans_free(trans);
		RET_ERR(PM_ERR_DB_VERSION, -1);
	}

	return 0;
}

static alpm_list_t *check_arch(pmhandle_t *handle, alpm_list_t *pkgs)
{
	alpm_list_t *i;
	alpm_list_t *invalid = NULL;

	const char *arch = alpm_option_get_arch(handle);
	if(!arch) {
		return NULL;
	}
	for(i = pkgs; i; i = i->next) {
		pmpkg_t *pkg = i->data;
		const char *pkgarch = alpm_pkg_get_arch(pkg);
		if(pkgarch && strcmp(pkgarch, arch) && strcmp(pkgarch, "any")) {
			char *string;
			const char *pkgname = alpm_pkg_get_name(pkg);
			const char *pkgver = alpm_pkg_get_version(pkg);
			size_t len = strlen(pkgname) + strlen(pkgver) + strlen(pkgarch) + 3;
			MALLOC(string, len, RET_ERR(PM_ERR_MEMORY, invalid));
			sprintf(string, "%s-%s-%s", pkgname, pkgver, pkgarch);
			invalid = alpm_list_add(invalid, string);
		}
	}
	return invalid;
}

/** Prepare a transaction. */
int SYMEXPORT alpm_trans_prepare(pmhandle_t *handle, alpm_list_t **data)
{
	pmtrans_t *trans;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	ASSERT(data != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	trans = handle->trans;

	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_INITIALIZED, RET_ERR(PM_ERR_TRANS_NOT_INITIALIZED, -1));

	/* If there's nothing to do, return without complaining */
	if(trans->add == NULL && trans->remove == NULL) {
		return 0;
	}

	alpm_list_t *invalid = check_arch(handle, trans->add);
	if(invalid) {
		if(data) {
			*data = invalid;
		}
		RET_ERR(PM_ERR_PKG_INVALID_ARCH, -1);
	}

	if(trans->add == NULL) {
		if(_alpm_remove_prepare(handle, data) == -1) {
			/* pm_errno is set by _alpm_remove_prepare() */
			return -1;
		}
	}	else {
		if(_alpm_sync_prepare(handle, data) == -1) {
			/* pm_errno is set by _alpm_sync_prepare() */
			return -1;
		}
	}

	trans->state = STATE_PREPARED;

	return 0;
}

/** Commit a transaction. */
int SYMEXPORT alpm_trans_commit(pmhandle_t *handle, alpm_list_t **data)
{
	pmtrans_t *trans;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	trans = handle->trans;

	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_PREPARED, RET_ERR(PM_ERR_TRANS_NOT_PREPARED, -1));

	ASSERT(!(trans->flags & PM_TRANS_FLAG_NOLOCK), RET_ERR(PM_ERR_TRANS_NOT_LOCKED, -1));

	/* If there's nothing to do, return without complaining */
	if(trans->add == NULL && trans->remove == NULL) {
		return 0;
	}

	trans->state = STATE_COMMITING;

	if(trans->add == NULL) {
		if(_alpm_remove_packages(handle) == -1) {
			/* pm_errno is set by _alpm_remove_commit() */
			return -1;
		}
	} else {
		if(_alpm_sync_commit(handle, data) == -1) {
			/* pm_errno is set by _alpm_sync_commit() */
			return -1;
		}
	}

	trans->state = STATE_COMMITED;

	return 0;
}

/** Interrupt a transaction. */
int SYMEXPORT alpm_trans_interrupt(pmhandle_t *handle)
{
	pmtrans_t *trans;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	trans = handle->trans;
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_COMMITING || trans->state == STATE_INTERRUPTED,
			RET_ERR(PM_ERR_TRANS_TYPE, -1));

	trans->state = STATE_INTERRUPTED;

	return 0;
}

/** Release a transaction. */
int SYMEXPORT alpm_trans_release(pmhandle_t *handle)
{
	pmtrans_t *trans;

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
		if(remove_lock(handle)) {
			_alpm_log(PM_LOG_WARNING, _("could not remove lock file %s\n"),
					alpm_option_get_lockfile(handle));
			alpm_logaction(handle, "warning: could not remove lock file %s\n",
					alpm_option_get_lockfile(handle));
		}
	}

	return 0;
}

/** @} */

pmtrans_t *_alpm_trans_new(void)
{
	pmtrans_t *trans;

	CALLOC(trans, 1, sizeof(pmtrans_t), RET_ERR(PM_ERR_MEMORY, NULL));
	trans->state = STATE_IDLE;

	return trans;
}

void _alpm_trans_free(pmtrans_t *trans)
{
	if(trans == NULL) {
		return;
	}

	alpm_list_free_inner(trans->add, (alpm_list_fn_free)_alpm_pkg_free_trans);
	alpm_list_free(trans->add);
	alpm_list_free_inner(trans->remove, (alpm_list_fn_free)_alpm_pkg_free);
	alpm_list_free(trans->remove);

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
		return 0;
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
			return 1;
		}
	}
	fclose(fp);
	return 0;
}

int _alpm_runscriptlet(pmhandle_t *handle, const char *installfn,
		const char *script, const char *ver, const char *oldver)
{
	char scriptfn[PATH_MAX];
	char cmdline[PATH_MAX];
	char tmpdir[PATH_MAX];
	char *argv[] = { "sh", "-c", cmdline, NULL };
	char *scriptpath;
	int clean_tmpdir = 0;
	int retval = 0;

	if(access(installfn, R_OK)) {
		/* not found */
		_alpm_log(PM_LOG_DEBUG, "scriptlet '%s' not found\n", installfn);
		return 0;
	}

	/* creates a directory in $root/tmp/ for copying/extracting the scriptlet */
	snprintf(tmpdir, PATH_MAX, "%stmp/", handle->root);
	if(access(tmpdir, F_OK) != 0) {
		_alpm_makepath_mode(tmpdir, 01777);
	}
	snprintf(tmpdir, PATH_MAX, "%stmp/alpm_XXXXXX", handle->root);
	if(mkdtemp(tmpdir) == NULL) {
		_alpm_log(PM_LOG_ERROR, _("could not create temp directory\n"));
		return 1;
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
	scriptpath = scriptfn + strlen(handle->root) - 1;

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

	retval = _alpm_run_chroot(handle, "/bin/sh", argv);

cleanup:
	if(clean_tmpdir && _alpm_rmrf(tmpdir)) {
		_alpm_log(PM_LOG_WARNING, _("could not remove tmpdir %s\n"), tmpdir);
	}

	return retval;
}

pmtransflag_t SYMEXPORT alpm_trans_get_flags(pmhandle_t *handle)
{
	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	ASSERT(handle->trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));

	return handle->trans->flags;
}

alpm_list_t SYMEXPORT *alpm_trans_get_add(pmhandle_t *handle)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return NULL);
	ASSERT(handle->trans != NULL, return NULL);

	return handle->trans->add;
}

alpm_list_t SYMEXPORT *alpm_trans_get_remove(pmhandle_t *handle)
{
	/* Sanity checks */
	ASSERT(handle != NULL, return NULL);
	ASSERT(handle->trans != NULL, return NULL);

	return handle->trans->remove;
}
/* vim: set ts=2 sw=2 noet: */
