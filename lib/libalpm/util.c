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
#include <sys/types.h>
#include <sys/stat.h>

/* libarchive */
#include <archive.h>
#include <archive_entry.h>

/* libalpm */
#include "util.h"
#include "log.h"
#include "error.h"
#include "package.h"
#include "alpm.h"

#ifndef HAVE_STRVERSCMP
/* GNU's strverscmp() function, taken from glibc 2.3.2 sources
 */

/* Compare strings while treating digits characters numerically.
   Copyright (C) 1997, 2002 Free Software Foundation, Inc.
   Contributed by Jean-François Bignolles <bignolle@ecoledoc.ibp.fr>, 1997.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.
*/

/* states: S_N: normal, S_I: comparing integral part, S_F: comparing
           fractionnal parts, S_Z: idem but with leading Zeroes only */
#define  S_N    0x0
#define  S_I    0x4
#define  S_F    0x8
#define  S_Z    0xC

/* result_type: CMP: return diff; LEN: compare using len_diff/diff */
#define  CMP    2
#define  LEN    3

/* Compare S1 and S2 as strings holding indices/version numbers,
   returning less than, equal to or greater than zero if S1 is less than,
   equal to or greater than S2 (for more info, see the texinfo doc).
*/

static int strverscmp (s1, s2)
     const char *s1;
     const char *s2;
{
  const unsigned char *p1 = (const unsigned char *) s1;
  const unsigned char *p2 = (const unsigned char *) s2;
  unsigned char c1, c2;
  int state;
  int diff;

  /* Symbol(s)    0       [1-9]   others  (padding)
     Transition   (10) 0  (01) d  (00) x  (11) -   */
  static const unsigned int next_state[] =
  {
      /* state    x    d    0    - */
      /* S_N */  S_N, S_I, S_Z, S_N,
      /* S_I */  S_N, S_I, S_I, S_I,
      /* S_F */  S_N, S_F, S_F, S_F,
      /* S_Z */  S_N, S_F, S_Z, S_Z
  };

  static const int result_type[] =
  {
      /* state   x/x  x/d  x/0  x/-  d/x  d/d  d/0  d/-
                 0/x  0/d  0/0  0/-  -/x  -/d  -/0  -/- */

      /* S_N */  CMP, CMP, CMP, CMP, CMP, LEN, CMP, CMP,
                 CMP, CMP, CMP, CMP, CMP, CMP, CMP, CMP,
      /* S_I */  CMP, -1,  -1,  CMP, +1,  LEN, LEN, CMP,
                 +1,  LEN, LEN, CMP, CMP, CMP, CMP, CMP,
      /* S_F */  CMP, CMP, CMP, CMP, CMP, LEN, CMP, CMP,
                 CMP, CMP, CMP, CMP, CMP, CMP, CMP, CMP,
      /* S_Z */  CMP, +1,  +1,  CMP, -1,  CMP, CMP, CMP,
                 -1,  CMP, CMP, CMP
  };

  if (p1 == p2)
    return 0;

  c1 = *p1++;
  c2 = *p2++;
  /* Hint: '0' is a digit too.  */
  state = S_N | ((c1 == '0') + (isdigit (c1) != 0));

  while ((diff = c1 - c2) == 0 && c1 != '\0')
    {
      state = next_state[state];
      c1 = *p1++;
      c2 = *p2++;
      state |= (c1 == '0') + (isdigit (c1) != 0);
    }

  state = result_type[state << 2 | (((c2 == '0') + (isdigit (c2) != 0)))];

  switch (state)
  {
    case CMP:
      return diff;

    case LEN:
      while (isdigit (*p1++))
	if (!isdigit (*p2++))
	  return 1;

      return isdigit (*p2) ? -1 : diff;

    default:
      return state;
  }
}
#endif

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

	/* do the actual file copy */
	while((len = fread(buf, 1, 4096, in))) {
		fwrite(buf, 1, len, out);
	}
	fclose(in);

	/* chmod dest to permissions of src, as long as it is not a symlink */
	struct stat statbuf;
	if(!stat(src, &statbuf)) {
		if(! S_ISLNK(statbuf.st_mode)) {
			fchmod(fileno(out), statbuf.st_mode);
		}
	} else {
		/* stat was unsuccessful */
		fclose(out);
		return(1);
	}

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
		*str = calloc(sizeof(char), count + 1);
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
	int fd, count = 0;
	char *dir, *ptr;
	const char *file = alpm_option_get_lockfile();

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

	if(archive_read_open_filename(_archive, archive,
				ARCHIVE_DEFAULT_BYTES_PER_BLOCK) != ARCHIVE_OK) {
		_alpm_log(PM_LOG_ERROR, _("could not open %s: %s\n"), archive, archive_error_string(_archive));
		RET_ERR(PM_ERR_PKG_OPEN, -1);
	}

	while(archive_read_next_header(_archive, &entry) == ARCHIVE_OK) {
		const char *entryname; /* the name of the file in the archive */
		entryname = archive_entry_pathname(entry);

		if (fn && strcmp(fn, entryname)) {
			if (archive_read_data_skip(_archive) != ARCHIVE_OK)
				return(1);
			continue;
		}
		snprintf(expath, PATH_MAX, "%s/%s", prefix, entryname);
		archive_entry_set_pathname(entry, expath);
		int ret = archive_read_extract(_archive, entry, archive_flags);
		if(ret == ARCHIVE_WARN) {
			/* operation succeeded but a non-critical error was encountered */
			_alpm_log(PM_LOG_DEBUG, "warning extracting %s (%s)\n",
					entryname, archive_error_string(_archive));
		} else if(ret != ARCHIVE_OK) {
			_alpm_log(PM_LOG_ERROR, _("could not extract %s (%s)\n"),
					entryname, archive_error_string(_archive));
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

int _alpm_logaction(unsigned short usesyslog, FILE *f, const char *fmt, va_list args)
{
	int ret = 0;

	if(usesyslog) {
		vsyslog(LOG_WARNING, fmt, args);
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
		fprintf(f, "\n");
		fflush(f);
	}

	return(ret);
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
