/*
 *  util.c
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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>

#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "util.h"
#include "conf.h"

extern config_t *config;

/* gets the current screen column width */
int getcols()
{
	if(!isatty(1)) {
		/* We will default to 80 columns if we're not a tty
		 * this seems a fairly standard file width.
		 */
		return 80;
	} else {
#ifdef TIOCGSIZE
		struct ttysize win;
		if(ioctl(1, TIOCGSIZE, &win) == 0) {
			return win.ts_cols;
		}
#elif defined(TIOCGWINSZ)
		struct winsize win;
		if(ioctl(1, TIOCGWINSZ, &win) == 0) {
			return win.ws_col;
		}
#endif
		/* If we can't figure anything out, we'll just assume 80 columns */
		/* TODO any problems caused by this assumption? */
		return 80;
	}
	/* Original envvar way - prone to display issues
	const char *cenv = getenv("COLUMNS");
	if(cenv != NULL) {
		return atoi(cenv);
	}
	return -1;
	*/
}

/* does the same thing as 'mkdir -p' */
int makepath(const char *path)
{
	char *orig, *str, *ptr;
	char full[PATH_MAX+1] = "";
	mode_t oldmask;

	oldmask = umask(0000);

	orig = strdup(path);
	str = orig;
	while((ptr = strsep(&str, "/"))) {
		if(strlen(ptr)) {
			struct stat buf;

			/* TODO we should use strncat */
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
int rmrf(const char *path)
{
	int errflag = 0;
	struct dirent *dp;
	DIR *dirp;

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
				char name[PATH_MAX];
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
}

/* output a string, but wrap words properly with a specified indentation
 */
void indentprint(const char *str, int indent)
{
	const char *p = str;
	int cidx = indent;

	while(*p) {
		if(*p == ' ') {
			const char *next = NULL;
			int len;
			p++;
			if(p == NULL || *p == ' ') continue;
			next = strchr(p, ' ');
			if(next == NULL) {
				next = p + mbstowcs(NULL, p, 0);
			}
			len = next - p;
			if(len > (getcols() - cidx - 1)) {
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

	if(str == NULL || *str == '\0') {
		/* string is empty, so we're done. */
		return(str);
	}

	while(isspace(*pch)) {
		pch++;
	}
	if(pch != str) {
		memmove(str, pch, (strlen(pch) + 1));
	}
	
	/* check if there wasn't anything but whitespace in the string. */
	if(*str == '\0') {
		return(str);
	}

	pch = (str + (strlen(str) - 1));
	while(isspace(*pch)) {
		pch--;
	}
	*++pch = '\0';

	return(str);
}

/* Helper function for strreplace */
static void _strnadd(char **str, const char *append, unsigned int count)
{
	if(*str) {
		*str = realloc(*str, strlen(*str) + count + 1);
	} else {
		*str = calloc(sizeof(char), count + 1);
	}

	strncat(*str, append, count);
}

/* Replace all occurances of 'needle' with 'replace' in 'str', returning
 * a new string (must be free'd) */
char *strreplace(const char *str, const char *needle, const char *replace)
{
	const char *p, *q;
	p = q = str;

	char *newstr = NULL;
	unsigned int needlesz = strlen(needle),
							 replacesz = strlen(replace);

	while (1) {
		q = strstr(p, needle);
		if(!q) { /* not found */
			if(*p) {
				/* add the rest of 'p' */
				_strnadd(&newstr, p, strlen(p));
			}
			break;
		} else { /* found match */
			if(q > p){
				/* add chars between this occurance and last occurance, if any */
				_strnadd(&newstr, p, q - p);
			}
			_strnadd(&newstr, replace, replacesz);
			p = q + needlesz;
		}
	}

	return newstr;
}

void list_display(const char *title, const alpm_list_t *list)
{
	const alpm_list_t *i;
	int cols, len;

	len = mbstowcs(NULL, title, 0);
	printf("%s ", title);

	if(list) {
		for(i = list, cols = len; i; i = alpm_list_next(i)) {
			char *str = alpm_list_getdata(i);
			int s = mbstowcs(NULL, str, 0) + 2;
			int maxcols = getcols();
			if(s + cols >= maxcols) {
				int i;
				cols = len;
				printf("\n");
				for (i = 0; i <= len; ++i) {
					printf(" ");
				}
			}
			printf("%s  ", str);
			cols += s;
		}
		printf("\n");
	} else {
		printf(_("None\n"));
	}
}

/* Display a list of transaction targets.
 * `pkgs` should be a list of pmsyncpkg_t's,
 * retrieved from a transaction object
 */
/* TODO move to output.c? or just combine util and output */
void display_targets(const alpm_list_t *syncpkgs)
{
	char *str;
	const alpm_list_t *i, *j;
	alpm_list_t *targets = NULL, *to_remove = NULL;
	/* TODO these are some messy variable names */
	unsigned long size = 0, isize = 0, rsize = 0, dispsize = 0;
	double mbsize = 0.0, mbisize = 0.0, mbrsize = 0.0, mbdispsize = 0.0;

	for(i = syncpkgs; i; i = alpm_list_next(i)) {
		pmsyncpkg_t *sync = alpm_list_getdata(i);
		pmpkg_t *pkg = alpm_sync_get_pkg(sync);

		/* If this sync record is a replacement, the data member contains
		 * a list of packages to be removed due to the package that is being
		 * installed. */
		if(alpm_sync_get_type(sync) == PM_SYNC_TYPE_REPLACE) {
			alpm_list_t *to_replace = alpm_sync_get_data(sync);
			
			for(j = to_replace; j; j = alpm_list_next(j)) {
				pmpkg_t *rp = alpm_list_getdata(j);
				const char *name = alpm_pkg_get_name(rp);

				if(!alpm_list_find_str(to_remove, name)) {
					rsize += alpm_pkg_get_isize(rp);
					to_remove = alpm_list_add(to_remove, strdup(name));
				}
			}
		}

		dispsize = alpm_pkg_get_size(pkg);
		size += dispsize;
		isize += alpm_pkg_get_isize(pkg);

		/* print the package size with the output if ShowSize option set */
		if(config->showsize) {
			/* Convert byte size to MB */
			mbdispsize = dispsize / (1024.0 * 1024.0);

			asprintf(&str, "%s-%s [%.1f MB]", alpm_pkg_get_name(pkg),
					alpm_pkg_get_version(pkg), (mbdispsize < 0.1 ? 0.1 : mbdispsize));
		} else {
			asprintf(&str, "%s-%s", alpm_pkg_get_name(pkg),
					alpm_pkg_get_version(pkg));
		}
		targets = alpm_list_add(targets, str);
	}

	/* Convert byte sizes to MB */
	mbsize = size / (1024.0 * 1024.0);
	mbisize = isize / (1024.0 * 1024.0);
	mbrsize = rsize / (1024.0 * 1024.0);

	/* start displaying information */
	printf("\n");

	if(to_remove) {
		list_display(_("Remove:"), to_remove);
		printf("\n");
		FREELIST(to_remove);
	
		/* round up if size is really small */
		if(mbrsize < 0.1) {
			mbrsize = 0.1;
		}
		printf(_("Total Removed Size:   %.2f MB\n"), mbrsize);
	}

	list_display(_("Targets:"), targets);
	printf("\n");

	/* round up if size is really small */
	if(mbsize < 0.1) {
		mbsize = 0.1;
	}
	printf(_("Total Package Size:   %.2f MB\n"), mbsize);
	
	/* TODO because all pkgs don't include isize, this is a crude hack */
	if(mbisize > mbsize) {
		/*round up if size is really small */
		if(mbisize < 0.1) {
			mbisize = 0.1;
		}
		printf(_("Total Installed Size:   %.2f MB\n"), mbisize);
	}

	FREELIST(targets);
}

/* presents a prompt and gets a Y/N answer */
/* TODO there must be a better way */
int yesno(char *fmt, ...)
{
	char response[32];
	va_list args;

	if(config->noconfirm) {
		return(1);
	}

	va_start(args, fmt);
	/* Use stderr so questions are always displayed when redirecting output */
	vfprintf(stderr, fmt, args);
	va_end(args);

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

int pm_printf(pmloglevel_t level, const char *format, ...)
{
	int ret;
	va_list args;

	/* print the message using va_arg list */
	va_start(args, format);
	ret = pm_vfprintf(stdout, level, format, args);
	va_end(args);

	return(ret);
}

int pm_fprintf(FILE *stream, pmloglevel_t level, const char *format, ...)
{
	int ret;
	va_list args;

	/* print the message using va_arg list */
	va_start(args, format);
	ret = pm_vfprintf(stream, level, format, args);
	va_end(args);

	return(ret);
}

int pm_vfprintf(FILE *stream, pmloglevel_t level, const char *format, va_list args)
{
	int ret = 0;

	/* if current logmask does not overlap with level, do not print msg */
	if(!(config->logmask & level)) {
		return ret;
	}

#if defined(PACMAN_DEBUG)
	/* If debug is on, we'll timestamp the output */
  if(config->logmask & PM_LOG_DEBUG) {
		time_t t;
		struct tm *tmp;
		char timestr[10] = {0};

		t = time(NULL);
		tmp = localtime(&t);
		strftime(timestr, 9, "%H:%M:%S", tmp);
		timestr[8] = '\0';

		printf("[%s] ", timestr);
	}
#endif

	/* print a prefix to the message */
	switch(level) {
		case PM_LOG_DEBUG:
			fprintf(stream, _("debug: "));
			break;
		case PM_LOG_ERROR:
			fprintf(stream, _("error: "));
			break;
		case PM_LOG_WARNING:
			fprintf(stream, _("warning: "));
			break;
		case PM_LOG_FUNCTION:
		  /* TODO we should increase the indent level when this occurs so we can see
			 * program flow easier.  It'll be fun */
			fprintf(stream, _("function: "));
			break;
		default:
			break;
	}

	/* print the message using va_arg list */
	ret = vfprintf(stream, format, args);
	return(ret);
}

/* vim: set ts=2 sw=2 noet: */
