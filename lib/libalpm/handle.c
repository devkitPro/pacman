/*
 *  handle.c
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
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <stdarg.h>
#include <syslog.h>
/* pacman */
#include "util.h"
#include "log.h"
#include "list.h"
#include "error.h"
#include "trans.h"
#include "alpm.h"
#include "handle.h"

/* log */
extern alpm_cb_log __pm_logcb;
extern unsigned char __pm_logmask;

pmhandle_t *handle_new()
{
	pmhandle_t *handle;

	handle = (pmhandle_t *)malloc(sizeof(pmhandle_t));
	if(handle == NULL) {
		PM_RET_ERR(PM_ERR_MEMORY, NULL);
	}

	/* see if we're root or not */
	handle->uid = geteuid();
	if(!handle->uid && getenv("FAKEROOTKEY")) {
		/* fakeroot doesn't count, we're non-root */
		handle->uid = 99;
	}

	/* see if we're root or not (fakeroot does not count) */
	if(getuid() == 0 && !getenv("FAKEROOTKEY")) {
		handle->access = PM_ACCESS_RW;
	} else {
		handle->access = PM_ACCESS_RO;
	}

	handle->trans = NULL;

	handle->db_local = NULL;
	handle->dbs_sync = NULL;

	handle->logfd = NULL;

	handle->root = NULL;
	handle->dbpath = NULL;
	handle->logfile = NULL;
	handle->noupgrade = NULL;
	handle->ignorepkg = NULL;
	handle->usesyslog = 0;

	return(handle);
}

int handle_free(pmhandle_t *handle)
{
	ASSERT(handle != NULL, PM_RET_ERR(PM_ERR_HANDLE_NULL, -1));

	/* close logfiles */
	if(handle->logfd) {
		fclose(handle->logfd);
		handle->logfd = NULL;
	}
	if(handle->usesyslog) {
		handle->usesyslog = 0;
		closelog();
	}

	/* free memory */
	FREETRANS(handle->trans);
	FREE(handle->root);
	FREE(handle->dbpath);
	FREE(handle->logfile);
	FREELIST(handle->dbs_sync);
	FREELIST(handle->noupgrade);
	FREELIST(handle->ignorepkg);
	free(handle);

	return(0);
}

int handle_set_option(pmhandle_t *handle, unsigned char val, unsigned long data)
{
	PMList *lp;
	char str[PATH_MAX];

	/* Sanity checks */
	ASSERT(handle != NULL, PM_RET_ERR(PM_ERR_HANDLE_NULL, -1));

	switch(val) {
		case PM_OPT_DBPATH:
			if(handle->db_local) {
				PM_RET_ERR(PM_ERR_DB_NOT_NULL, -1);
			}
			for(lp = handle->dbs_sync; lp; lp = lp->next) {
				if(lp->data) {
					PM_RET_ERR(PM_ERR_DB_NOT_NULL, -1);
				}
			}

			if(handle->trans && handle->trans->state != STATE_IDLE) {
				PM_RET_ERR(PM_ERR_TRANS_INITIALIZED, -1);
			}

			strncpy(str, ((char *)data) ? (char *)data : PM_DBPATH, PATH_MAX);
			handle->dbpath = strdup(str);
			_alpm_log(PM_LOG_FLOW2, "PM_OPT_DBPATH set to '%s'", handle->dbpath);
		break;
		case PM_OPT_LOGFILE:
			if((char *)data == NULL || getuid() != 0) {
				return(0);
			}
			if(handle->logfile) {
				FREE(handle->logfile);
			}
			if(handle->logfd) {
				if(fclose(handle->logfd) != 0) {
					handle->logfd = NULL;
					PM_RET_ERR(PM_ERR_OPT_LOGFILE, -1);
				}
				handle->logfd = NULL;
			}
			if((handle->logfd = fopen((char *)data, "a")) == NULL) {
				_alpm_log(PM_LOG_ERROR, "can't open log file %s", (char *)data);
				PM_RET_ERR(PM_ERR_OPT_LOGFILE, -1);
			}
			handle->logfile = strdup((char *)data);
			_alpm_log(PM_LOG_FLOW2, "PM_OPT_LOGFILE set to '%s'", (char *)data);
		break;
		case PM_OPT_NOUPGRADE:
			if((char *)data && strlen((char *)data) != 0) {
				handle->noupgrade = pm_list_add(handle->noupgrade, strdup((char *)data));
				_alpm_log(PM_LOG_FLOW2, "'%s' added to PM_OPT_NOUPGRADE", (char *)data);
			} else {
				FREELIST(handle->noupgrade);
				_alpm_log(PM_LOG_FLOW2, "PM_OPT_NOUPGRADE flushed");
			}
		break;
		case PM_OPT_IGNOREPKG:
			if((char *)data && strlen((char *)data) != 0) {
				handle->ignorepkg = pm_list_add(handle->ignorepkg, strdup((char *)data));
				_alpm_log(PM_LOG_FLOW2, "'%s' added to PM_OPT_IGNOREPKG", (char *)data);
			} else {
				FREELIST(handle->ignorepkg);
				_alpm_log(PM_LOG_FLOW2, "PM_OPT_IGNOREPKG flushed");
			}
		break;
		case PM_OPT_USESYSLOG:
			if(data != 0 && data != 1) {
				PM_RET_ERR(PM_ERR_OPT_USESYSLOG, -1);
			}
			if(handle->usesyslog == data) {
				return(0);
			}
			if(handle->usesyslog) {
				closelog();
			} else {
				openlog("alpm", 0, LOG_USER);
			}
			handle->usesyslog = (unsigned short)data;
			_alpm_log(PM_LOG_FLOW2, "PM_OPT_USESYSLOG set to '%d'", handle->usesyslog);
		break;
		case PM_OPT_LOGCB:
			__pm_logcb = (alpm_cb_log)data;
		break;
		case PM_OPT_LOGMASK:
			__pm_logmask = (unsigned char)data;
			_alpm_log(PM_LOG_FLOW2, "PM_OPT_LOGMASK set to '%02x'", (unsigned char)data);
		break;
		default:
			PM_RET_ERR(PM_ERR_WRONG_ARGS, -1);
	}

	return(0);
}

int handle_get_option(pmhandle_t *handle, unsigned char val, long *data)
{
	/* Sanity checks */
	ASSERT(handle != NULL, PM_RET_ERR(PM_ERR_HANDLE_NULL, -1));

	switch(val) {
		case PM_OPT_ROOT:      *data = (long)handle->root; break;
		case PM_OPT_DBPATH:    *data = (long)handle->dbpath; break;
		case PM_OPT_LOCALDB:   *data = (long)handle->db_local; break;
		case PM_OPT_SYNCDB:    *data = (long)handle->dbs_sync; break;
		case PM_OPT_LOGFILE:   *data = (long)handle->logfile; break;
		case PM_OPT_NOUPGRADE: *data = (long)handle->noupgrade; break;
		case PM_OPT_IGNOREPKG: *data = (long)handle->ignorepkg; break;
		case PM_OPT_USESYSLOG: *data = handle->usesyslog; break;
		case PM_OPT_LOGCB:     *data = (long)__pm_logcb; break;
		case PM_OPT_LOGMASK:   *data = __pm_logmask; break;
		default:
			PM_RET_ERR(PM_ERR_WRONG_ARGS, -1);
		break;
	}

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
