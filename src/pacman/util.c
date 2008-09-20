/*
 *  util.c
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
#include <wchar.h>

#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "util.h"
#include "conf.h"
#include "callback.h"


int trans_init(pmtranstype_t type, pmtransflag_t flags)
{
	if(alpm_trans_init(type, flags, cb_trans_evt,
				cb_trans_conv, cb_trans_progress) == -1) {
		pm_fprintf(stderr, PM_LOG_ERROR, _("failed to init transaction (%s)\n"),
				alpm_strerrorlast());
		if(pm_errno == PM_ERR_HANDLE_LOCK) {
			fprintf(stderr, _("  if you're sure a package manager is not already\n"
						"  running, you can remove %s\n"), alpm_option_get_lockfile());
		}
		return(-1);
	}
	return(0);
}

int trans_release(void)
{
	if(alpm_trans_release() == -1) {
		pm_fprintf(stderr, PM_LOG_ERROR, _("failed to release transaction (%s)\n"),
				alpm_strerrorlast());
		return(-1);
	}
	return(0);
}

int needs_transaction(void)
{
	if(config->op != PM_OP_MAIN && config->op != PM_OP_QUERY && config->op != PM_OP_DEPTEST) {
		if((config->op == PM_OP_SYNC && !config->op_s_sync &&
				(config->op_s_search || config->group || config->op_q_list || config->op_q_info))
			 || config->op == PM_OP_DEPTEST) {
			/* special case: PM_OP_SYNC can be used w/ config->op_s_search by any user */
			return(0);
		} else {
			return(1);
		}
	}
	return(0);
}

/* gets the current screen column width */
int getcols(void)
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
	/* A bit of pointer hell here. Descriptions:
	 * orig - a copy of path so we can safely butcher it with strsep
	 * str - the current position in the path string (after the delimiter)
	 * ptr - the original position of str after calling strsep
	 * incr - incrementally generated path for use in access/mkdir call
	 */
	char *orig, *str, *ptr, *incr;
	mode_t oldmask = umask(0000);
	int ret = 0;

	orig = strdup(path);
	incr = calloc(strlen(orig) + 1, sizeof(char));
	str = orig;
	while((ptr = strsep(&str, "/"))) {
		if(strlen(ptr)) {
			/* we have another path component- append the newest component to
			 * existing string and create one more level of dir structure */
			strcat(incr, "/");
			strcat(incr, ptr);
			if(access(incr, F_OK)) {
				if(mkdir(incr, 0755)) {
					ret = 1;
					break;
				}
			}
		}
	}
	free(orig);
	free(incr);
	umask(oldmask);
	return(ret);
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

/** Parse the basename of a program from a path.
* Grabbed from the uClibc source.
* @param path path to parse basename from
*
* @return everything following the final '/'
*/
char *mbasename(const char *path)
{
	const char *s;
	const char *p;

	p = s = path;

	while (*s) {
		if (*s++ == '/') {
			p = s;
		}
	}

	return (char *)p;
}

/** Parse the dirname of a program from a path.
* The path returned should be freed.
* @param path path to parse dirname from
*
* @return everything preceding the final '/'
*/
char *mdirname(const char *path)
{
	char *ret, *last;

	/* null or empty path */
	if(path == NULL || path == '\0') {
		return(strdup("."));
	}

	ret = strdup(path);
	last = strrchr(ret, '/');

	if(last != NULL) {
		/* we found a '/', so terminate our string */
		*last = '\0';
		return(ret);
	}
	/* no slash found */
	free(ret);
	return(strdup("."));
}

/* output a string, but wrap words properly with a specified indentation
 */
void indentprint(const char *str, int indent)
{
	wchar_t *wcstr;
	const wchar_t *p;
	int len, cidx, cols;

	if(!str) {
		return;
	}

	len = strlen(str) + 1;
	wcstr = calloc(len, sizeof(wchar_t));
	len = mbstowcs(wcstr, str, len);
	p = wcstr;
	cidx = indent;

	if(!p) {
		return;
	}
	cols = getcols();

	while(*p) {
		if(*p == L' ') {
			const wchar_t *q, *next;
			p++;
			if(p == NULL || *p == L' ') continue;
			next = wcschr(p, L' ');
			if(next == NULL) {
				next = p + wcslen(p);
			}
			/* len captures # cols */
			len = 0;
			q = p;
			while(q < next) {
				len += wcwidth(*q++);
			}
			if(len > (cols - cidx - 1)) {
				/* wrap to a newline and reindent */
				fprintf(stdout, "\n%-*s", indent, "");
				cidx = indent;
			} else {
				printf(" ");
				cidx++;
			}
			continue;
		}
		fprintf(stdout, "%lc", (wint_t)*p);
		cidx += wcwidth(*p);
		p++;
	}
	free(wcstr);
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

/** Splits a string into a list of strings using the chosen character as
 * a delimiter.
 *
 * @param str the string to split
 * @param splitchar the character to split at
 *
 * @return a list containing the duplicated strings
 */
alpm_list_t *strsplit(const char *str, const char splitchar)
{
	alpm_list_t *list = NULL;
	const char *prev = str;
	char *dup = NULL;

	while((str = strchr(str, splitchar))) {
		dup = strndup(prev, str - prev);
		if(dup == NULL) {
			return(NULL);
		}
		list = alpm_list_add(list, dup);

		str++;
		prev = str;
	}

	dup = strdup(prev);
	if(dup == NULL) {
		return(NULL);
	}
	list = alpm_list_add(list, dup);

	return(list);
}

static int string_length(const char *s)
{
	int len;
	wchar_t *wcstr;

	if(!s) {
		return(0);
	}
	/* len goes from # bytes -> # chars -> # cols */
	len = strlen(s) + 1;
	wcstr = calloc(len, sizeof(wchar_t));
	len = mbstowcs(wcstr, s, len);
	len = wcswidth(wcstr, len);
	free(wcstr);

	return(len);
}

void string_display(const char *title, const char *string)
{
	int len = 0;

	if(title) {
		/* compute the length of title + a space */
		len = string_length(title) + 1;
		printf("%s ", title);
	}
	if(string == NULL || string[0] == '\0') {
		printf(_("None"));
	} else {
		indentprint(string, len);
	}
	printf("\n");
}

void list_display(const char *title, const alpm_list_t *list)
{
	const alpm_list_t *i;
	int cols, len = 0;

	if(title) {
		len = string_length(title) + 1;
		printf("%s ", title);
	}

	if(!list) {
		printf("%s\n", _("None"));
	} else {
		for(i = list, cols = len; i; i = alpm_list_next(i)) {
			char *str = alpm_list_getdata(i);
			int s = string_length(str);
			/* two additional spaces are added to the length */
			s += 2;
			int maxcols = getcols();
			if(s + cols > maxcols) {
				int j;
				cols = len;
				printf("\n");
				for (j = 1; j <= len; j++) {
					printf(" ");
				}
			}
			printf("%s  ", str);
			cols += s;
		}
		printf("\n");
	}
}

void list_display_linebreak(const char *title, const alpm_list_t *list)
{
	const alpm_list_t *i;
	int len = 0;

	if(title) {
		len = string_length(title) + 1;
		printf("%s ", title);
	}

	if(!list) {
		printf("%s\n", _("None"));
	} else {
		/* Print the first element */
		indentprint((const char *) alpm_list_getdata(list), len);
		printf("\n");
		/* Print the rest */
		for(i = alpm_list_next(list); i; i = alpm_list_next(i)) {
			int j;
			for(j = 1; j <= len; j++) {
				printf(" ");
			}
			indentprint((const char *) alpm_list_getdata(i), len);
			printf("\n");
		}
	}
}
/* prepare a list of pkgs to display */
void display_targets(const alpm_list_t *pkgs, int install)
{
	char *str;
	const alpm_list_t *i;
	off_t isize = 0, dlsize = 0;
	double mbisize = 0.0, mbdlsize = 0.0;
	alpm_list_t *targets = NULL;

	if(!pkgs) {
		return;
	}

	printf("\n");
	for(i = pkgs; i; i = alpm_list_next(i)) {
		pmpkg_t *pkg = alpm_list_getdata(i);

		dlsize += alpm_pkg_download_size(pkg);
		isize += alpm_pkg_get_isize(pkg);

		/* print the package size with the output if ShowSize option set */
		if(config->showsize) {
			double mbsize = 0.0;
			mbsize = alpm_pkg_get_size(pkg) / (1024.0 * 1024.0);

			asprintf(&str, "%s-%s [%.2f MB]", alpm_pkg_get_name(pkg),
					alpm_pkg_get_version(pkg), mbsize);
		} else {
			asprintf(&str, "%s-%s", alpm_pkg_get_name(pkg),
					alpm_pkg_get_version(pkg));
		}
		targets = alpm_list_add(targets, str);
	}

	/* Convert byte sizes to MB */
	mbdlsize = dlsize / (1024.0 * 1024.0);
	mbisize = isize / (1024.0 * 1024.0);

	if(install) {
		asprintf(&str, _("Targets (%d):"), alpm_list_count(targets));
		list_display(str, targets);
		free(str);
		printf("\n");

		printf(_("Total Download Size:    %.2f MB\n"), mbdlsize);
		printf(_("Total Installed Size:   %.2f MB\n"), mbisize);
	} else {
		asprintf(&str, _("Remove (%d):"), alpm_list_count(targets));
		list_display(str, targets);
		free(str);
		printf("\n");

		printf(_("Total Removed Size:   %.2f MB\n"), mbisize);
	}

	FREELIST(targets);
}

/* Display a list of transaction targets.
 * `pkgs` should be a list of pmsyncpkg_t's,
 * retrieved from a transaction object
 */
void display_synctargets(const alpm_list_t *syncpkgs)
{
	const alpm_list_t *i, *j;
	alpm_list_t *pkglist = NULL, *rpkglist = NULL;

	for(i = syncpkgs; i; i = alpm_list_next(i)) {
		pmsyncpkg_t *sync = alpm_list_getdata(i);
		pmpkg_t *pkg = alpm_sync_get_pkg(sync);
		pkglist = alpm_list_add(pkglist, pkg);

		/* The removes member contains a list of packages to be removed
		 * due to the package that is being installed. */
		alpm_list_t *to_replace = alpm_sync_get_removes(sync);

		for(j = to_replace; j; j = alpm_list_next(j)) {
			pmpkg_t *rp = alpm_list_getdata(j);
			rpkglist = alpm_list_add(rpkglist, rp);
		}
	}

	/* start displaying information */
	display_targets(rpkglist, 0);
	display_targets(pkglist, 1);

	alpm_list_free(pkglist);
	alpm_list_free(rpkglist);
}

void display_optdepends(pmpkg_t *pkg)
{
	alpm_list_t *optdeps = alpm_pkg_get_optdepends(pkg);
	if(optdeps) {
		printf(_("Optional dependencies for %s\n"), alpm_pkg_get_name(pkg));
		list_display_linebreak("   ", optdeps);
	}
}

/* presents a prompt and gets a Y/N answer */
int yesno(short preset, char *fmt, ...)
{
	char response[32];
	va_list args;
	FILE *stream;

	if(config->noconfirm) {
		stream = stdout;
	} else {
		/* Use stderr so questions are always displayed when redirecting output */
		stream = stderr;
	}

	va_start(args, fmt);
	vfprintf(stream, fmt, args);
	va_end(args);

	if(preset) {
		fprintf(stream, " %s ", _("[Y/n]"));
	} else {
		fprintf(stream, " %s ", _("[y/N]"));
	}

	if(config->noconfirm) {
		fprintf(stream, "\n");
		return(preset);
	}

	if(fgets(response, 32, stdin)) {
		strtrim(response);
		if(strlen(response) == 0) {
			return(preset);
		}

		if(!strcasecmp(response, _("Y")) || !strcasecmp(response, _("YES"))) {
			return(1);
		} else if (!strcasecmp(response, _("N")) || !strcasecmp(response, _("NO"))) {
			return(0);
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

int pm_vasprintf(char **string, pmloglevel_t level, const char *format, va_list args)
{
	int ret = 0;
	char *msg = NULL;

	/* if current logmask does not overlap with level, do not print msg */
	if(!(config->logmask & level)) {
		return ret;
	}

	/* print the message using va_arg list */
	ret = vasprintf(&msg, format, args);

	/* print a prefix to the message */
	switch(level) {
		case PM_LOG_DEBUG:
			asprintf(string, "debug: %s", msg);
			break;
		case PM_LOG_ERROR:
			asprintf(string, _("error: %s"), msg);
			break;
		case PM_LOG_WARNING:
			asprintf(string, _("warning: %s"), msg);
			break;
		case PM_LOG_FUNCTION:
			asprintf(string, _("function: %s"), msg);
			break;
		default:
			break;
	}
	free(msg);

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
			fprintf(stream, "debug: ");
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

#ifndef HAVE_STRNDUP
/* A quick and dirty implementation derived from glibc */
static size_t strnlen(const char *s, size_t max)
{
    register const char *p;
    for(p = s; *p && max--; ++p);
    return(p - s);
}

char *strndup(const char *s, size_t n)
{
  size_t len = strnlen(s, n);
  char *new = (char *) malloc(len + 1);

  if (new == NULL)
    return NULL;

  new[len] = '\0';
  return (char *) memcpy(new, s, len);
}
#endif

/* vim: set ts=2 sw=2 noet: */
