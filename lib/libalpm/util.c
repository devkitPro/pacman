/*
 *  util.c
 *
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2005 by Christian Hamar <krics@linuxforum.hu>
 *  Copyright (c) 2006 by David Kimpe <dnaku@frugalware.org>
 *  Copyright (c) 2005, 2006 by Miklos Vajna <vmiklos@frugalware.org>
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
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <time.h>
#include <syslog.h>
#include <errno.h>

/* libarchive */
#include <archive.h>
#include <archive_entry.h>

/* libalpm */
#include "util.h"
#include "alpm_list.h"
#include "log.h"
#include "error.h"
#include "package.h"
#include "alpm.h"

#ifdef __sun__
/* This is a replacement for strsep which is not portable (missing on Solaris).
 * Copyright (c) 2001 by François Gouget <fgouget_at_codeweavers.com> */
char* strsep(char** str, const char* delims)
{
	char* token;

	if (*str==NULL) {
		/* No more tokens */
		return NULL;
	}

	token=*str;
	while (**str!='\0') {
		if (strchr(delims,**str)!=NULL) {
			**str='\0';
			(*str)++;
			return token;
		}
		(*str)++;
	}
	/* There is no other token */
	*str=NULL;
	return token;
}

/* Backported from Solaris Express 4/06
 * Copyright (c) 2006 Sun Microsystems, Inc. */
char *mkdtemp(char *template)
{
	char *t = alloca(strlen(template) + 1);
	char *r;

	/* Save template */
	(void) strcpy(t, template);
	for (; ; ) {
		r = mktemp(template);

		if (*r == '\0')
			return (NULL);

		if (mkdir(template, 0700) == 0)
			return (r);

		/* Other errors indicate persistent conditions. */
		if (errno != EEXIST)
			return (NULL);

		/* Reset template */
		(void) strcpy(template, t);
	}
}
#endif

/* does the same thing as 'mkdir -p' */
int _alpm_makepath(const char *path)
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
					FREE(orig);
					umask(oldmask);
					_alpm_log(PM_LOG_ERROR, _("failed to make path '%s' : %s"),
										path, strerror(errno));
					return(1);
				}
			}
		}
	}
	FREE(orig);
	umask(oldmask);
	return(0);
}

int _alpm_copyfile(const char *src, const char *dest)
{
	FILE *in, *out;
	size_t len;
	char buf[4097];

	in = fopen(src, "r");
	if(in == NULL) {
		return(1);
	}
	out = fopen(dest, "w");
	if(out == NULL) {
		fclose(in);
		return(1);
	}

	while((len = fread(buf, 1, 4096, in))) {
		fwrite(buf, 1, len, out);
	}

	fclose(in);
	fclose(out);
	return(0);
}

/* Convert a string to uppercase
 */
char *_alpm_strtoupper(char *str)
{
	char *ptr = str;

	while(*ptr) {
		(*ptr) = toupper(*ptr);
		ptr++;
	}
	return(str);
}

/* Trim whitespace and newlines from a string
 */
char *_alpm_strtrim(char *str)
{
	char *pch = str;

	if(*str == '\0') {
		/* string is empty, so we're done. */
		return(str);
	}

	while(isspace((int)*pch)) {
		pch++;
	}
	if(pch != str) {
		memmove(str, pch, (strlen(pch) + 1));
	}

	/* check if there wasn't anything but whitespace in the string. */
	if(*str == '\0') {
		return(str);
	}

	pch = (char *)(str + (strlen(str) - 1));
	while(isspace((int)*pch)) {
		pch--;
	}
	*++pch = '\0';

	return(str);
}

/* Create a lock file
 */
int _alpm_lckmk(const char *file)
{
	int fd, count = 0;
	char *dir, *ptr;

	/* create the dir of the lockfile first */
	dir = strdup(file);
	ptr = strrchr(dir, '/');
	if(ptr) {
		*ptr = '\0';
	}
	_alpm_makepath(dir);

	while((fd = open(file, O_WRONLY | O_CREAT | O_EXCL, 0000)) == -1 && errno == EACCES) { 
		if(++count < 1) {
			sleep(1);
		}	else {
			return(-1);
		}
	}

	free(dir);

	return(fd > 0 ? fd : -1);
}

/* Remove a lock file
 */
int _alpm_lckrm(const char *file)
{
	if(unlink(file) == -1 && errno != ENOENT) {
		return(-1);
	}
	return(0);
}

/* Compression functions
 */

int _alpm_unpack(const char *archive, const char *prefix, const char *fn)
{
	register struct archive *_archive;
	struct archive_entry *entry;
	char expath[PATH_MAX];
	const int archive_flags = ARCHIVE_EXTRACT_OWNER |
	                          ARCHIVE_EXTRACT_PERM |
	                          ARCHIVE_EXTRACT_TIME;

	ALPM_LOG_FUNC;

	if((_archive = archive_read_new()) == NULL)
		RET_ERR(PM_ERR_LIBARCHIVE_ERROR, -1);

	archive_read_support_compression_all(_archive);
	archive_read_support_format_all(_archive);

	if(archive_read_open_file(_archive, archive, ARCHIVE_DEFAULT_BYTES_PER_BLOCK) != ARCHIVE_OK) {
		_alpm_log(PM_LOG_ERROR, _("could not open %s: %s\n"), archive, archive_error_string(_archive));
		RET_ERR(PM_ERR_PKG_OPEN, -1);
	}

	while(archive_read_next_header(_archive, &entry) == ARCHIVE_OK) {
		if (fn && strcmp(fn, archive_entry_pathname(entry))) {
			if (archive_read_data_skip(_archive) != ARCHIVE_OK)
				return(1);
			continue;
		}
		snprintf(expath, PATH_MAX, "%s/%s", prefix, archive_entry_pathname(entry));
		archive_entry_set_pathname(entry, expath);
		if(archive_read_extract(_archive, entry, archive_flags) != ARCHIVE_OK) {
			_alpm_log(PM_LOG_ERROR, _("could not extract %s: %s\n"), archive_entry_pathname(entry), archive_error_string(_archive));
			 return(1);
		}

		if(fn) {
			break;
		}
	}
	
	archive_read_finish(_archive);
	return(0);
}

/* does the same thing as 'rm -rf' */
int _alpm_rmrf(const char *path)
{
	int errflag = 0;
	struct dirent *dp;
	DIR *dirp;
	char name[PATH_MAX];
	struct stat st;

	if(lstat(path, &st) == 0) {
		if(!S_ISDIR(st.st_mode)) {
			if(!unlink(path)) {
				return(0);
			} else {
				if(errno == ENOENT) {
					return(0);
				} else {
					return(1);
				}
			}
		} else {
			if((dirp = opendir(path)) == (DIR *)-1) {
				return(1);
			}
			for(dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
				if(dp->d_ino) {
					sprintf(name, "%s/%s", path, dp->d_name);
					if(strcmp(dp->d_name, "..") && strcmp(dp->d_name, ".")) {
						errflag += _alpm_rmrf(name);
					}
				}
			}
			closedir(dirp);
			if(rmdir(path)) {
				errflag++;
			}
		}
		return(errflag);
	}
	return(0);
}

int _alpm_logaction(unsigned short usesyslog, FILE *f, const char *str)
{
	_alpm_log(PM_LOG_DEBUG, _("logaction called: %s"), str);

	if(usesyslog) {
		syslog(LOG_WARNING, "%s", str);
	}

	if(f) {
		time_t t;
		struct tm *tm;

		t = time(NULL);
		tm = localtime(&t);

		/* Use ISO-8601 date format */
		fprintf(f, "[%04d-%02d-%02d %02d:%02d] %s\n",
		        tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
		        tm->tm_hour, tm->tm_min, str);

		fflush(f);
	}

	return(0);
}

int _alpm_ldconfig(const char *root)
{
	char line[PATH_MAX];
	struct stat buf;

	snprintf(line, PATH_MAX, "%setc/ld.so.conf", root);
	if(stat(line, &buf) == 0) {
		snprintf(line, PATH_MAX, "%ssbin/ldconfig", root);
		if(stat(line, &buf) == 0) {
			char cmd[PATH_MAX];
			snprintf(cmd, PATH_MAX, "%s -r %s", line, root);
			system(cmd);
		}
	}

	return(0);
}

/* convert a time_t to a string - buffer MUST be large enough for
 * YYYYMMDDHHMMSS - 15 chars */
void _alpm_time2string(time_t t, char *buffer)
{
	if(buffer) {
		struct tm *lt;
		lt = localtime(&t);
		sprintf(buffer, "%4d%02d%02d%02d%02d%02d",
						lt->tm_year+1900, lt->tm_mon+1, lt->tm_mday,
						lt->tm_hour, lt->tm_min, lt->tm_sec);
		buffer[14] = '\0';
	}
}

/* Helper function for comparing strings using the
 * alpm "compare func" signature */
int _alpm_str_cmp(const void *s1, const void *s2)
{
	return(strcmp(s1, s2));
}


/* vim: set ts=2 sw=2 noet: */
