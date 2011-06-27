/*
 *  util.c
 *
 *  Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h> /* intmax_t */
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <wchar.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h> /* tcflush */
#endif

#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "util.h"
#include "conf.h"
#include "callback.h"


int trans_init(pmtransflag_t flags)
{
	int ret;
	if(config->print) {
		ret = alpm_trans_init(config->handle, flags, NULL, NULL, NULL);
	} else {
		ret = alpm_trans_init(config->handle, flags, cb_trans_evt, cb_trans_conv,
				cb_trans_progress);
	}

	if(ret == -1) {
		enum _pmerrno_t err = alpm_errno(config->handle);
		pm_fprintf(stderr, PM_LOG_ERROR, _("failed to init transaction (%s)\n"),
				alpm_strerror(err));
		if(err == PM_ERR_HANDLE_LOCK) {
			fprintf(stderr, _("  if you're sure a package manager is not already\n"
						"  running, you can remove %s\n"),
					alpm_option_get_lockfile(config->handle));
		}

		return -1;
	}
	return 0;
}

int trans_release(void)
{
	if(alpm_trans_release(config->handle) == -1) {
		pm_fprintf(stderr, PM_LOG_ERROR, _("failed to release transaction (%s)\n"),
				alpm_strerror(alpm_errno(config->handle)));
		return -1;
	}
	return 0;
}

int needs_root(void)
{
	switch(config->op) {
		case PM_OP_DATABASE:
			return 1;
		case PM_OP_UPGRADE:
		case PM_OP_REMOVE:
			return !config->print;
		case PM_OP_SYNC:
			return (config->op_s_clean || config->op_s_sync ||
					(!config->group && !config->op_s_info && !config->op_q_list &&
					 !config->op_s_search && !config->print));
		default:
			return 0;
	}
}

/* discard unhandled input on the terminal's input buffer */
static int flush_term_input(void) {
#ifdef HAVE_TCFLUSH
	if(isatty(fileno(stdin))) {
		return tcflush(fileno(stdin), TCIFLUSH);
	}
#endif

	/* fail silently */
	return 0;
}

/* gets the current screen column width */
int getcols()
{
	int termwidth = -1;
	const int default_tty = 80;
	const int default_notty = 0;

	if(!isatty(fileno(stdout))) {
		return default_notty;
	}

#ifdef TIOCGSIZE
	struct ttysize win;
	if(ioctl(1, TIOCGSIZE, &win) == 0) {
		termwidth = win.ts_cols;
	}
#elif defined(TIOCGWINSZ)
	struct winsize win;
	if(ioctl(1, TIOCGWINSZ, &win) == 0) {
		termwidth = win.ws_col;
	}
#endif
	return termwidth <= 0 ? default_tty : termwidth;
}

/* does the same thing as 'rm -rf' */
int rmrf(const char *path)
{
	int errflag = 0;
	struct dirent *dp;
	DIR *dirp;

	if(!unlink(path)) {
		return 0;
	} else {
		if(errno == ENOENT) {
			return 0;
		} else if(errno == EPERM) {
			/* fallthrough */
		} else if(errno == EISDIR) {
			/* fallthrough */
		} else if(errno == ENOTDIR) {
			return 1;
		} else {
			/* not a directory */
			return 1;
		}

		dirp = opendir(path);
		if(!dirp) {
			return 1;
		}
		for(dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
			if(dp->d_ino) {
				char name[PATH_MAX];
				sprintf(name, "%s/%s", path, dp->d_name);
				if(strcmp(dp->d_name, "..") != 0 && strcmp(dp->d_name, ".") != 0) {
					errflag += rmrf(name);
				}
			}
		}
		closedir(dirp);
		if(rmdir(path)) {
			errflag++;
		}
		return errflag;
	}
}

/** Parse the basename of a program from a path.
* @param path path to parse basename from
*
* @return everything following the final '/'
*/
const char *mbasename(const char *path)
{
	const char *last = strrchr(path, '/');
	if(last) {
		return last + 1;
	}
	return path;
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
		return strdup(".");
	}

	ret = strdup(path);
	last = strrchr(ret, '/');

	if(last != NULL) {
		/* we found a '/', so terminate our string */
		*last = '\0';
		return ret;
	}
	/* no slash found */
	free(ret);
	return strdup(".");
}

/* output a string, but wrap words properly with a specified indentation
 */
void indentprint(const char *str, int indent)
{
	wchar_t *wcstr;
	const wchar_t *p;
	int len, cidx;
	const int cols = getcols();

	if(!str) {
		return;
	}

	/* if we're not a tty, or our tty is not wide enough that wrapping even makes
	 * sense, print without indenting */
	if(cols == 0 || indent > cols) {
		printf("%s", str);
		return;
	}

	len = strlen(str) + 1;
	wcstr = calloc(len, sizeof(wchar_t));
	len = mbstowcs(wcstr, str, len);
	p = wcstr;
	cidx = indent;

	if(!p || !len) {
		return;
	}

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
				printf("\n%-*s", indent, "");
				cidx = indent;
			} else {
				printf(" ");
				cidx++;
			}
			continue;
		}
		printf("%lc", (wint_t)*p);
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
		(*ptr) = (char)toupper((unsigned char)*ptr);
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
		return str;
	}

	while(isspace((unsigned char)*pch)) {
		pch++;
	}
	if(pch != str) {
		memmove(str, pch, (strlen(pch) + 1));
	}

	/* check if there wasn't anything but whitespace in the string. */
	if(*str == '\0') {
		return str;
	}

	pch = (str + (strlen(str) - 1));
	while(isspace((unsigned char)*pch)) {
		pch--;
	}
	*++pch = '\0';

	return str;
}

/* Replace all occurances of 'needle' with 'replace' in 'str', returning
 * a new string (must be free'd) */
char *strreplace(const char *str, const char *needle, const char *replace)
{
	const char *p = NULL, *q = NULL;
	char *newstr = NULL, *newp = NULL;
	alpm_list_t *i = NULL, *list = NULL;
	size_t needlesz = strlen(needle), replacesz = strlen(replace);
	size_t newsz;

	if(!str) {
		return NULL;
	}

	p = str;
	q = strstr(p, needle);
	while(q) {
		list = alpm_list_add(list, (char *)q);
		p = q + needlesz;
		q = strstr(p, needle);
	}

	/* no occurences of needle found */
	if(!list) {
		return strdup(str);
	}
	/* size of new string = size of old string + "number of occurences of needle"
	 * x "size difference between replace and needle" */
	newsz = strlen(str) + 1 +
		alpm_list_count(list) * (replacesz - needlesz);
	newstr = malloc(newsz);
	if(!newstr) {
		return NULL;
	}
	*newstr = '\0';

	p = str;
	newp = newstr;
	for(i = list; i; i = alpm_list_next(i)) {
		q = alpm_list_getdata(i);
		if(q > p){
			/* add chars between this occurence and last occurence, if any */
			strncpy(newp, p, (size_t)(q - p));
			newp += q - p;
		}
		strncpy(newp, replace, replacesz);
		newp += replacesz;
		p = q + needlesz;
	}
	alpm_list_free(list);

	if(*p) {
		/* add the rest of 'p' */
		strcpy(newp, p);
		newp += strlen(p);
	}
	*newp = '\0';

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
		dup = strndup(prev, (size_t)(str - prev));
		if(dup == NULL) {
			return NULL;
		}
		list = alpm_list_add(list, dup);

		str++;
		prev = str;
	}

	dup = strdup(prev);
	if(dup == NULL) {
		return NULL;
	}
	list = alpm_list_add(list, dup);

	return list;
}

static int string_length(const char *s)
{
	int len;
	wchar_t *wcstr;

	if(!s) {
		return 0;
	}
	/* len goes from # bytes -> # chars -> # cols */
	len = strlen(s) + 1;
	wcstr = calloc(len, sizeof(wchar_t));
	len = mbstowcs(wcstr, s, len);
	len = wcswidth(wcstr, len);
	free(wcstr);

	return len;
}

void string_display(const char *title, const char *string)
{
	if(title) {
		printf("%s ", title);
	}
	if(string == NULL || string[0] == '\0') {
		printf(_("None"));
	} else {
		/* compute the length of title + a space */
		int len = string_length(title) + 1;
		indentprint(string, len);
	}
	printf("\n");
}

static void table_print_line(const alpm_list_t *line,
		const alpm_list_t *formats)
{
	const alpm_list_t *curformat = formats;
	const alpm_list_t *curcell = line;

	while(curcell && curformat) {
		printf(alpm_list_getdata(curformat), alpm_list_getdata(curcell));
		curcell = alpm_list_next(curcell);
		curformat = alpm_list_next(curformat);
	}

	printf("\n");
}

/* creates format strings by checking max cell lengths in cols */
static alpm_list_t *table_create_format(const alpm_list_t *header,
		const alpm_list_t *rows)
{
	alpm_list_t *longest_str, *longest_strs = NULL;
	alpm_list_t *formats = NULL;
	const alpm_list_t *i, *row, *cell;
	char *str, *formatstr;
	const int padding = 2;
	int colwidth, totalwidth = 0;
	int curcol = 0;

	/* header determines column count and initial values of longest_strs */
	for(i = header; i; i = alpm_list_next(i)) {
		longest_strs = alpm_list_add(longest_strs, alpm_list_getdata(i));
	}

	/* now find the longest string in each column */
	for(longest_str = longest_strs; longest_str;
			longest_str = alpm_list_next(longest_str), curcol++) {
		for(i = rows; i; i = alpm_list_next(i)) {
			row = alpm_list_getdata(i);
			cell = alpm_list_nth(row, curcol);
			str = alpm_list_getdata(cell);

			if(strlen(str) > strlen(alpm_list_getdata(longest_str))) {
				longest_str->data = str;
			}
		}
	}

	/* now use the column width info to generate format strings */
	for(i = longest_strs; i; i = alpm_list_next(i)) {
		const char *display;
		colwidth = strlen(alpm_list_getdata(i)) + padding;
		totalwidth += colwidth;

		/* right align the last column for a cleaner table display */
		display = (alpm_list_next(i) != NULL) ? "%%-%ds" : "%%%ds";
		pm_asprintf(&formatstr, display, colwidth);

		formats = alpm_list_add(formats, formatstr);
	}

	alpm_list_free(longest_strs);

	/* return NULL if terminal is not wide enough */
	if(totalwidth > getcols()) {
		fprintf(stderr, _("insufficient columns available for table display\n"));
		FREELIST(formats);
		return NULL;
	}

	return formats;
}

/** Displays the list in table format
 *
 * @param title the tables title
 * @param header the column headers. column count is determined by the nr
 *               of headers
 * @param rows the rows to display as a list of lists of strings. the outer
 *             list represents the rows, the inner list the cells (= columns)
 *
 * @return -1 if not enough terminal cols available, else 0
 */
int table_display(const char *title, const alpm_list_t *header,
		const alpm_list_t *rows)
{
	const alpm_list_t *i;
	alpm_list_t *formats;

	if(rows == NULL || header == NULL) {
		return 0;
	}

	formats = table_create_format(header, rows);
	if(formats == NULL) {
		return -1;
	}

	if(title != NULL) {
		printf("%s\n\n", title);
	}

	table_print_line(header, formats);
	printf("\n");

	for(i = rows; i; i = alpm_list_next(i)) {
		table_print_line(alpm_list_getdata(i), formats);
	}

	FREELIST(formats);
	return 0;
}

void list_display(const char *title, const alpm_list_t *list)
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
		const int maxcols = getcols();
		int cols = len;
		const char *str = alpm_list_getdata(list);
		printf("%s", str);
		cols += string_length(str);
		for(i = alpm_list_next(list); i; i = alpm_list_next(i)) {
			str = alpm_list_getdata(i);
			int s = string_length(str);
			/* wrap only if we have enough usable column space */
			if(maxcols > len && cols + s + 2 >= maxcols) {
				int j;
				cols = len;
				printf("\n");
				for (j = 1; j <= len; j++) {
					printf(" ");
				}
			} else if(cols != len) {
				/* 2 spaces are added if this is not the first element on a line. */
				printf("  ");
				cols += 2;
			}
			printf("%s", str);
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

/* creates a header row for use with table_display */
static alpm_list_t *create_verbose_header(int install)
{
	alpm_list_t *res = NULL;
	char *str;

	pm_asprintf(&str, "%s", _("Name"));
	res = alpm_list_add(res, str);
	pm_asprintf(&str, "%s", _("Old Version"));
	res = alpm_list_add(res, str);
	if(install) {
		pm_asprintf(&str, "%s", _("New Version"));
		res = alpm_list_add(res, str);
	}
	pm_asprintf(&str, "%s", _("Size"));
	res = alpm_list_add(res, str);

	return res;
}

/* returns package info as list of strings */
static alpm_list_t *create_verbose_row(pmpkg_t *pkg, int install)
{
	char *str;
	double size;
	const char *label;
	alpm_list_t *ret = NULL;
	pmdb_t *ldb = alpm_option_get_localdb(config->handle);

	/* a row consists of the package name, */
	pm_asprintf(&str, "%s", alpm_pkg_get_name(pkg));
	ret = alpm_list_add(ret, str);

	/* old and new versions */
	if(install) {
		pmpkg_t *oldpkg = alpm_db_get_pkg(ldb, alpm_pkg_get_name(pkg));
		pm_asprintf(&str, "%s",
				oldpkg != NULL ? alpm_pkg_get_version(oldpkg) : "");
		ret = alpm_list_add(ret, str);
	}

	pm_asprintf(&str, "%s", alpm_pkg_get_version(pkg));
	ret = alpm_list_add(ret, str);

	/* and size */
	size = humanize_size(alpm_pkg_get_size(pkg), 'M', 1, &label);
	pm_asprintf(&str, "%.2f %s", size, label);
	ret = alpm_list_add(ret, str);

	return ret;
}

/* prepare a list of pkgs to display */
void display_targets(const alpm_list_t *pkgs, int install)
{
	char *str;
	const char *title, *label;
	double size;
	const alpm_list_t *i;
	off_t isize = 0, rsize = 0, dlsize = 0;
	alpm_list_t *j, *lp, *header = NULL, *targets = NULL;
	pmdb_t *db_local = alpm_option_get_localdb(config->handle);

	if(!pkgs) {
		return;
	}

	/* gather pkg infos */
	for(i = pkgs; i; i = alpm_list_next(i)) {
		pmpkg_t *pkg = alpm_list_getdata(i);

		if(install) {
			pmpkg_t *lpkg = alpm_db_get_pkg(db_local, alpm_pkg_get_name(pkg));
			dlsize += alpm_pkg_download_size(pkg);
			if(lpkg) {
				/* add up size of all removed packages */
				rsize += alpm_pkg_get_isize(lpkg);
			}
		}
		isize += alpm_pkg_get_isize(pkg);

		if(config->verbosepkglists) {
			targets = alpm_list_add(targets, create_verbose_row(pkg, install));
		} else {
			pm_asprintf(&str, "%s-%s", alpm_pkg_get_name(pkg),
					alpm_pkg_get_version(pkg));
			targets = alpm_list_add(targets, str);
		}
	}

	/* print to screen */
	title = install ? _("Targets (%d):") : _("Remove (%d):");
	pm_asprintf(&str, title, alpm_list_count(pkgs));

	printf("\n");
	if(config->verbosepkglists) {
		header = create_verbose_header(install);
		if(table_display(str, header, targets) != 0) {
			config->verbosepkglists = 0;
			display_targets(pkgs, install);
			goto out;
		}
	} else {
		list_display(str, targets);
	}
	printf("\n");

	if(install) {
		size = humanize_size(dlsize, 'M', 1, &label);
		printf(_("Total Download Size:    %.2f %s\n"), size, label);
		if(!(config->flags & PM_TRANS_FLAG_DOWNLOADONLY)) {
			size = humanize_size(isize, 'M', 1, &label);
			printf(_("Total Installed Size:   %.2f %s\n"), size, label);
			/* only show this net value if different from raw installed size */
			if(rsize > 0) {
				size = humanize_size(isize - rsize, 'M', 1, &label);
				printf(_("Net Upgrade Size:       %.2f %s\n"), size, label);
			}
		}
	} else {
		size = humanize_size(isize, 'M', 1, &label);
		printf(_("Total Removed Size:   %.2f %s\n"), size, label);
	}

out:
	/* cleanup */
	if(config->verbosepkglists) {
		/* targets is a list of lists of strings, free inner lists here */
		for(j = alpm_list_first(targets); j; j = alpm_list_next(j)) {
			lp = alpm_list_getdata(j);
			FREELIST(lp);
		}
		alpm_list_free(targets);
		FREELIST(header);
	} else {
		FREELIST(targets);
	}
	free(str);
}

static off_t pkg_get_size(pmpkg_t *pkg)
{
	switch(config->op) {
		case PM_OP_SYNC:
			return alpm_pkg_download_size(pkg);
		case PM_OP_UPGRADE:
			return alpm_pkg_get_size(pkg);
		default:
			return alpm_pkg_get_isize(pkg);
	}
}

static char *pkg_get_location(pmpkg_t *pkg)
{
	alpm_list_t *servers;
	char *string = NULL;
	switch(config->op) {
		case PM_OP_SYNC:
			servers = alpm_db_get_servers(alpm_pkg_get_db(pkg));
			if(servers) {
				pm_asprintf(&string, "%s/%s", alpm_list_getdata(servers),
						alpm_pkg_get_filename(pkg));
				return string;
			}
		case PM_OP_UPGRADE:
			return strdup(alpm_pkg_get_filename(pkg));
		default:
			pm_asprintf(&string, "%s-%s", alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
			return string;
	}
}

/** Converts sizes in bytes into human readable units.
 *
 * @param bytes the size in bytes
 * @param target_unit '\0' or a short label. If equal to one of the short unit
 * labels ('B', 'K', ...) bytes is converted to target_unit; if '\0', the first
 * unit which will bring the value to below a threshold of 2048 will be chosen.
 * @param long_labels whether to use short ("K") or long ("KiB") unit labels
 * @param label will be set to the appropriate unit label
 *
 * @return the size in the appropriate unit
 */
double humanize_size(off_t bytes, const char target_unit, int long_labels,
		const char **label)
{
	static const char *shortlabels[] = {"B", "K", "M", "G", "T", "P"};
	static const char *longlabels[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
	static const int unitcount = sizeof(shortlabels) / sizeof(shortlabels[0]);

	const char **labels = long_labels ? longlabels : shortlabels;
	double val = (double)bytes;
	int index;

	for(index = 0; index < unitcount - 1; index++) {
		if(target_unit != '\0' && shortlabels[index][0] == target_unit) {
			break;
		} else if(target_unit == '\0' && val <= 2048.0 && val >= -2048.0) {
			break;
		}
		val /= 1024.0;
	}

	if(label) {
		*label = labels[index];
	}

	return val;
}

void print_packages(const alpm_list_t *packages)
{
	const alpm_list_t *i;
	if(!config->print_format) {
		config->print_format = strdup("%l");
	}
	for(i = packages; i; i = alpm_list_next(i)) {
		pmpkg_t *pkg = alpm_list_getdata(i);
		char *string = strdup(config->print_format);
		char *temp = string;
		/* %n : pkgname */
		if(strstr(temp,"%n")) {
			string = strreplace(temp, "%n", alpm_pkg_get_name(pkg));
			free(temp);
			temp = string;
		}
		/* %v : pkgver */
		if(strstr(temp,"%v")) {
			string = strreplace(temp, "%v", alpm_pkg_get_version(pkg));
			free(temp);
			temp = string;
		}
		/* %l : location */
		if(strstr(temp,"%l")) {
			char *pkgloc = pkg_get_location(pkg);
			string = strreplace(temp, "%l", pkgloc);
			free(pkgloc);
			free(temp);
			temp = string;
		}
		/* %r : repo */
		if(strstr(temp,"%r")) {
			const char *repo = "local";
			pmdb_t *db = alpm_pkg_get_db(pkg);
			if(db) {
				repo = alpm_db_get_name(db);
			}
			string = strreplace(temp, "%r", repo);
			free(temp);
			temp = string;
		}
		/* %s : size */
		if(strstr(temp,"%s")) {
			char *size;
			pm_asprintf(&size, "%jd", (intmax_t)pkg_get_size(pkg));
			string = strreplace(temp, "%s", size);
			free(size);
			free(temp);
		}
		printf("%s\n",string);
		free(string);
	}
}

/* Helper function for comparing strings using the
 * alpm "compare func" signature */
int str_cmp(const void *s1, const void *s2)
{
	return strcmp(s1, s2);
}

void display_new_optdepends(pmpkg_t *oldpkg, pmpkg_t *newpkg)
{
	alpm_list_t *old = alpm_pkg_get_optdepends(oldpkg);
	alpm_list_t *new = alpm_pkg_get_optdepends(newpkg);
	alpm_list_t *optdeps = alpm_list_diff(new,old,str_cmp);
	if(optdeps) {
		printf(_("New optional dependencies for %s\n"), alpm_pkg_get_name(newpkg));
		list_display_linebreak("   ", optdeps);
	}
	alpm_list_free(optdeps);
}

void display_optdepends(pmpkg_t *pkg)
{
	alpm_list_t *optdeps = alpm_pkg_get_optdepends(pkg);
	if(optdeps) {
		printf(_("Optional dependencies for %s\n"), alpm_pkg_get_name(pkg));
		list_display_linebreak("   ", optdeps);
	}
}

static void display_repo_list(const char *dbname, alpm_list_t *list)
{
	const char *prefix= "  ";

	printf(":: ");
	printf(_("Repository %s\n"), dbname);
	list_display(prefix, list);
}

void select_display(const alpm_list_t *pkglist)
{
	const alpm_list_t *i;
	int nth = 1;
	alpm_list_t *list = NULL;
	char *string = NULL;
	const char *dbname = NULL;

	for (i = pkglist; i; i = i->next) {
		pmpkg_t *pkg = alpm_list_getdata(i);
		pmdb_t *db = alpm_pkg_get_db(pkg);

		if(!dbname)
			dbname = alpm_db_get_name(db);
		if(strcmp(alpm_db_get_name(db), dbname) != 0) {
			display_repo_list(dbname, list);
			FREELIST(list);
			dbname = alpm_db_get_name(db);
		}
		string = NULL;
		pm_asprintf(&string, "%d) %s", nth, alpm_pkg_get_name(pkg));
		list = alpm_list_add(list, string);
		nth++;
	}
	display_repo_list(dbname, list);
	FREELIST(list);
}

static int parseindex(char *s, int *val, int min, int max)
{
	char *endptr = NULL;
	int n = strtol(s, &endptr, 10);
	if(*endptr == '\0') {
		if(n < min || n > max) {
			fprintf(stderr, _("Invalid value: %d is not between %d and %d\n"),
					n, min, max);
			return -1;
		}
		*val = n;
		return 0;
	} else {
		fprintf(stderr, _("Invalid number: %s\n"), s);
		return -1;
	}
}

static int multiselect_parse(char *array, int count, char *response)
{
	char *str, *saveptr;

	for (str = response; ; str = NULL) {
		int include = 1;
		int start, end;
		char *ends = NULL;
		char *starts = strtok_r(str, " ", &saveptr);

		if(starts == NULL) {
			break;
		}
		strtrim(starts);
		int len = strlen(starts);
		if(len == 0)
			continue;

		if(*starts == '^') {
			starts++;
			len--;
			include = 0;
		} else if(str) {
			/* if first token is including, we unselect all targets */
			memset(array, 0, count);
		}

		if(len > 1) {
			/* check for range */
			char *p;
			if((p = strchr(starts + 1, '-'))) {
				*p = 0;
				ends = p + 1;
			}
		}

		if(parseindex(starts, &start, 1, count) != 0)
			return -1;

		if(!ends) {
			array[start-1] = include;
		} else {
			int d;
			if(parseindex(ends, &end, start, count) != 0) {
				return -1;
			}
			for(d = start; d <= end; d++) {
				array[d-1] = include;
			}
		}
	}

	return 0;
}

int multiselect_question(char *array, int count)
{
	char response[64];
	FILE *stream;

	if(config->noconfirm) {
		stream = stdout;
	} else {
		/* Use stderr so questions are always displayed when redirecting output */
		stream = stderr;
	}

	while(1) {
		memset(array, 1, count);

		fprintf(stream, "\n");
		fprintf(stream, _("Enter a selection (default=all)"));
		fprintf(stream,	": ");

		if(config->noconfirm) {
			fprintf(stream, "\n");
			break;
		}

		flush_term_input();

		if(fgets(response, sizeof(response), stdin)) {
			strtrim(response);
			if(strlen(response) > 0) {
				if(multiselect_parse(array, count, response) == -1) {
					/* only loop if user gave an invalid answer */
					continue;
				}
			}
		}
		break;
	}
	return 0;
}

int select_question(int count)
{
	char response[32];
	FILE *stream;
	int preset = 1;

	if(config->noconfirm) {
		stream = stdout;
	} else {
		/* Use stderr so questions are always displayed when redirecting output */
		stream = stderr;
	}

	while(1) {
		fprintf(stream, "\n");
		fprintf(stream, _("Enter a number (default=%d)"), preset);
		fprintf(stream,	": ");

		if(config->noconfirm) {
			fprintf(stream, "\n");
			break;
		}

		flush_term_input();

		if(fgets(response, sizeof(response), stdin)) {
			strtrim(response);
			if(strlen(response) > 0) {
				int n;
				if(parseindex(response, &n, 1, count) != 0)
					continue;
				return (n - 1);
			}
		}
		break;
	}

	return (preset - 1);
}


/* presents a prompt and gets a Y/N answer */
static int question(short preset, char *fmt, va_list args)
{
	char response[32];
	FILE *stream;

	if(config->noconfirm) {
		stream = stdout;
	} else {
		/* Use stderr so questions are always displayed when redirecting output */
		stream = stderr;
	}

	/* ensure all text makes it to the screen before we prompt the user */
	fflush(stdout);
	fflush(stderr);

	vfprintf(stream, fmt, args);

	if(preset) {
		fprintf(stream, " %s ", _("[Y/n]"));
	} else {
		fprintf(stream, " %s ", _("[y/N]"));
	}

	if(config->noconfirm) {
		fprintf(stream, "\n");
		return preset;
	}

	fflush(stream);
	flush_term_input();

	if(fgets(response, sizeof(response), stdin)) {
		strtrim(response);
		if(strlen(response) == 0) {
			return preset;
		}

		if(strcasecmp(response, _("Y")) == 0 || strcasecmp(response, _("YES")) == 0) {
			return 1;
		} else if(strcasecmp(response, _("N")) == 0 || strcasecmp(response, _("NO")) == 0) {
			return 0;
		}
	}
	return 0;
}

int yesno(char *fmt, ...)
{
	int ret;
	va_list args;

	va_start(args, fmt);
	ret = question(1, fmt, args);
	va_end(args);

	return ret;
}

int noyes(char *fmt, ...)
{
	int ret;
	va_list args;

	va_start(args, fmt);
	ret = question(0, fmt, args);
	va_end(args);

	return ret;
}

int pm_printf(pmloglevel_t level, const char *format, ...)
{
	int ret;
	va_list args;

	/* print the message using va_arg list */
	va_start(args, format);
	ret = pm_vfprintf(stdout, level, format, args);
	va_end(args);

	return ret;
}

int pm_fprintf(FILE *stream, pmloglevel_t level, const char *format, ...)
{
	int ret;
	va_list args;

	/* print the message using va_arg list */
	va_start(args, format);
	ret = pm_vfprintf(stream, level, format, args);
	va_end(args);

	return ret;
}

int pm_asprintf(char **string, const char *format, ...)
{
	int ret = 0;
	va_list args;

	/* print the message using va_arg list */
	va_start(args, format);
	if(vasprintf(string, format, args) == -1) {
		pm_fprintf(stderr, PM_LOG_ERROR,  _("failed to allocate string\n"));
		ret = -1;
	}
	va_end(args);

	return ret;
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
		case PM_LOG_ERROR:
			pm_asprintf(string, _("error: %s"), msg);
			break;
		case PM_LOG_WARNING:
			pm_asprintf(string, _("warning: %s"), msg);
			break;
		case PM_LOG_DEBUG:
			pm_asprintf(string, "debug: %s", msg);
			break;
		case PM_LOG_FUNCTION:
			pm_asprintf(string, "function: %s", msg);
			break;
		default:
			pm_asprintf(string, "%s", msg);
			break;
	}
	free(msg);

	return ret;
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
		case PM_LOG_ERROR:
			fprintf(stream, _("error: "));
			break;
		case PM_LOG_WARNING:
			fprintf(stream, _("warning: "));
			break;
		case PM_LOG_DEBUG:
			fprintf(stream, "debug: ");
			break;
		case PM_LOG_FUNCTION:
			fprintf(stream, "function: ");
			break;
		default:
			break;
	}

	/* print the message using va_arg list */
	ret = vfprintf(stream, format, args);
	return ret;
}

#ifndef HAVE_STRNDUP
/* A quick and dirty implementation derived from glibc */
static size_t strnlen(const char *s, size_t max)
{
    register const char *p;
    for(p = s; *p && max--; ++p);
    return (p - s);
}

char *strndup(const char *s, size_t n)
{
  size_t len = strnlen(s, n);
  char *new = (char *) malloc(len + 1);

  if(new == NULL)
    return NULL;

  new[len] = '\0';
  return (char *)memcpy(new, s, len);
}
#endif

/* vim: set ts=2 sw=2 noet: */
