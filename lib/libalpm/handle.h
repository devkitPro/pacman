/*
 *  handle.h
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
#ifndef _ALPM_HANDLE_H
#define _ALPM_HANDLE_H

#include "list.h"
#include "db.h"
#include "trans.h"
#include "alpm.h"

typedef enum __pmaccess_t {
	PM_ACCESS_RO,
	PM_ACCESS_RW
} pmaccess_t;

typedef struct __pmhandle_t {
	pmaccess_t access;
	uid_t uid;
	pmdb_t *db_local;
	PMList *dbs_sync; /* List of (pmdb_t *) */
	FILE *logfd;
	int lckfd;
	pmtrans_t *trans;
	/* parameters */
	char *root;
	char *dbpath;
	char *cachedir;
	char *logfile;
	PMList *noupgrade; /* List of strings */
	PMList *noextract; /* List of strings */
	PMList *ignorepkg; /* List of strings */
	unsigned char usesyslog;
} pmhandle_t;

#define FREEHANDLE(p) do { if (p) { handle_free(p); p = NULL; } } while (0)

pmhandle_t *handle_new(void);
int handle_free(pmhandle_t *handle);
int handle_set_option(pmhandle_t *handle, unsigned char val, unsigned long data);
int handle_get_option(pmhandle_t *handle, unsigned char val, long *data);

#endif /* _ALPM_HANDLE_H */

/* vim: set ts=2 sw=2 noet: */
