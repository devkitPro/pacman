/*
 *  util.c
 *
 *  Copyright (c) 2002-2007 by Judd Vinet <jvinet@zeroflux.org>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <time.h>
#include <syslog.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

/* libarchive */
#include <archive.h>
#include <archive_entry.h>

/* libalpm */
#include "util.h"
#include "log.h"
#include "package.h"
#include "alpm.h"
#include "alpm_list.h"
#include "md5.h"

#ifndef HAVE_STRSEP
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
#endif

int _alpm_makepath(const char *path)
{
	return(_alpm_makepath_mode(path, 0755));
}

/* does the same thing as 'mkdir -p' */
int _alpm_makepath_mode(const char *path, mode_t mode)
{
	/* A bit of pointer hell here. Descriptions:
	 * orig - a copy of path so we can safely butcher it with strsep
	 * str - the current position in the path string (after the delimiter)
	 * ptr - the original position of str after calling strsep
	 * incr - incrementally generated path for use in stat/mkdir call
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
				if(mkdir(incr, mode)) {
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

#define CPBUFSIZE 8 * 1024

int _alpm_copyfile(const char *src, const char *dest)
{
	FILE *in, *out;
	size_t len;
	char *buf;
	int ret = 0;

	in = fopen(src, "rb");
	if(in == NULL) {
		return(1);
	}
	out = fopen(dest, "wb");
	if(out == NULL) {
		fclose(in);
		return(1);
	}

	CALLOC(buf, (size_t)CPBUFSIZE, (size_t)1, ret = 1; goto cleanup;);

	/* do the actual file copy */
	while((len = fread(buf, 1, CPBUFSIZE, in))) {
		fwrite(buf, 1, len, out);
	}

	/* chmod dest to permissions of src, as long as it is not a symlink */
	struct stat statbuf;
	if(!stat(src, &statbuf)) {
		if(! S_ISLNK(statbuf.st_mode)) {
			fchmod(fileno(out), statbuf.st_mode);
		}
	} else {
		/* stat was unsuccessful */
		ret = 1;
	}

cleanup:
	fclose(in);
	fclose(out);
	FREE(buf);
	return(ret);
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

	pch = (str + (strlen(str) - 1));
	while(isspace((int)*pch)) {
		pch--;
	}
	*++pch = '\0';

	return(str);
}

/* Helper function for _alpm_strreplace */
static void _strnadd(char **str, const char *append, unsigned int count)
{
	if(*str) {
		*str = realloc(*str, strlen(*str) + count + 1);
	} else {
		*str = calloc(count + 1, sizeof(char));
	}

	strncat(*str, append, count);
}

/* Replace all occurances of 'needle' with 'replace' in 'str', returning
 * a new string (must be free'd) */
char *_alpm_strreplace(const char *str, const char *needle, const char *replace)
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


/* Create a lock file */
int _alpm_lckmk()
{
	int fd;
	char *dir, *ptr;
	const char *file = alpm_option_get_lockfile();

	/* create the dir of the lockfile first */
	dir = strdup(file);
	ptr = strrchr(dir, '/');
	if(ptr) {
		*ptr = '\0';
	}
	_alpm_makepath(dir);
	FREE(dir);

	while((fd = open(file, O_WRONLY | O_CREAT | O_EXCL, 0000)) == -1
			&& errno == EINTR);
	return(fd > 0 ? fd : -1);
}

/* Remove a lock file */
int _alpm_lckrm()
{
	const char *file = alpm_option_get_lockfile();
	if(unlink(file) == -1 && errno != ENOENT) {
		return(-1);
	}
	return(0);
}

/* Compression functions */

/**
 * @brief Unpack a specific file or all files in an archive.
 *
 * @param archive  the archive to unpack
 * @param prefix   where to extract the files
 * @param fn       a file within the archive to unpack or NULL for all
 */
int _alpm_unpack(const char *archive, const char *prefix, const char *fn)
{
	int ret = 1;
	mode_t oldmask;
	struct archive *_archive;
	struct archive_entry *entry;
	char expath[PATH_MAX];

	ALPM_LOG_FUNC;

	if((_archive = archive_read_new()) == NULL)
		RET_ERR(PM_ERR_LIBARCHIVE, -1);

	archive_read_support_compression_all(_archive);
	archive_read_support_format_all(_archive);

	if(archive_read_open_filename(_archive, archive,
				ARCHIVE_DEFAULT_BYTES_PER_BLOCK) != ARCHIVE_OK) {
		_alpm_log(PM_LOG_ERROR, _("could not open %s: %s\n"), archive,
				archive_error_string(_archive));
		RET_ERR(PM_ERR_PKG_OPEN, -1);
	}

	oldmask = umask(0022);
	while(archive_read_next_header(_archive, &entry) == ARCHIVE_OK) {
		const struct stat *st;
		const char *entryname; /* the name of the file in the archive */

		st = archive_entry_stat(entry);
		entryname = archive_entry_pathname(entry);
		
		if(S_ISREG(st->st_mode)) {
			archive_entry_set_mode(entry, 0644);
		} else if(S_ISDIR(st->st_mode)) {
			archive_entry_set_mode(entry, 0755);
		}

		/* If a specific file was requested skip entries that don't match. */
		if (fn && strcmp(fn, entryname)) {
			_alpm_log(PM_LOG_DEBUG, "skipping: %s\n", entryname);
			if (archive_read_data_skip(_archive) != ARCHIVE_OK) {
				ret = 1;
				goto cleanup;
			}
			continue;
		}

		/* Extract the archive entry. */
		ret = 0;
		snprintf(expath, PATH_MAX, "%s/%s", prefix, entryname);
		archive_entry_set_pathname(entry, expath);

		int readret = archive_read_extract(_archive, entry, 0);
		if(readret == ARCHIVE_WARN) {
			/* operation succeeded but a non-critical error was encountered */
			_alpm_log(PM_LOG_DEBUG, "warning extracting %s (%s)\n",
					entryname, archive_error_string(_archive));
		} else if(readret != ARCHIVE_OK) {
			_alpm_log(PM_LOG_ERROR, _("could not extract %s (%s)\n"),
					entryname, archive_error_string(_archive));
			ret = 1;
			goto cleanup;
		}

		if(fn) {
			break;
		}
	}

cleanup:
	umask(oldmask);
	archive_read_finish(_archive);
	return(ret);
}

/* does the same thing as 'rm -rf' */
int _alpm_rmrf(const char *path)
{
	int errflag = 0;
	struct dirent *dp;
	DIR *dirp;
	char name[PATH_MAX];
	struct stat st;

	if(_alpm_lstat(path, &st) == 0) {
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

int _alpm_logaction(unsigned short usesyslog, FILE *f,
		const char *fmt, va_list args)
{
	int ret = 0;

	if(usesyslog) {
		/* we can't use a va_list more than once, so we need to copy it
		 * so we can use the original when calling vfprintf below. */
		va_list args_syslog;
		va_copy(args_syslog, args);
		vsyslog(LOG_WARNING, fmt, args_syslog);
		va_end(args_syslog);
	}

	if(f) {
		time_t t;
		struct tm *tm;

		t = time(NULL);
		tm = localtime(&t);

		/* Use ISO-8601 date format */
		fprintf(f, "[%04d-%02d-%02d %02d:%02d] ",
						tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
						tm->tm_hour, tm->tm_min);
		ret = vfprintf(f, fmt, args);
		fflush(f);
	}

	return(ret);
}

int _alpm_ldconfig(const char *root)
{
	char line[PATH_MAX];

	snprintf(line, PATH_MAX, "%setc/ld.so.conf", root);
	if(access(line, F_OK) == 0) {
		snprintf(line, PATH_MAX, "%ssbin/ldconfig", root);
		if(access(line, X_OK) == 0) {
			char cmd[PATH_MAX];
			snprintf(cmd, PATH_MAX, "%s -r %s", line, root);
			system(cmd);
		}
	}

	return(0);
}

/* Helper function for comparing strings using the
 * alpm "compare func" signature */
int _alpm_str_cmp(const void *s1, const void *s2)
{
	return(strcmp(s1, s2));
}

/** Find a filename in a registered alpm cachedir.
 * @param filename name of file to find
 * @return malloced path of file, NULL if not found
 */
char *_alpm_filecache_find(const char* filename)
{
	char path[PATH_MAX];
	char *retpath;
	alpm_list_t *i;

	/* Loop through the cache dirs until we find a matching file */
	for(i = alpm_option_get_cachedirs(); i; i = alpm_list_next(i)) {
		snprintf(path, PATH_MAX, "%s%s", (char*)alpm_list_getdata(i),
				filename);
		if(access(path, R_OK) == 0) {
			retpath = strdup(path);
			_alpm_log(PM_LOG_DEBUG, "found cached pkg: %s\n", retpath);
			return(retpath);
		}
	}
	/* package wasn't found in any cachedir */
	return(NULL);
}

/** Check the alpm cachedirs for existance and find a writable one.
 * If no valid cache directory can be found, use /tmp.
 * @return pointer to a writable cache directory.
 */
const char *_alpm_filecache_setup(void)
{
	struct stat buf;
	alpm_list_t *i, *tmp;
	char *cachedir;

	/* Loop through the cache dirs until we find a writeable dir */
	for(i = alpm_option_get_cachedirs(); i; i = alpm_list_next(i)) {
		cachedir = alpm_list_getdata(i);
		if(stat(cachedir, &buf) != 0) {
			/* cache directory does not exist.... try creating it */
			_alpm_log(PM_LOG_WARNING, _("no %s cache exists, creating...\n"),
					cachedir);
			if(_alpm_makepath(cachedir) == 0) {
				_alpm_log(PM_LOG_DEBUG, "using cachedir: %s\n", cachedir);
				return(cachedir);
			}
		} else if(S_ISDIR(buf.st_mode) && (buf.st_mode & S_IWUSR)) {
			_alpm_log(PM_LOG_DEBUG, "using cachedir: %s\n", cachedir);
			return(cachedir);
		}
	}

	/* we didn't find a valid cache directory. use /tmp. */
	tmp = alpm_list_add(NULL, strdup("/tmp/"));
	alpm_option_set_cachedirs(tmp);
	_alpm_log(PM_LOG_DEBUG, "using cachedir: %s", "/tmp/\n");
	_alpm_log(PM_LOG_WARNING, _("couldn't create package cache, using /tmp instead\n"));
	return(alpm_list_getdata(tmp));
}

/** lstat wrapper that treats /path/dirsymlink/ the same as /path/dirsymlink.
 * Linux lstat follows POSIX semantics and still performs a dereference on
 * the first, and for uses of lstat in libalpm this is not what we want.
 * @param path path to file to lstat
 * @param buf structure to fill with stat information
 * @return the return code from lstat
 */
int _alpm_lstat(const char *path, struct stat *buf)
{
	int ret;
	char *newpath = strdup(path);
	int len = strlen(newpath);

	/* strip the trailing slash if one exists */
	if(len != 0 && newpath[len - 1] == '/') {
			newpath[len - 1] = '\0';
	}

	ret = lstat(newpath, buf);

	FREE(newpath);
	return(ret);
}

/** Get the md5 sum of file.
 * @param filename name of the file
 * @return the checksum on success, NULL on error
 * @addtogroup alpm_misc
 */
char SYMEXPORT *alpm_get_md5sum(const char *filename)
{
	unsigned char output[16];
	char *md5sum;
	int ret, i;

	ALPM_LOG_FUNC;

	ASSERT(filename != NULL, return(NULL));

	/* allocate 32 chars plus 1 for null */
	md5sum = calloc(33, sizeof(char));
	ret = md5_file(filename, output);

	if (ret > 0) {
		RET_ERR(PM_ERR_NOT_A_FILE, NULL);
	}

	/* Convert the result to something readable */
	for (i = 0; i < 16; i++) {
		/* sprintf is acceptable here because we know our output */
		sprintf(md5sum +(i * 2), "%02x", output[i]);
	}
	md5sum[32] = '\0';

	_alpm_log(PM_LOG_DEBUG, "md5(%s) = %s\n", filename, md5sum);
	return(md5sum);
}

int _alpm_test_md5sum(const char *filepath, const char *md5sum)
{
	char *md5sum2;
	int ret;

	md5sum2 = alpm_get_md5sum(filepath);

	if(md5sum == NULL || md5sum2 == NULL) {
		ret = -1;
	} else if(strcmp(md5sum, md5sum2) != 0) {
		ret = 1;
	} else {
		ret = 0;
	}

	FREE(md5sum2);
	return(ret);
}

char *_alpm_archive_fgets(char *line, size_t size, struct archive *a)
{
	/* for now, just read one char at a time until we get to a
	 * '\n' char. we can optimize this later with an internal
	 * buffer. */
	/* leave room for zero terminator */
	char *last = line + size - 1;
	char *i;

	for(i = line; i < last; i++) {
		int ret = archive_read_data(a, i, 1);
		/* special check for first read- if null, return null,
		 * this indicates EOF */
		if(i == line && (ret <= 0 || *i == '\0')) {
			return(NULL);
		}
		/* check if read value was null or newline */
		if(ret <= 0 || *i == '\0' || *i == '\n') {
			last = i + 1;
			break;
		}
	}

	/* always null terminate the buffer */
	*last = '\0';

	return(line);
}

/* vim: set ts=2 sw=2 noet: */
