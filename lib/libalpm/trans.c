/*
 *  trans.c
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <errno.h>
#ifndef __sun__
#include <mntent.h>
#endif

/* libalpm */
#include "trans.h"
#include "alpm_list.h"
#include "error.h"
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
#include "provide.h"

pmtrans_t *_alpm_trans_new()
{
	pmtrans_t *trans;

	ALPM_LOG_FUNC;

	if((trans = malloc(sizeof(pmtrans_t))) == NULL) {
		_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(pmtrans_t));
		return(NULL);
	}

	trans->targets = NULL;
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

	FREELIST(trans->targets);
	if(trans->type == PM_TRANS_TYPE_SYNC) {
		alpm_list_t *i;
		for(i = trans->packages; i; i = alpm_list_next(i)) {
			_alpm_sync_free(i->data);
			i->data = NULL;
		}
		FREELIST(trans->packages);
	} else {
		alpm_list_t *tmp;
		for(tmp = trans->packages; tmp; tmp = alpm_list_next(tmp)) {
			_alpm_pkg_free(tmp->data);
			tmp->data = NULL;
		}
	}
	trans->packages = NULL;

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

	return(_alpm_sync_sysupgrade(trans, handle->db_local, handle->dbs_sync));
}

int _alpm_trans_addtarget(pmtrans_t *trans, char *target)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(target != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	if(alpm_list_find_str(trans->targets, target)) {
		return(0);
		//RET_ERR(PM_ERR_TRANS_DUP_TARGET, -1);
	}

	switch(trans->type) {
		case PM_TRANS_TYPE_ADD:
		case PM_TRANS_TYPE_UPGRADE:
			if(_alpm_add_loadtarget(trans, handle->db_local, target) == -1) {
				/* pm_errno is set by _alpm_add_loadtarget() */
				return(-1);
			}
		break;
		case PM_TRANS_TYPE_REMOVE:
			if(_alpm_remove_loadtarget(trans, handle->db_local, target) == -1) {
				/* pm_errno is set by remove_loadtarget() */
				return(-1);
			}
		break;
		case PM_TRANS_TYPE_SYNC:
			if(_alpm_sync_addtarget(trans, handle->db_local, handle->dbs_sync, target) == -1) {
				/* pm_errno is set by sync_loadtarget() */
				return(-1);
			}
		break;
	}

	trans->targets = alpm_list_add(trans->targets, strdup(target));

	return(0);
}

int _alpm_trans_prepare(pmtrans_t *trans, alpm_list_t **data)
{
	*data = NULL;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));

	/* If there's nothing to do, return without complaining */
	if(trans->packages == NULL) {
		return(0);
	}

	switch(trans->type) {
		case PM_TRANS_TYPE_ADD:
		case PM_TRANS_TYPE_UPGRADE:
			if(_alpm_add_prepare(trans, handle->db_local, data) == -1) {
				/* pm_errno is set by _alpm_add_prepare() */
				return(-1);
			}
		break;
		case PM_TRANS_TYPE_REMOVE:
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
		case PM_TRANS_TYPE_ADD:
		case PM_TRANS_TYPE_UPGRADE:
			if(_alpm_add_commit(trans, handle->db_local) == -1) {
				/* pm_errno is set by _alpm_add_prepare() */
				return(-1);
			}
		break;
		case PM_TRANS_TYPE_REMOVE:
			if(_alpm_remove_commit(trans, handle->db_local) == -1) {
				/* pm_errno is set by _alpm_remove_prepare() */
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

int _alpm_trans_update_depends(pmtrans_t *trans, pmpkg_t *pkg)
{
	alpm_list_t *i, *j;
	alpm_list_t *depends = NULL;
	const char *pkgname;
	pmdb_t *localdb;

	ALPM_LOG_FUNC;
		
	/* Sanity checks */
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(pkg != NULL, RET_ERR(PM_ERR_PKG_INVALID, -1));

	pkgname = alpm_pkg_get_name(pkg);
	depends = alpm_pkg_get_depends(pkg);

	if(depends) {
		_alpm_log(PM_LOG_DEBUG, _("updating dependency packages 'requiredby' fields for %s-%s"),
		          pkgname, pkg->version);
	} else {
		_alpm_log(PM_LOG_DEBUG, _("package has no dependencies, no other packages to update"));
	}

	localdb = alpm_option_get_localdb();
	for(i = depends; i; i = i->next) {
		pmdepend_t* dep = alpm_splitdep(i->data);
		if(dep == NULL) {
			continue;
		}
	
		if(trans->packages && trans->type == PM_TRANS_TYPE_REMOVE) {
			if(_alpm_pkg_find(dep->name, handle->trans->packages)) {
				continue;
			}
		}

		pmpkg_t *deppkg = _alpm_db_get_pkgfromcache(localdb, dep->name);
		if(!deppkg) {
			int found_provides = 0;
			/* look for a provides package */
			alpm_list_t *provides = _alpm_db_whatprovides(localdb, dep->name);
			for(j = provides; j; j = j->next) {
				if(!j->data) {
					continue;
				}
				pmpkg_t *provpkg = j->data;
				deppkg = _alpm_db_get_pkgfromcache(localdb, alpm_pkg_get_name(provpkg));

				if(!deppkg) {
					continue;
				}

				found_provides = 1;

				/* this is cheating... we call this function to populate the package */
				alpm_list_t *rqdby = alpm_pkg_get_requiredby(deppkg);

				_alpm_log(PM_LOG_DEBUG, _("updating 'requiredby' field for package '%s'"),
				          alpm_pkg_get_name(deppkg));
				if(trans->type == PM_TRANS_TYPE_REMOVE) {
					void *data = NULL;
					rqdby = alpm_list_remove(rqdby,	pkgname, _alpm_str_cmp, &data);
					FREE(data);
					deppkg->requiredby = rqdby;
				} else {
					if(!alpm_list_find_str(rqdby, pkgname)) {
						rqdby = alpm_list_add(rqdby, strdup(pkgname));
						deppkg->requiredby = rqdby;
					}
				}

				if(_alpm_db_write(localdb, deppkg, INFRQ_DEPENDS)) {
					_alpm_log(PM_LOG_ERROR, _("could not update 'requiredby' database entry %s-%s"),
										alpm_pkg_get_name(deppkg), alpm_pkg_get_version(deppkg));
				}
			}
			alpm_list_free(provides);

			if(!found_provides) {
				_alpm_log(PM_LOG_DEBUG, _("could not find dependency '%s'"), dep->name);
				continue;
			}
		}

		/* this is cheating... we call this function to populate the package */
		alpm_list_t *rqdby = alpm_pkg_get_requiredby(deppkg);

		_alpm_log(PM_LOG_DEBUG, _("updating 'requiredby' field for package '%s'"),
		          alpm_pkg_get_name(deppkg));
		if(trans->type == PM_TRANS_TYPE_REMOVE) {
			void *data = NULL;
			rqdby = alpm_list_remove(rqdby, pkgname, _alpm_str_cmp, &data);
			FREE(data);
			deppkg->requiredby = rqdby;
		} else {
			if(!alpm_list_find_str(rqdby, pkgname)) {
				rqdby = alpm_list_add(rqdby, strdup(pkgname));
				deppkg->requiredby = rqdby;
			}
		}

		if(_alpm_db_write(localdb, deppkg, INFRQ_DEPENDS)) {
			_alpm_log(PM_LOG_ERROR, _("could not update 'requiredby' database entry %s-%s"),
								alpm_pkg_get_name(deppkg), alpm_pkg_get_version(deppkg));
		}
		free(dep);
	}
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
	char tmpdir[PATH_MAX] = "";
	char *scriptpath;
	struct stat buf;
	char cwd[PATH_MAX] = "";
	pid_t pid;
	int retval = 0;

	ALPM_LOG_FUNC;

	if(stat(installfn, &buf)) {
		/* not found */
		_alpm_log(PM_LOG_DEBUG, "scriptlet '%s' not found", installfn);
		return(0);
	}

	if(!strcmp(script, "pre_upgrade") || !strcmp(script, "pre_install")) {
		snprintf(tmpdir, PATH_MAX, "%stmp/", root);
		if(stat(tmpdir, &buf)) {
			_alpm_makepath(tmpdir);
		}
		snprintf(tmpdir, PATH_MAX, "%stmp/alpm_XXXXXX", root);
		if(mkdtemp(tmpdir) == NULL) {
			_alpm_log(PM_LOG_ERROR, _("could not create temp directory"));
			return(1);
		}
		_alpm_unpack(installfn, tmpdir, ".INSTALL");
		snprintf(scriptfn, PATH_MAX, "%s/.INSTALL", tmpdir);
		/* chop off the root so we can find the tmpdir in the chroot */
		scriptpath = scriptfn + strlen(root) - 1;
	} else {
		strncpy(scriptfn, installfn, PATH_MAX);
		/* chop off the root so we can find the tmpdir in the chroot */
		scriptpath = scriptfn + strlen(root) - 1;
	}

	if(!grep(scriptfn, script)) {
		/* script not found in scriptlet file */
		goto cleanup;
	}

	/* save the cwd so we can restore it later */
	if(getcwd(cwd, PATH_MAX) == NULL) {
		_alpm_log(PM_LOG_ERROR, _("could not get current working directory"));
		/* in case of error, cwd content is undefined: so we set it to something */
		cwd[0] = 0;
	}

	/* just in case our cwd was removed in the upgrade operation */
	if(chdir(root) != 0) {
		_alpm_log(PM_LOG_ERROR, _("could not change directory to %s (%s)"), root, strerror(errno));
		goto cleanup;
	}

	_alpm_log(PM_LOG_DEBUG, _("executing %s script..."), script);

	if(oldver) {
		snprintf(cmdline, PATH_MAX, "source %s %s %s %s",
				scriptpath, script, ver, oldver);
	} else {
		snprintf(cmdline, PATH_MAX, "source %s %s %s",
				scriptpath, script, ver);
	}
	_alpm_log(PM_LOG_DEBUG, "%s", cmdline);

	pid = fork();
	if(pid == -1) {
		_alpm_log(PM_LOG_ERROR, _("could not fork a new process (%s)"), strerror(errno));
		retval = 1;
		goto cleanup;
	}

	if(pid == 0) {
		FILE *pp;
		_alpm_log(PM_LOG_DEBUG, _("chrooting in %s"), root);
		if(chroot(root) != 0) {
			_alpm_log(PM_LOG_ERROR, _("could not change the root directory (%s)"), strerror(errno));
			return(1);
		}
		if(chdir("/") != 0) {
			_alpm_log(PM_LOG_ERROR, _("could not change directory to / (%s)"), strerror(errno));
			return(1);
		}
		umask(0022);
		_alpm_log(PM_LOG_DEBUG, _("executing \"%s\""), cmdline);
		pp = popen(cmdline, "r");
		if(!pp) {
			_alpm_log(PM_LOG_ERROR, _("call to popen failed (%s)"), strerror(errno));
			retval = 1;
			goto cleanup;
		}
		while(!feof(pp)) {
			char line[1024];
			if(fgets(line, 1024, pp) == NULL)
				break;
			/*TODO clean this code up, remove weird SCRIPTLET_START/DONE,
			 * (void*)atol call, etc. */
			/* "START <event desc>" */
			if((strlen(line) > strlen(SCRIPTLET_START))
			   && !strncmp(line, SCRIPTLET_START, strlen(SCRIPTLET_START))) {
				EVENT(trans, PM_TRANS_EVT_SCRIPTLET_START,
				      _alpm_strtrim(line + strlen(SCRIPTLET_START)), NULL);
			/* "DONE <ret code>" */
			} else if((strlen(line) > strlen(SCRIPTLET_DONE))
			          && !strncmp(line, SCRIPTLET_DONE, strlen(SCRIPTLET_DONE))) {
				EVENT(trans, PM_TRANS_EVT_SCRIPTLET_DONE,
				      (void*)atol(_alpm_strtrim(line + strlen(SCRIPTLET_DONE))),
				      NULL);
			} else {
				_alpm_strtrim(line);
				/* log our script output */
				alpm_logaction(line);
				EVENT(trans, PM_TRANS_EVT_SCRIPTLET_INFO, line, NULL);
			}
		}
		pclose(pp);
		exit(0);
	} else {
		if(waitpid(pid, 0, 0) == -1) {
			_alpm_log(PM_LOG_ERROR, _("call to waitpid failed (%s)"),
			          strerror(errno));
			retval = 1;
			goto cleanup;
		}
	}

cleanup:
	if(strlen(tmpdir) && _alpm_rmrf(tmpdir)) {
		_alpm_log(PM_LOG_WARNING, _("could not remove tmpdir %s"), tmpdir);
	}
	if(strlen(cwd)) {
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

alpm_list_t SYMEXPORT * alpm_trans_get_targets()
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(handle->trans != NULL, return(NULL));

	return handle->trans->targets;
}

alpm_list_t SYMEXPORT * alpm_trans_get_pkgs()
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(handle->trans != NULL, return(NULL));

	return handle->trans->packages;
}
/* vim: set ts=2 sw=2 noet: */
