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
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <libintl.h>

#include <alpm.h>
/* pacman */
#include "log.h"
#include "conf.h"
#include "util.h"

#define LOG_STR_LEN 256

extern config_t *config;

static int neednl = 0; /* for cleaner message output */
static int needpad = 0; /* pad blanks to terminal width */

/* simple helper for needpad */
void set_output_padding(int on)
{
	needpad = on;
}

/* Callback to handle notifications from the library
 */
void cb_log(unsigned short level, char *msg)
{
	char str[LOG_STR_LEN] = "";

	if(!strlen(msg)) {
		return;
	}

	switch(level) {
		case PM_LOG_DEBUG:
			sprintf(str, _("debug"));
		break;
		case PM_LOG_ERROR:
			sprintf(str, _("error"));
		break;
		case PM_LOG_WARNING:
			sprintf(str, _("warning"));
		break;
		case PM_LOG_FUNCTION:
		  /* TODO we should increase the indent level when this occurs so we can see
			 * program flow easier.  It'll be fun
			 */
			sprintf(str, _("function"));
		break;
		default:
			sprintf(str, "???");
		break;
	}

#ifdef PACMAN_DEBUG
	/* If debug is on, we'll timestamp the output */
  if(alpm_option_get_logmask() & PM_LOG_DEBUG) {
		time_t t;
		struct tm *tmp;
		char timestr[10] = {0};

		t = time(NULL);
		tmp = localtime(&t);
		strftime(timestr, 9, "%H:%M:%S", tmp);
		timestr[8] = '\0';

		MSG(NL, "[%s] %s: %s", timestr, str, msg);
	} else {
    MSG(NL, "%s: %s", str, msg);
	}
#else
	MSG(NL, "%s: %s", str, msg);
#endif
}

/* Wrapper to fprintf() that allows to choose if we want the output
 * to be appended on the current line, or written to a new one
 */
void pm_fprintf(FILE *file, unsigned short line, char *fmt, ...)
{
	va_list args;

	char str[LOG_STR_LEN];
	int len = 0;

	if(neednl == 1 && line == NL) {
		fprintf(file, "\n");
		neednl = 0;
	}

	va_start(args, fmt);
	vsnprintf(str, LOG_STR_LEN, fmt, args);
	va_end(args);

	len = strlen(str);

  if(needpad == 1 && str[len-1] == '\n') {
		/* we want this removed so we can pad */
		str[len-1] = ' ';
		neednl = 1;
	}

	fprintf(file, str);

	if(needpad == 1) {
		unsigned int i, cols = getcols();
		for(i=len; i < cols; ++i) {
			fprintf(file, " ");
		}
		if(neednl == 1 && line == NL) {
			fprintf(file, "\n");
			neednl = 0;
		}
	}
	fflush(file);
	neednl = (str[strlen(str)-1] == '\n') ? 0 : 1;
}

/* Check verbosity option and, if set, print the
 * string to stdout
 */
void vprint(char *fmt, ...)
{
	va_list args;

	char str[LOG_STR_LEN];

	if(config->verbose > 0) {
		va_start(args, fmt);
		vsnprintf(str, LOG_STR_LEN, fmt, args);
		va_end(args);
		pm_fprintf(stdout, NL, str);
	}
}

/* presents a prompt and gets a Y/N answer
 */
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
	pm_fprintf(stderr, NL, str); \

	if(fgets(response, 32, stdin)) {
		/* trim whitespace and newlines */
		char *pch = response;
		while(isspace(*pch)) {
			pch++;
		}
		if(pch != response) {
			memmove(response, pch, strlen(pch) + 1);
		}
		pch = response + strlen(response) - 1;
		while(isspace(*pch)) {
			pch--;
		}
		*++pch = 0;
		if(strlen(response) != 0) {
			strtrim(response);
		}

		/* User hits 'enter', forcing a newline here */
		neednl = 0;

		if(!strcasecmp(response, _("Y")) || !strcasecmp(response, _("YES")) || strlen(response) == 0) {
			return(1);
		}
	}
	return(0);
}

/* vim: set ts=2 sw=2 noet: */
