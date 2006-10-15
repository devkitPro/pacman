/*
 *  handle.c
 * 
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
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
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <stdarg.h>
#include <syslog.h>
#include <libintl.h>
#include <time.h>
#include <ftplib.h>
/* pacman */
#include "util.h"
#include "log.h"
#include "list.h"
#include "error.h"
#include "trans.h"
#include "alpm.h"
#include "handle.h"

/* log */
extern alpm_cb_log pm_logcb;
extern FtpCallback pm_dlcb;
extern unsigned char pm_logmask;
/* progress bar */
extern char *pm_dlfnm;
extern int *pm_dloffset;
extern struct timeval *pm_dlt0, *pm_dlt;
extern float *pm_dlrate;
extern int *pm_dlxfered1;
extern unsigned char *pm_dleta_h, *pm_dleta_m, *pm_dleta_s;

pmhandle_t *_alpm_handle_new()
{
	pmhandle_t *handle;

	handle = (pmhandle_t *)malloc(sizeof(pmhandle_t));
	if(handle == NULL) {
		_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(pmhandle_t));
		RET_ERR(PM_ERR_MEMORY, NULL);
	}

	memset(handle, 0, sizeof(pmhandle_t));
	handle->lckfd = -1;

#ifndef CYGWIN
	/* see if we're root or not */
	handle->uid = geteuid();
#ifndef FAKEROOT
	if(!handle->uid && getenv("FAKEROOTKEY")) {
		/* fakeroot doesn't count, we're non-root */
		handle->uid = 99;
	}
#endif

	/* see if we're root or not (fakeroot does not count) */
#ifndef FAKEROOT
	if(handle->uid == 0 && !getenv("FAKEROOTKEY")) {
#else
	if(handle->uid == 0) {
#endif
		handle->access = PM_ACCESS_RW;
	} else {
		handle->access = PM_ACCESS_RO;
	}
#else
	handle->access = PM_ACCESS_RW;
#endif

	handle->dbpath = strdup(PM_DBPATH);
	handle->cachedir = strdup(PM_CACHEDIR);

	return(handle);
}

int _alpm_handle_free(pmhandle_t *handle)
{
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

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
	FREE(handle->cachedir);
	FREE(handle->logfile);
	FREE(handle->proxyhost);
	FREE(handle->xfercommand);
	FREELIST(handle->dbs_sync);
	FREELIST(handle->noupgrade);
	FREELIST(handle->noextract);
	FREELIST(handle->ignorepkg);
	FREELIST(handle->holdpkg);
	FREELIST(handle->needles);
	free(handle);

	return(0);
}

int _alpm_handle_set_option(pmhandle_t *handle, unsigned char val, unsigned long data)
{
	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	char *p;
	switch(val) {
		case PM_OPT_DBPATH:
			if(handle->dbpath) {
				FREE(handle->dbpath);
			}
			handle->dbpath = strdup((data && strlen((char *)data) != 0) ? (char *)data : PM_DBPATH);
			_alpm_log(PM_LOG_FLOW2, _("PM_OPT_DBPATH set to '%s'"), handle->dbpath);
		break;
		case PM_OPT_CACHEDIR:
			if(handle->cachedir) {
				FREE(handle->cachedir);
			}
			handle->cachedir = strdup((data && strlen((char *)data) != 0) ? (char *)data : PM_CACHEDIR);
			_alpm_log(PM_LOG_FLOW2, _("PM_OPT_CACHEDIR set to '%s'"), handle->cachedir);
		break;
		case PM_OPT_LOGFILE:
			if((char *)data == NULL || handle->uid != 0) {
				return(0);
			}
			if(handle->logfile) {
				FREE(handle->logfile);
			}
			if(handle->logfd) {
				if(fclose(handle->logfd) != 0) {
					handle->logfd = NULL;
					RET_ERR(PM_ERR_OPT_LOGFILE, -1);
				}
				handle->logfd = NULL;
			}
			if((handle->logfd = fopen((char *)data, "a")) == NULL) {
				_alpm_log(PM_LOG_ERROR, _("can't open log file %s"), (char *)data);
				RET_ERR(PM_ERR_OPT_LOGFILE, -1);
			}
			handle->logfile = strdup((char *)data);
			_alpm_log(PM_LOG_FLOW2, _("PM_OPT_LOGFILE set to '%s'"), (char *)data);
		break;
		case PM_OPT_NOUPGRADE:
			if((char *)data && strlen((char *)data) != 0) {
				handle->noupgrade = _alpm_list_add(handle->noupgrade, strdup((char *)data));
				_alpm_log(PM_LOG_FLOW2, _("'%s' added to PM_OPT_NOUPGRADE"), (char *)data);
			} else {
				FREELIST(handle->noupgrade);
				_alpm_log(PM_LOG_FLOW2, _("PM_OPT_NOUPGRADE flushed"));
			}
		break;
		case PM_OPT_NOEXTRACT:
			if((char *)data && strlen((char *)data) != 0) {
				handle->noextract = _alpm_list_add(handle->noextract, strdup((char *)data));
				_alpm_log(PM_LOG_FLOW2, _("'%s' added to PM_OPT_NOEXTRACT"), (char *)data);
			} else {
				FREELIST(handle->noextract);
				_alpm_log(PM_LOG_FLOW2, _("PM_OPT_NOEXTRACT flushed"));
			}
		break;
		case PM_OPT_IGNOREPKG:
			if((char *)data && strlen((char *)data) != 0) {
				handle->ignorepkg = _alpm_list_add(handle->ignorepkg, strdup((char *)data));
				_alpm_log(PM_LOG_FLOW2, _("'%s' added to PM_OPT_IGNOREPKG"), (char *)data);
			} else {
				FREELIST(handle->ignorepkg);
				_alpm_log(PM_LOG_FLOW2, _("PM_OPT_IGNOREPKG flushed"));
			}
		break;
		case PM_OPT_HOLDPKG:
			if((char *)data && strlen((char *)data) != 0) {
				handle->holdpkg = _alpm_list_add(handle->holdpkg, strdup((char *)data));
				_alpm_log(PM_LOG_FLOW2, _("'%s' added to PM_OPT_HOLDPKG"), (char *)data);
			} else {
				FREELIST(handle->holdpkg);
				_alpm_log(PM_LOG_FLOW2, _("PM_OPT_HOLDPKG flushed"));
			}
		break;
		case PM_OPT_NEEDLES:
			if((char *)data && strlen((char *)data) != 0) {
				handle->needles = _alpm_list_add(handle->needles, strdup((char *)data));
				_alpm_log(PM_LOG_FLOW2, _("'%s' added to PM_OPT_NEEDLES"), (char *)data);
			} else {
				FREELIST(handle->needles);
				_alpm_log(PM_LOG_FLOW2, _("PM_OPT_NEEDLES flushed"));
			}
		break;
		case PM_OPT_USESYSLOG:
			if(data != 0 && data != 1) {
				RET_ERR(PM_ERR_OPT_USESYSLOG, -1);
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
			_alpm_log(PM_LOG_FLOW2, _("PM_OPT_USESYSLOG set to '%d'"), handle->usesyslog);
		break;
		case PM_OPT_LOGCB:
			pm_logcb = (alpm_cb_log)data;
		break;
		case PM_OPT_DLCB:
			pm_dlcb = (FtpCallback)data;
		break;
		case PM_OPT_DLFNM:
			pm_dlfnm = (char *)data;
		break;
		case PM_OPT_DLOFFSET:
			pm_dloffset = (int *)data;
		break;
		case PM_OPT_DLT0:
			pm_dlt0 = (struct timeval *)data;
		break;
		case PM_OPT_DLT:
			pm_dlt = (struct timeval *)data;
		break;
		case PM_OPT_DLRATE:
			pm_dlrate = (float *)data;
		break;
		case PM_OPT_DLXFERED1:
			pm_dlxfered1 = (int *)data;
		break;
		case PM_OPT_DLETA_H:
			pm_dleta_h = (unsigned char *)data;
		break;
		case PM_OPT_DLETA_M:
			pm_dleta_m = (unsigned char *)data;
		break;
		case PM_OPT_DLETA_S:
			pm_dleta_s = (unsigned char *)data;
		break;
		case PM_OPT_UPGRADEDELAY:
			handle->upgradedelay = data;
		break;
		case PM_OPT_LOGMASK:
			pm_logmask = (unsigned char)data;
			_alpm_log(PM_LOG_FLOW2, _("PM_OPT_LOGMASK set to '%02x'"), (unsigned char)data);
		break;
		case PM_OPT_PROXYHOST:
			if(handle->proxyhost) {
				FREE(handle->proxyhost);
			}
			p = strstr((char*)data, "://");
			if(p) {
				p += 3;
				if(p == NULL || *p == '\0') {
					RET_ERR(PM_ERR_SERVER_BAD_LOCATION, -1);
				}
				data = (long)p;
			}
#if defined(__APPLE__) || defined(__OpenBSD__)
			handle->proxyhost = strdup((char*)data);
#else
			handle->proxyhost = strndup((char*)data, PATH_MAX);
#endif
			_alpm_log(PM_LOG_FLOW2, _("PM_OPT_PROXYHOST set to '%s'"), handle->proxyhost);
		break;
		case PM_OPT_PROXYPORT:
			handle->proxyport = (unsigned short)data;
			_alpm_log(PM_LOG_FLOW2, _("PM_OPT_PROXYPORT set to '%d'"), handle->proxyport);
		break;
		case PM_OPT_XFERCOMMAND:
			if(handle->xfercommand) {
				FREE(handle->xfercommand);
			}
#if defined(__APPLE__) || defined(__OpenBSD__)
			handle->xfercommand = strdup((char*)data);
#else
			handle->xfercommand = strndup((char*)data, PATH_MAX);
#endif
			_alpm_log(PM_LOG_FLOW2, _("PM_OPT_XFERCOMMAND set to '%s'"), handle->xfercommand);
		break;
		case PM_OPT_NOPASSIVEFTP:
			handle->nopassiveftp = (unsigned short)data;
			_alpm_log(PM_LOG_FLOW2, _("PM_OPT_NOPASSIVEFTP set to '%d'"), handle->nopassiveftp);
		break;
		case PM_OPT_CHOMP:
			handle->chomp = (unsigned short)data;
			_alpm_log(PM_LOG_FLOW2, _("PM_OPT_CHOMP set to '%d'"), handle->chomp);
		break;
		default:
			RET_ERR(PM_ERR_WRONG_ARGS, -1);
	}

	return(0);
}

int _alpm_handle_get_option(pmhandle_t *handle, unsigned char val, long *data)
{
	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	switch(val) {
		case PM_OPT_ROOT:      *data = (long)handle->root; break;
		case PM_OPT_DBPATH:    *data = (long)handle->dbpath; break;
		case PM_OPT_CACHEDIR:  *data = (long)handle->cachedir; break;
		case PM_OPT_LOCALDB:   *data = (long)handle->db_local; break;
		case PM_OPT_SYNCDB:    *data = (long)handle->dbs_sync; break;
		case PM_OPT_LOGFILE:   *data = (long)handle->logfile; break;
		case PM_OPT_NOUPGRADE: *data = (long)handle->noupgrade; break;
		case PM_OPT_NOEXTRACT: *data = (long)handle->noextract; break;
		case PM_OPT_IGNOREPKG: *data = (long)handle->ignorepkg; break;
		case PM_OPT_HOLDPKG:   *data = (long)handle->holdpkg; break;
		case PM_OPT_NEEDLES:   *data = (long)handle->needles; break;
		case PM_OPT_USESYSLOG: *data = handle->usesyslog; break;
		case PM_OPT_LOGCB:     *data = (long)pm_logcb; break;
		case PM_OPT_DLCB:     *data = (long)pm_dlcb; break;
		case PM_OPT_UPGRADEDELAY: *data = (long)handle->upgradedelay; break;
		case PM_OPT_LOGMASK:   *data = pm_logmask; break;
		case PM_OPT_DLFNM:     *data = (long)pm_dlfnm; break;
		case PM_OPT_DLOFFSET:  *data = (long)pm_dloffset; break;
		case PM_OPT_DLT0:      *data = (long)pm_dlt0; break;
		case PM_OPT_DLT:       *data = (long)pm_dlt; break;
		case PM_OPT_DLRATE:    *data = (long)pm_dlrate; break;
		case PM_OPT_DLXFERED1: *data = (long)pm_dlxfered1; break;
		case PM_OPT_DLETA_H:   *data = (long)pm_dleta_h; break;
		case PM_OPT_DLETA_M:   *data = (long)pm_dleta_m; break;
		case PM_OPT_DLETA_S:   *data = (long)pm_dleta_s; break;
		case PM_OPT_PROXYHOST: *data = (long)handle->proxyhost; break;
		case PM_OPT_PROXYPORT: *data = handle->proxyport; break;
		case PM_OPT_XFERCOMMAND: *data = (long)handle->xfercommand; break;
		case PM_OPT_NOPASSIVEFTP: *data = handle->nopassiveftp; break;
		case PM_OPT_CHOMP: *data = handle->chomp; break;
		default:
			RET_ERR(PM_ERR_WRONG_ARGS, -1);
		break;
	}

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
