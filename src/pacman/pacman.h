/*
 *  pacman.h
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
#ifndef _PM_PACMAN_H
#define _PM_PACMAN_H

#ifndef PACCONF
#define PACCONF  "/etc/pacman.conf"
#endif

/* Operations */
#define PM_OP_MAIN    1
#define PM_OP_ADD     2
#define PM_OP_REMOVE  3
#define PM_OP_UPGRADE 4
#define PM_OP_QUERY   5
#define PM_OP_SYNC    6
#define PM_OP_DEPTEST 7

void cleanup(int signum);

int pacman_deptest(list_t *targets);

int parseargs(int argc, char **argv);

void usage(int op, char *myname);

void version();

char *buildstring(list_t *strlist);

#endif /* _PM_PACMAN_H */

/* vim: set ts=2 sw=2 noet: */
