/*
 *  trans.h
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
#ifndef _PM_TRANS_H
#define _PM_TRANS_H

/* callback to handle messages/notifications from pacman transactions */
void cb_trans_evt(unsigned char event, void *data1, void *data2);

/* callback to handle questions from pacman transactions (yes/no) */
void cb_trans_conv(unsigned char event, void *data1, void *data2, void *data3, int *response);

void cb_trans_progress(unsigned char event, char *pkgname, int percent, int howmany, int remain);

#endif /* _PM_TRANS_H */

/* vim: set ts=2 sw=2 noet: */
