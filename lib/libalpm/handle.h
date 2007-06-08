/*
 *  handle.h
 * 
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
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
#ifndef _ALPM_HANDLE_H
#define _ALPM_HANDLE_H

#include <stdio.h> 
#include <sys/types.h>

#include "alpm_list.h"
#include "db.h"
#include "log.h"
#include "alpm.h"
#include "trans.h"

typedef enum _pmaccess_t {
	PM_ACCESS_RO,
	PM_ACCESS_RW
} pmaccess_t;

typedef struct _pmhandle_t {
	/* Internal */
	pmaccess_t access;
	uid_t uid;
	pmdb_t *db_local;
	alpm_list_t *dbs_sync; /* List of (pmdb_t *) */
	FILE *logfd;
	int lckfd;
	pmtrans_t *trans;
	
	/* options */
  alpm_cb_log logcb;				/* Log callback function */
	alpm_cb_download dlcb;    /* Download callback function */
	char *root;								/* Root path, default '/' */
	char *dbpath;							/* Base path to pacman's DBs */
	char *cachedir;						/* Base path to pacman's cache */
	char *logfile;						/* Name of the file to log to */ /*TODO is this used?*/
	char *lockfile;						/* Name of the lock file */
	unsigned short usesyslog;	/* Use syslog instead of logfile? */ /* TODO move to frontend */
	
	alpm_list_t *noupgrade;			/* List of packages NOT to be upgraded */
	alpm_list_t *noextract;			/* List of packages NOT to extract */ /*TODO is this used?*/
	alpm_list_t *ignorepkg;			/* List of packages to ignore */
	alpm_list_t *holdpkg;				/* List of packages which 'hold' pacman */

	time_t upgradedelay;			/* Amount of time to wait before upgrading a package */
	/* servers */
	char *xfercommand;				/* External download command */
	unsigned short nopassiveftp; /* Don't use PASV ftp connections */
} pmhandle_t;

extern pmhandle_t *handle;

pmhandle_t *_alpm_handle_new();
void _alpm_handle_free(pmhandle_t *handle);

#endif /* _ALPM_HANDLE_H */

/* vim: set ts=2 sw=2 noet: */
