/*
 *  output.c
 *
 *  Copyright (c) 2002-2007 by Judd Vinet <jvinet@zeroflux.org>
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
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>

#include <alpm.h>

/* pacman */
#include "output.h"
#include "conf.h"
#include "util.h"

#define LOG_STR_LEN 256

extern config_t *config;

/* presents a prompt and gets a Y/N answer */
/* TODO there must be a better way */
int yesno(char *fmt, ...)
{
	char str[LOG_STR_LEN];
	char response[32];
	va_list args;

	if(config->noconfirm) {
		return(1);
	}

	va_start(args, fmt);
	vsnprintf(str, LOG_STR_LEN, fmt, args);
	va_end(args);

	/* Use stderr so questions are always displayed when redirecting output */
	fprintf(stderr, str);

	if(fgets(response, 32, stdin)) {
		if(strlen(response) != 0) {
			strtrim(response);
		}

		if(!strcasecmp(response, _("Y")) || !strcasecmp(response, _("YES")) || strlen(response) == 0) {
			return(1);
		}
	}
	return(0);
}

/* vim: set ts=2 sw=2 noet: */
