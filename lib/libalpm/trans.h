/*
 *  trans.h
 * 
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2005 by Christian Hamar <krics@linuxforum.hu>
 *  Copyright (c) 2006 by Miklos Vajna <vmiklos@frugalware.org>
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
	STATE_DOWNLOADING,
	STATE_COMMITING,
	STATE_COMMITED,
	STATE_INTERRUPTED
};

#include "alpm.h"

typedef struct __pmtrans_t {
	unsigned char type;
	unsigned int flags;
	unsigned char state;
	PMList *targets;     /* PMList of (char *) */
	PMList *packages;    /* PMList of (pmpkg_t *) or (pmsyncpkg_t *) */
	PMList *skiplist;    /* PMList of (char *) */
	alpm_trans_cb_event cb_event;
	alpm_trans_cb_conv cb_conv;
	alpm_trans_cb_progress cb_progress;
} pmtrans_t;

#define FREETRANS(p) \
do { \
	if(p) { \
		_alpm_trans_free(p); \
		p = NULL; \
	} \
} while (0)
#define EVENT(t, e, d1, d2) \
do { \
	if((t) && (t)->cb_event) { \
		(t)->cb_event(e, d1, d2); \
	} \
} while(0)
#define QUESTION(t, q, d1, d2, d3, r) \
do { \
	if((t) && (t)->cb_conv) { \
		(t)->cb_conv(q, d1, d2, d3, r); \
	} \
} while(0)
#define PROGRESS(t, e, p, per, h, r) \
do { \
	if((t) && (t)->cb_progress) { \
		(t)->cb_progress(e, p, per, h, r); \
	} \
} while(0)

pmtrans_t *_alpm_trans_new(void);
void _alpm_trans_free(void *data);
int _alpm_trans_init(pmtrans_t *trans, unsigned char type, unsigned int flags, alpm_trans_cb_event event, alpm_trans_cb_conv conv, alpm_trans_cb_progress progress);
int _alpm_trans_sysupgrade(pmtrans_t *trans);
int _alpm_trans_addtarget(pmtrans_t *trans, char *target);
int _alpm_trans_prepare(pmtrans_t *trans, PMList **data);
int _alpm_trans_commit(pmtrans_t *trans, PMList **data);

#endif /* _ALPM_TRANS_H */

/* vim: set ts=2 sw=2 noet: */
