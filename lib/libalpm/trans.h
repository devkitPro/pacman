/*
 *  trans.h
 *
 *  Copyright (c) 2006-2010 Pacman Development Team <pacman-dev@archlinux.org>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _ALPM_TRANS_H
#define _ALPM_TRANS_H

#include "alpm.h"

typedef enum _pmtransstate_t {
	STATE_IDLE = 0,
	STATE_INITIALIZED,
	STATE_PREPARED,
	STATE_DOWNLOADING,
	STATE_COMMITING,
	STATE_COMMITED,
	STATE_INTERRUPTED
} pmtransstate_t;

/* Transaction */
struct __pmtrans_t {
	pmtransflag_t flags;
	pmtransstate_t state;
	alpm_list_t *add;      /* list of (pmpkg_t *) */
	alpm_list_t *remove;      /* list of (pmpkg_t *) */
	alpm_list_t *skip_add;      /* list of (char *) */
	alpm_list_t *skip_remove;   /* list of (char *) */
	alpm_trans_cb_event cb_event;
	alpm_trans_cb_conv cb_conv;
	alpm_trans_cb_progress cb_progress;
};

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
void _alpm_trans_free(pmtrans_t *trans);
int _alpm_trans_init(pmtrans_t *trans, pmtransflag_t flags,
                     alpm_trans_cb_event event, alpm_trans_cb_conv conv,
                     alpm_trans_cb_progress progress);
int _alpm_runscriptlet(const char *root, const char *installfn,
                       const char *script, const char *ver,
                       const char *oldver, pmtrans_t *trans);

#endif /* _ALPM_TRANS_H */

/* vim: set ts=2 sw=2 noet: */
