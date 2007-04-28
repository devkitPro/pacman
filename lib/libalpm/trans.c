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

	if((trans = (pmtrans_t *)malloc(sizeof(pmtrans_t))) == NULL) {
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


pmtranstype_t alpm_trans_get_type()
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

alpm_list_t * alpm_trans_get_targets()
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
