/*
 *  trans.c
 *
 *  Copyright (c) 2002-2007 by Judd Vinet <jvinet@zeroflux.org>
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
#include <sys/wait.h>
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
#include "cache.h"

/** \addtogroup alpm_trans Transaction Functions
 * @brief Functions to manipulate libalpm transactions
 * @{
 */

/** Initialize the transaction.
 * @param type type of the transaction
 * @param flags flags of the transaction (like nodeps, etc)
 * @param event event callback function pointer
 * @param conv question callback function pointer
 * @param progress progress callback function pointer
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_trans_init(pmtranstype_t type, pmtransflag_t flags,
                              alpm_trans_cb_event event, alpm_trans_cb_conv conv,
                              alpm_trans_cb_progress progress)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	ASSERT(handle->trans == NULL, RET_ERR(PM_ERR_TRANS_NOT_NULL, -1));

	/* lock db */
	handle->lckfd = _alpm_lckmk();
	if(handle->lckfd == -1) {
		RET_ERR(PM_ERR_HANDLE_LOCK, -1);
	}

	handle->trans = _alpm_trans_new();
	if(handle->trans == NULL) {
		RET_ERR(PM_ERR_MEMORY, -1);
	}

	return(_alpm_trans_init(handle->trans, type, flags, event, conv, progress));
}

/** Search for packages to upgrade and add them to the transaction.
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_trans_sysupgrade()
{
	pmtrans_t *trans;

	ALPM_LOG_FUNC;

	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	trans = handle->trans;
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_INITIALIZED, RET_ERR(PM_ERR_TRANS_NOT_INITIALIZED, -1));
	ASSERT(trans->type == PM_TRANS_TYPE_SYNC, RET_ERR(PM_ERR_TRANS_TYPE, -1));

	return(_alpm_trans_sysupgrade(trans));
}

/** Add a target to the transaction.
 * @param target the name of the target to add
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_trans_addtarget(char *target)
{
	pmtrans_t *trans;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	ASSERT(target != NULL && strlen(target) != 0, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	trans = handle->trans;
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_INITIALIZED, RET_ERR(PM_ERR_TRANS_NOT_INITIALIZED, -1));

	return(_alpm_trans_addtarget(trans, target));
}

/** Prepare a transaction.
 * @param data the address of an alpm_list where detailed description
 * of an error can be dumped (ie. list of conflicting files)
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_trans_prepare(alpm_list_t **data)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	ASSERT(data != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	ASSERT(handle->trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(handle->trans->state == STATE_INITIALIZED, RET_ERR(PM_ERR_TRANS_NOT_INITIALIZED, -1));

	return(_alpm_trans_prepare(handle->trans, data));
}

/** Commit a transaction.
 * @param data the address of an alpm_list where detailed description
 * of an error can be dumped (ie. list of conflicting files)
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_trans_commit(alpm_list_t **data)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	ASSERT(handle->trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(handle->trans->state == STATE_PREPARED, RET_ERR(PM_ERR_TRANS_NOT_PREPARED, -1));

	return(_alpm_trans_commit(handle->trans, data));
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

	_alpm_trans_free(trans);
	handle->trans = NULL;

	/* unlock db */
	if(handle->lckfd != -1) {
		while(close(handle->lckfd) == -1 && errno == EINTR);
		handle->lckfd = -1;
	}
	if(_alpm_lckrm()) {
		_alpm_log(PM_LOG_WARNING, _("could not remove lock file %s\n"),
				alpm_option_get_lockfile());
		alpm_logaction("warning: could not remove lock file %s\n",
				alpm_option_get_lockfile());
	}

	return(0);
}

/** @} */

pmtrans_t *_alpm_trans_new()
{
	pmtrans_t *trans;

	ALPM_LOG_FUNC;

	CALLOC(trans, 1, sizeof(pmtrans_t), RET_ERR(PM_ERR_MEMORY, NULL));

	trans->packages = NULL;
	trans->skip_add = NULL;
	trans->skip_remove = NULL;
	trans->type = 0;
	trans->flags = 0;
	trans->cb_event = NULL;
	trans->cb_conv = NULL;
	trans->cb_progress = NULL;
	trans->state = STATE_IDLE;

	return(trans);
}

void _alpm_trans_free(pmtrans_t *trans)
{
	ALPM_LOG_FUNC;

	if(trans == NULL) {
		return;
	}

	if(trans->type == PM_TRANS_TYPE_SYNC) {
		alpm_list_free_inner(trans->packages, (alpm_list_fn_free)_alpm_sync_free);
	} else {
		alpm_list_free_inner(trans->packages, (alpm_list_fn_free)_alpm_pkg_free);
	}
	alpm_list_free(trans->packages);

	FREELIST(trans->skip_add);
	FREELIST(trans->skip_remove);

	FREE(trans);
}

int _alpm_trans_init(pmtrans_t *trans, pmtranstype_t type, pmtransflag_t flags,
                     alpm_trans_cb_event event, alpm_trans_cb_conv conv,
                     alpm_trans_cb_progress progress)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));

	trans->type = type;
	trans->flags = flags;
	trans->cb_event = event;
	trans->cb_conv = conv;
	trans->cb_progress = progress;
	trans->state = STATE_INITIALIZED;

	return(0);
}

int _alpm_trans_sysupgrade(pmtrans_t *trans)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));

	return(_alpm_sync_sysupgrade(trans, handle->db_local, handle->dbs_sync,
				&(trans->packages)));
}

/** Add a target to the transaction.
 * @param trans the current transaction
 * @param target the name of the target to add
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int _alpm_trans_addtarget(pmtrans_t *trans, char *target)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(target != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	switch(trans->type) {
		case PM_TRANS_TYPE_UPGRADE:
			if(_alpm_add_loadtarget(trans, handle->db_local, target) == -1) {
				/* pm_errno is set by _alpm_add_loadtarget() */
				return(-1);
			}
		break;
		case PM_TRANS_TYPE_REMOVE:
		case PM_TRANS_TYPE_REMOVEUPGRADE:
			if(_alpm_remove_loadtarget(trans, handle->db_local, target) == -1) {
				/* pm_errno is set by _alpm_remove_loadtarget() */
				return(-1);
			}
		break;
		case PM_TRANS_TYPE_SYNC:
			if(_alpm_sync_addtarget(trans, handle->db_local, handle->dbs_sync, target) == -1) {
				/* pm_errno is set by _alpm_sync_loadtarget() */
				return(-1);
			}
		break;
	}

	return(0);
}

int _alpm_trans_prepare(pmtrans_t *trans, alpm_list_t **data)
{
	if(data) {
		*data = NULL;
	}

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));

	/* If there's nothing to do, return without complaining */
	if(trans->packages == NULL) {
		return(0);
	}

	switch(trans->type) {
		case PM_TRANS_TYPE_UPGRADE:
			if(_alpm_add_prepare(trans, handle->db_local, data) == -1) {
				/* pm_errno is set by _alpm_add_prepare() */
				return(-1);
			}
		break;
		case PM_TRANS_TYPE_REMOVE:
		case PM_TRANS_TYPE_REMOVEUPGRADE:
			if(_alpm_remove_prepare(trans, handle->db_local, data) == -1) {
				/* pm_errno is set by _alpm_remove_prepare() */
				return(-1);
			}
		break;
		case PM_TRANS_TYPE_SYNC:
			if(_alpm_sync_prepare(trans, handle->db_local, handle->dbs_sync, data) == -1) {
				/* pm_errno is set by _alpm_sync_prepare() */
				return(-1);
			}
		break;
	}

	trans->state = STATE_PREPARED;

	return(0);
}

int _alpm_trans_commit(pmtrans_t *trans, alpm_list_t **data)
{
	ALPM_LOG_FUNC;

	if(data!=NULL)
		*data = NULL;

	/* Sanity checks */
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));

	/* If there's nothing to do, return without complaining */
	if(trans->packages == NULL) {
		return(0);
	}

	trans->state = STATE_COMMITING;

	switch(trans->type) {
		case PM_TRANS_TYPE_UPGRADE:
			if(_alpm_add_commit(trans, handle->db_local) == -1) {
				/* pm_errno is set by _alpm_add_commit() */
				return(-1);
			}
		break;
		case PM_TRANS_TYPE_REMOVE:
		case PM_TRANS_TYPE_REMOVEUPGRADE:
			if(_alpm_remove_commit(trans, handle->db_local) == -1) {
				/* pm_errno is set by _alpm_remove_commit() */
				return(-1);
			}
		break;
		case PM_TRANS_TYPE_SYNC:
			if(_alpm_sync_commit(trans, handle->db_local, data) == -1) {
				/* pm_errno is set by _alpm_sync_commit() */
				return(-1);
			}
		break;
	}

	trans->state = STATE_COMMITED;

	return(0);
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
		fgets(line, 1024, fp);
		if(feof(fp)) {
			continue;
		}
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
	char cwd[PATH_MAX];
	char *scriptpath;
	pid_t pid;
	int clean_tmpdir = 0;
	int restore_cwd = 0;
	int retval = 0;

	ALPM_LOG_FUNC;

	if(access(installfn, R_OK)) {
		/* not found */
		_alpm_log(PM_LOG_DEBUG, "scriptlet '%s' not found\n", installfn);
		return(0);
	}

	/* NOTE: popen will use the PARENT's /bin/sh, not the chroot's */
	if(access("/bin/sh", X_OK)) {
		/* not found */
		_alpm_log(PM_LOG_ERROR, _("No /bin/sh in parent environment, aborting scriptlet\n"));
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
	if(!strcmp(script, "pre_upgrade") || !strcmp(script, "pre_install")) {
		if(_alpm_unpack(installfn, tmpdir, ".INSTALL")) {
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

	/* save the cwd so we can restore it later */
	if(getcwd(cwd, PATH_MAX) == NULL) {
		_alpm_log(PM_LOG_ERROR, _("could not get current working directory\n"));
	} else {
		restore_cwd = 1;
	}

	/* just in case our cwd was removed in the upgrade operation */
	if(chdir(root) != 0) {
		_alpm_log(PM_LOG_ERROR, _("could not change directory to %s (%s)\n"), root, strerror(errno));
		goto cleanup;
	}

	_alpm_log(PM_LOG_DEBUG, "executing %s script...\n", script);

	if(oldver) {
		snprintf(cmdline, PATH_MAX, ". %s; %s %s %s",
				scriptpath, script, ver, oldver);
	} else {
		snprintf(cmdline, PATH_MAX, ". %s; %s %s",
				scriptpath, script, ver);
	}
	_alpm_log(PM_LOG_DEBUG, "%s\n", cmdline);

	/* fork- parent and child each have seperate code blocks below */
	pid = fork();
	if(pid == -1) {
		_alpm_log(PM_LOG_ERROR, _("could not fork a new process (%s)\n"), strerror(errno));
		retval = 1;
		goto cleanup;
	}

	if(pid == 0) {
		FILE *pipe;
		/* this code runs for the child only (the actual chroot/exec) */
		_alpm_log(PM_LOG_DEBUG, "chrooting in %s\n", root);
		if(chroot(root) != 0) {
			_alpm_log(PM_LOG_ERROR, _("could not change the root directory (%s)\n"),
					strerror(errno));
			exit(1);
		}
		if(chdir("/") != 0) {
			_alpm_log(PM_LOG_ERROR, _("could not change directory to / (%s)\n"),
					strerror(errno));
			exit(1);
		}
		umask(0022);
		_alpm_log(PM_LOG_DEBUG, "executing \"%s\"\n", cmdline);
		/* execl("/bin/sh", "sh", "-c", cmdline, (char *)NULL); */
		pipe = popen(cmdline, "r");
		if(!pipe) {
			_alpm_log(PM_LOG_ERROR, _("call to popen failed (%s)"),
					strerror(errno));
			exit(1);
		}
		while(!feof(pipe)) {
			char line[PATH_MAX];
			if(fgets(line, PATH_MAX, pipe) == NULL)
				break;
			alpm_logaction("%s", line);
			EVENT(trans, PM_TRANS_EVT_SCRIPTLET_INFO, line, NULL);
		}
		retval = pclose(pipe);
		exit(WEXITSTATUS(retval));
	} else {
		/* this code runs for the parent only (wait on the child) */
		pid_t retpid;
		int status;
		while((retpid = waitpid(pid, &status, 0)) == -1 && errno == EINTR);
		if(retpid == -1) {
			_alpm_log(PM_LOG_ERROR, _("call to waitpid failed (%s)\n"),
			          strerror(errno));
			retval = 1;
			goto cleanup;
		} else {
			/* check the return status, make sure it is 0 (success) */
			if(WIFEXITED(status)) {
				_alpm_log(PM_LOG_DEBUG, "call to waitpid succeeded\n");
				if(WEXITSTATUS(status) != 0) {
					_alpm_log(PM_LOG_ERROR, _("scriptlet failed to execute correctly\n"));
					retval = 1;
				}
			}
		}
	}

cleanup:
	if(clean_tmpdir && _alpm_rmrf(tmpdir)) {
		_alpm_log(PM_LOG_WARNING, _("could not remove tmpdir %s\n"), tmpdir);
	}
	if(restore_cwd) {
		chdir(cwd);
	}

	return(retval);
}

pmtranstype_t SYMEXPORT alpm_trans_get_type()
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(-1));
	ASSERT(handle->trans != NULL, return(-1));

	return handle->trans->type;
}

unsigned int SYMEXPORT alpm_trans_get_flags()
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(-1));
	ASSERT(handle->trans != NULL, return(-1));

	return handle->trans->flags;
}

alpm_list_t SYMEXPORT * alpm_trans_get_pkgs()
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(handle->trans != NULL, return(NULL));

	return handle->trans->packages;
}
/* vim: set ts=2 sw=2 noet: */
