/*
 *  conf.h
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
#ifndef _PM_CONF_H
#define _PM_CONF_H

typedef struct __config_t {
	/* command line options */
	char *root;
	char *dbpath;
	char *cachedir;
	char *configfile;
	unsigned short op;
	unsigned short verbose;
	unsigned short version;
	unsigned short help;
	unsigned short upgrade;
	unsigned short noconfirm;
	unsigned short op_d_vertest;
	unsigned short op_d_resolve;
	unsigned short op_q_isfile;
	unsigned short op_q_info;
	unsigned short op_q_list;
	unsigned short op_q_orphans;
	unsigned short op_q_owns;
	unsigned short op_q_search;
	unsigned short op_s_clean;
	unsigned short op_s_downloadonly;
	list_t *op_s_ignore;
	unsigned short op_s_info;
	unsigned short op_s_printuris;
	unsigned short op_s_sync;
	unsigned short op_s_search;
	unsigned short op_s_upgrade;
	unsigned short group;
	unsigned char  flags;
	unsigned short debug;
	/* configuration file option */
	char *proxyhost;
	unsigned short proxyport;
	char *xfercommand;
	unsigned short chomp;
	unsigned short nopassiveftp;
	list_t *holdpkg;
} config_t;

#define FREECONF(p) do { if(p) { config_free(p); p = NULL; } } while(0)

config_t *config_new();
int config_free(config_t *config);
int parseconfig(char *file, config_t *config);

#endif /* _PM_CONF_H */

/* vim: set ts=2 sw=2 noet: */
