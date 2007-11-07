/*
 *  conf.h
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
#ifndef _PM_CONF_H
#define _PM_CONF_H

#include <alpm.h>

typedef struct __config_t {
	char *configfile;
	unsigned short op;
	unsigned short quiet;
	unsigned short verbose;
	unsigned short version;
	unsigned short help;
	unsigned short upgrade;
	unsigned short noconfirm;
	unsigned short noprogressbar;
	unsigned short logmask;
	/* keep track if we had paths specified on command line */
	unsigned short have_root;
	unsigned short have_dbpath;
	unsigned short have_logfile;

	unsigned short op_q_isfile;
	unsigned short op_q_info;
	unsigned short op_q_list;
	unsigned short op_q_foreign;
	unsigned short op_q_orphans;
	unsigned short op_q_deps;
	unsigned short op_q_explicit;
	unsigned short op_q_owns;
	unsigned short op_q_search;
	unsigned short op_q_changelog;
	unsigned short op_q_upgrade;

	unsigned short op_s_clean;
	unsigned short op_s_dependsonly;
	unsigned short op_s_downloadonly;
	unsigned short op_s_info;
	unsigned short op_s_sync;
	unsigned short op_s_search;
	unsigned short op_s_upgrade;

	unsigned short group;
	pmtransflag_t flags;

	/* conf file options */
	unsigned short chomp; /* I Love Candy! */
	unsigned short usecolor; /* enable colorful output */
	unsigned short showsize; /* show individual package sizes */
	unsigned short totaldownload; /* When downloading, display the amount
	                                 downloaded, rate, ETA, and percent
	                                 downloaded of the total download list */
} config_t;

/* Operations */
enum {
	PM_OP_MAIN = 1,
	PM_OP_ADD,
	PM_OP_REMOVE,
	PM_OP_UPGRADE,
	PM_OP_QUERY,
	PM_OP_SYNC,
	PM_OP_DEPTEST
};

/* global config variable */
extern config_t *config;

config_t *config_new(void);
int config_free(config_t *oldconfig);

#endif /* _PM_CONF_H */

/* vim: set ts=2 sw=2 noet: */
