/*
 *  util.c
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
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <regex.h>
#ifdef CYGWIN
#include <limits.h> /* PATH_MAX */
#endif

/* pacman */
#include "util.h"

extern int maxcols;

/* does the same thing as 'mkdir -p' */
int makepath(char *path)
{
	char *orig, *str, *ptr;
	char full[PATH_MAX] = "";
	mode_t oldmask;

	oldmask = umask(0000);

	orig = strdup(path);
	str = orig;
	while((ptr = strsep(&str, "/"))) {
		if(strlen(ptr)) {
			struct stat buf;

			strcat(full, "/");
			strcat(full, ptr);
			if(stat(full, &buf)) {
				if(mkdir(full, 0755)) {
					free(orig);
					umask(oldmask);
					return(1);
				}
			}
		}
	}
	free(orig);
	umask(oldmask);
	return(0);
}

/* does the same thing as 'rm -rf' */
int rmrf(char *path)
{
	int errflag = 0;
	struct dirent *dp;
	DIR *dirp;
	char name[PATH_MAX];
	extern int errno;

	if(!unlink(path)) {
		return(0);
	} else {
		if(errno == ENOENT) {
			return(0);
		} else if(errno == EPERM) {
			/* fallthrough */
		} else if(errno == EISDIR) {
			/* fallthrough */
		} else if(errno == ENOTDIR) {
			return(1);
		} else {
			/* not a directory */
			return(1);
		}

		if((dirp = opendir(path)) == (DIR *)-1) {
			return(1);
		}
		for(dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
			if(dp->d_ino) {
				sprintf(name, "%s/%s", path, dp->d_name);
				if(strcmp(dp->d_name, "..") && strcmp(dp->d_name, ".")) {
					errflag += rmrf(name);
				}
			}
		}
		closedir(dirp);
		if(rmdir(path)) {
			errflag++;
		}
		return(errflag);
	}
	return(0);
}

/* output a string, but wrap words properly with a specified indentation
 */
void indentprint(char *str, int indent)
{
	char *p = str;
	int cidx = indent;

	while(*p) {
		if(*p == ' ') {
			char *next = NULL;
			int len;
			p++;
			if(p == NULL || *p == ' ') continue;
			next = strchr(p, ' ');
			if(next == NULL) {
				next = p + strlen(p);
			}
			len = next - p;
			if(len > (maxcols-cidx-1)) {
				/* newline */
				int i;
				fprintf(stdout, "\n");
				for(i = 0; i < indent; i++) {
					fprintf(stdout, " ");
				}
				cidx = indent;
			} else {
				printf(" ");
				cidx++;
			}
		}
		fprintf(stdout, "%c", *p);
		p++;
		cidx++;
	}
}

/* Convert a string to uppercase
 */
char *strtoupper(char *str)
{
	char *ptr = str;

	while(*ptr) {
		(*ptr) = toupper(*ptr);
		ptr++;
	}
	return str;
}

/* Trim whitespace and newlines from a string
 */
char *strtrim(char *str)
{
	char *pch = str;
	while(isspace(*pch)) {
		pch++;
	}
	if(pch != str) {
		memmove(str, pch, (strlen(pch) + 1));
	}
	
	pch = (char *)(str + (strlen(str) - 1));
	while(isspace(*pch)) {
		pch--;
	}
	*++pch = '\0';

	return str;
}

/* presents a prompt and gets a Y/N answer
 */
int yesno(char *fmt, ...)
{
	char response[32];
	va_list args;

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	fflush(stdout);
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
		strtrim(response);

		if(!strcasecmp(response, "Y") || !strcasecmp(response, "YES") || !strlen(response)) {
			return(1);
		}
	}
	return(0);
}

/* match a string against a regular expression */
int reg_match(char *string, char *pattern)
{
	int result;
	regex_t reg;

	if(regcomp(&reg, pattern, REG_EXTENDED | REG_NOSUB | REG_ICASE) != 0) {
		fprintf(stderr, "error: %s is not a valid regular expression.\n", pattern);
		return(-1);
	}
	result = regexec(&reg, string, 0, 0, 0);
	regfree(&reg);
	return(!(result));
}

/* vim: set ts=2 sw=2 noet: */
