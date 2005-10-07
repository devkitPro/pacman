/*
 *  trans.h
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
#ifndef _ALPM_TRANS_H
#define _ALPM_TRANS_H

enum {
	STATE_IDLE = 0,
	STATE_INITIALIZED,
	STATE_PREPARED,
	STATE_COMMITED
};

#include "alpm.h"

typedef struct __pmtrans_t {
	unsigned char type;
	unsigned char flags;
	unsigned char state;
	PMList *targets;     /* PMList of (char *) */
	PMList *packages;    /* PMList of (pmpkg_t *) or (pmsyncpkg_t *) */
	PMList *skiplist;    /* PMList of (char *) */
	alpm_trans_cb_event cb_event;
} pmtrans_t;

#define FREETRANS(p) \
do { \
	if(p) { \
		trans_free(p); \
		p = NULL; \
	} \
} while (0)
#define EVENT(t, e, d1, d2) \
do { \
	if((t) && (t)->cb_event) { \
		(t)->cb_event(e, d1, d2); \
	} \
} while(0)

pmtrans_t *trans_new();
void trans_free(pmtrans_t *trans);
int trans_init(pmtrans_t *trans, unsigned char type, unsigned char flags, alpm_trans_cb_event event);
int trans_sysupgrade(pmtrans_t *trans);
int trans_addtarget(pmtrans_t *trans, char *target);
int trans_prepare(pmtrans_t *trans, PMList **data);
int trans_commit(pmtrans_t *trans);

#endif /* _ALPM_TRANS_H */

/* vim: set ts=2 sw=2 noet: */
