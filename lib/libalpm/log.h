/*
 *  log.h
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
#ifndef _ALPM_LOG_H
#define _ALPM_LOG_H

typedef void (*alpm_cb_log)(unsigned short, char *);

void _alpm_log(unsigned char flag, char *fmt, ...);

int _alpm_log_action(unsigned char usesyslog, FILE *f, char *fmt, ...);

#endif /* _ALPM_LOG_H */

/* vim: set ts=2 sw=2 noet: */
