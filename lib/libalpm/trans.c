/*
 *  trans.c
 * 
 *  Copyright (c) 2002-2005 by Judd Vinet <jvinet@zeroflux.org>
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
/* pacman */
#include "error.h"
#include "package.h"
#include "util.h"
#include "list.h"
#include "handle.h"
#include "add.h"
#include "remove.h"
#include "sync.h"
#include "alpm.h"

extern pmhandle_t *handle;

pmtrans_t *trans_new()
{
	pmtrans_t *trans;

	if((trans = (pmtrans_t *)malloc(sizeof(pmtrans_t))) == NULL) {
		return(NULL);
	}

	trans->targets = NULL;
	trans->packages = NULL;
	trans->type = 0;
	trans->flags = 0;
	trans->cb = NULL;
	trans->state = STATE_IDLE;

	return(trans);
}

void trans_free(pmtrans_t *trans)
{
	if(trans == NULL) {
		return;
	}

	FREELIST(trans->targets);
	if(trans->type == PM_TRANS_TYPE_SYNC) {
		PMList *i;
		for(i = trans->packages; i; i = i->next) {
			sync_free(i->data);
			i->data = NULL;
		}
		FREELIST(trans->packages);
	} else {
		FREELISTPKGS(trans->packages);
	}

	free(trans);
}

int trans_init(pmtrans_t *trans, unsigned char type, unsigned char flags, alpm_trans_cb cb)
{
	/* Sanity checks */
	if(trans == NULL) {
		RET_ERR(PM_ERR_TRANS_NULL, -1);
	}

	/* ORE
	perform sanity checks on type and flags:
	for instance, we can't set UPGRADE and FRESHEN at the same time */

	trans->type = type;
	trans->flags = flags;
	trans->cb = cb;
	trans->state = STATE_INITIALIZED;

	return(0);
}

int trans_addtarget(pmtrans_t *trans, char *target)
{
	/* Sanity checks */
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(target != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	if(pm_list_is_strin(target, trans->targets)) {
		RET_ERR(PM_ERR_TRANS_DUP_TARGET, -1);
	}

	switch(trans->type) {
		case PM_TRANS_TYPE_ADD:
		case PM_TRANS_TYPE_UPGRADE:
			if(add_loadtarget(handle->db_local, trans, target) == -1) {
				/* pm_errno is set by add_loadtarget() */
				return(-1);
			}
		break;
		case PM_TRANS_TYPE_REMOVE:
			if(remove_loadtarget(handle->db_local, trans, target) == -1) {
				/* pm_errno is set by remove_loadtarget() */
				return(-1);
			}
		break;
		case PM_TRANS_TYPE_SYNC:
			if(sync_addtarget(handle->db_local, handle->dbs_sync, trans, target) == -1) {
				/* pm_errno is set by add_loadtarget() */
				return(-1);
			}
		break;
	}
	trans->targets = pm_list_add(trans->targets, strdup(target));

	return(0);
}

int trans_prepare(pmtrans_t *trans, PMList **data)
{
	*data = NULL;

	/* Sanity checks */
	ASSERT(trans != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	switch(trans->type) {
		case PM_TRANS_TYPE_ADD:
		case PM_TRANS_TYPE_UPGRADE:
			if(add_prepare(handle->db_local, trans, data) == -1) {
				/* pm_errno is set by add_prepare() */
				return(-1);
			}
		break;
		case PM_TRANS_TYPE_REMOVE:
			if(remove_prepare(handle->db_local, trans, data) == -1) {
				/* pm_errno is set by remove_prepare() */
				return(-1);
			}
		break;
		case PM_TRANS_TYPE_SYNC:
			if(sync_prepare(handle->db_local, trans, data) == -1) {
				/* pm_errno is set by sync_prepare() */
				return(-1);
			}
		break;
	}

	trans->state = STATE_PREPARED;

	return(0);
}

int trans_commit(pmtrans_t *trans)
{
	/* Sanity checks */
	ASSERT(trans != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	/* If there's nothing to do, return without complaining */
	if(trans->packages == NULL) {
		return(0);
	}

	switch(trans->type) {
		case PM_TRANS_TYPE_ADD:
		case PM_TRANS_TYPE_UPGRADE:
			if(add_commit(handle->db_local, trans) == -1) {
				return(-1);
			}
		break;
		case PM_TRANS_TYPE_REMOVE:
			if(remove_commit(handle->db_local, trans) == -1) {
				return(-1);
			}
		break;
		case PM_TRANS_TYPE_SYNC:
			if(sync_commit(handle->db_local, trans) == -1) {
				return(-1);
			}
		break;
	}

	trans->state = STATE_COMMITED;

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
