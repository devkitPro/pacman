/*
 *  log.c
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

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <alpm.h>

/* pacman */
#include "log.h"
#include "list.h"
#include "conf.h"

#define LOG_STR_LEN 256

extern config_t *config;

static int neednl; /* for cleaner message output */

/* Callback to handle notifications from the library
 */
void cb_log(unsigned short level, char *msg)
{
	char str[9] = "";

	switch(level) {
		case PM_LOG_DEBUG:
			sprintf(str, "DEBUG");
		break;
		case PM_LOG_ERROR:
			sprintf(str, "ERROR");
		break;
		case PM_LOG_WARNING:
			sprintf(str, "WARNING");
		break;
		case PM_LOG_FLOW1:
			sprintf(str, "FLOW1");
		break;
		case PM_LOG_FLOW2:
			sprintf(str, "FLOW2");
		break;
		case PM_LOG_FUNCTION:
			sprintf(str, "FUNCTION");
		break;
		default:
			sprintf(str, "???");
		break;
	}

	if(strlen(str) > 0) {
		MSG(NL, "%s: %s\n", str, msg);
	}
}

/* Wrapper to fprintf() that allows to choose if we want the output
 * to be appended on the current line, or written to a new one
 */
void pm_fprintf(FILE *file, unsigned short line, char *fmt, ...)
{
	va_list args;

	char str[LOG_STR_LEN];

	if(neednl == 1 && line == NL) {
		fprintf(stdout, "\n");
		neednl = 0;
	}

	va_start(args, fmt);
	vsnprintf(str, LOG_STR_LEN, fmt, args);
	va_end(args);

	fprintf(file, str);
	fflush(file);

	neednl = (str[strlen(str)-1] == 10) ? 0 : 1;
}

/* Check verbosity option and, if set, print the
 * string to stdout
 */
void vprint(char *fmt, ...)
{
	va_list args;

	if(config->verbose > 1) {
		if(neednl == 1) {
			fprintf(stdout, "\n");
			neednl = 0;
		}
		va_start(args, fmt);
		/* ORE
		commented for now: it produces corruption
		pm_fprintf(stdout, NL, fmt, args); */
		vprintf(fmt, args);
		va_end(args);
	}
}

/* vim: set ts=2 sw=2 noet: */
