/*
 *  log.c
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

#include "config.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
/* pacman */
#include "alpm.h"
#include "log.h"

void _alpm_log(unsigned char flag, char *fmt, ...)
{
	alpm_cb_log logcb = alpm_option_get_logcb();
	if(logcb == NULL) {
		return;
	}

	if(flag & alpm_option_get_logmask()) {
		char str[LOG_STR_LEN];
		va_list args;

		va_start(args, fmt);
		vsnprintf(str, LOG_STR_LEN, fmt, args);
		va_end(args);

		logcb(flag, str);
	}
}

/* vim: set ts=2 sw=2 noet: */
