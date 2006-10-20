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

#if defined(__APPLE__) || defined(__OpenBSD__)
#include <sys/syslimits.h>
#endif
#if defined(__APPLE__) || defined(__OpenBSD__) || defined(__sun__)
#include <sys/stat.h>
#endif

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef __sun__
#include <alloca.h>
#endif
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <dirent.h>
#include <time.h>
#include <syslog.h>
#include <sys/wait.h>
#include <libintl.h>
#ifdef CYGWIN
#include <limits.h> /* PATH_MAX */
#endif
#include <sys/statvfs.h>
#ifndef __sun__
#include <mntent.h>
#endif
#include <regex.h>

/* pacman */
#include "log.h"
#include "list.h"
#include "trans.h"
#include "sync.h"
#include "util.h"
#include "error.h"
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
char * mkdtemp(char *template)
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
int _alpm_makepath(char *path)
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

int _alpm_copyfile(char *src, char *dest)
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
	return str;
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
int _alpm_lckmk(char *file)
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

	return(fd > 0 ? fd : -1);
}

/* Remove a lock file
 */
int _alpm_lckrm(char *file)
{
	if(unlink(file) == -1 && errno != ENOENT) {
		return(-1);
	}
	return(0);
}

/* Compression functions
 */

int _alpm_unpack(char *archive, const char *prefix, const char *fn)
{
	register struct archive *_archive;
	struct archive_entry *entry;
	char expath[PATH_MAX];

	if ((_archive = archive_read_new ()) == NULL)
		RET_ERR(PM_ERR_LIBARCHIVE_ERROR, -1);

	archive_read_support_compression_all(_archive);
	archive_read_support_format_all (_archive);

	if (archive_read_open_file (_archive, archive, ARCHIVE_DEFAULT_BYTES_PER_BLOCK) != ARCHIVE_OK)
		RET_ERR(PM_ERR_PKG_OPEN, -1);

	while (archive_read_next_header (_archive, &entry) == ARCHIVE_OK) {
		if (fn && strcmp (fn, archive_entry_pathname (entry))) {
			if (archive_read_data_skip (_archive) != ARCHIVE_OK)
				return(1);
			continue;
		}
		snprintf(expath, PATH_MAX, "%s/%s", prefix, archive_entry_pathname (entry));
		archive_entry_set_pathname (entry, expath);
		if (archive_read_extract (_archive, entry, ARCHIVE_EXTRACT_FLAGS) != ARCHIVE_OK) {
			fprintf(stderr, _("could not extract %s: %s\n"), archive_entry_pathname (entry), archive_error_string (_archive));
			 return(1);
		}

		if (fn)
			break;
	}
	
	archive_read_finish (_archive);
	return(0);
}

/* does the same thing as 'rm -rf' */
int _alpm_rmrf(char *path)
{
	int errflag = 0;
	struct dirent *dp;
	DIR *dirp;
	char name[PATH_MAX];

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
					errflag += _alpm_rmrf(name);
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

int _alpm_logaction(unsigned char usesyslog, FILE *f, char *fmt, ...)
{
	char msg[1024];
	va_list args;

	va_start(args, fmt);
	vsnprintf(msg, 1024, fmt, args);
	va_end(args);

	if(usesyslog) {
		syslog(LOG_WARNING, "%s", msg);
	}

	if(f) {
		time_t t;
		struct tm *tm;

		t = time(NULL);
		tm = localtime(&t);

		fprintf(f, "[%02d/%02d/%02d %02d:%02d] %s\n",
		        tm->tm_mon+1, tm->tm_mday, tm->tm_year-100,
		        tm->tm_hour, tm->tm_min,
		        msg);
	}

	return(0);
}

int _alpm_ldconfig(char *root)
{
	char line[PATH_MAX];
	struct stat buf;

	snprintf(line, PATH_MAX, "%setc/ld.so.conf", root);
	if(!stat(line, &buf)) {
		snprintf(line, PATH_MAX, "%ssbin/ldconfig", root);
		if(!stat(line, &buf)) {
			char cmd[PATH_MAX];
			snprintf(cmd, PATH_MAX, "%s -r %s", line, root);
			system(cmd);
		}
	}

	return(0);
}

/* A cheap grep for text files, returns 1 if a substring
 * was found in the text file fn, 0 if it wasn't
 */
static int grep(const char *fn, const char *needle)
{
	FILE *fp;

	if((fp = fopen(fn, "r")) == NULL) {
		return(0);
	}
	while(!feof(fp)) {
		char line[1024];
		fgets(line, 1024, fp);
		if(feof(fp)) {
			continue;
		}
		if(strstr(line, needle)) {
			fclose(fp);
			return(1);
		}
	}
	fclose(fp);
	return(0);
}

int _alpm_runscriptlet(char *root, char *installfn, char *script, char *ver, char *oldver, pmtrans_t *trans)
{
	char scriptfn[PATH_MAX];
	char cmdline[PATH_MAX];
	char tmpdir[PATH_MAX] = "";
	char *scriptpath;
	struct stat buf;
	char cwd[PATH_MAX] = "";
	pid_t pid;
	int retval = 0;

	if(stat(installfn, &buf)) {
		/* not found */
		return(0);
	}

	if(!strcmp(script, "pre_upgrade") || !strcmp(script, "pre_install")) {
		snprintf(tmpdir, PATH_MAX, "%stmp/", root);
		if(stat(tmpdir, &buf)) {
			_alpm_makepath(tmpdir);
		}
		snprintf(tmpdir, PATH_MAX, "%stmp/alpm_XXXXXX", root);
		if(mkdtemp(tmpdir) == NULL) {
			_alpm_log(PM_LOG_ERROR, _("could not create temp directory"));
			return(1);
		}
		_alpm_unpack(installfn, tmpdir, ".INSTALL");
		snprintf(scriptfn, PATH_MAX, "%s/.INSTALL", tmpdir);
		/* chop off the root so we can find the tmpdir in the chroot */
		scriptpath = scriptfn + strlen(root) - 1;
	} else {
		STRNCPY(scriptfn, installfn, PATH_MAX);
		/* chop off the root so we can find the tmpdir in the chroot */
		scriptpath = scriptfn + strlen(root) - 1;
	}

	if(!grep(scriptfn, script)) {
		/* script not found in scriptlet file */
		goto cleanup;
	}

	/* save the cwd so we can restore it later */
	if(getcwd(cwd, PATH_MAX) == NULL) {
		_alpm_log(PM_LOG_ERROR, _("could not get current working directory"));
		/* in case of error, cwd content is undefined: so we set it to something */
		cwd[0] = 0;
	}

	/* just in case our cwd was removed in the upgrade operation */
	if(chdir(root) != 0) {
		_alpm_log(PM_LOG_ERROR, _("could not change directory to %s (%s)"), root, strerror(errno));
	}

	_alpm_log(PM_LOG_FLOW2, _("executing %s script..."), script);

	if(oldver) {
		snprintf(cmdline, PATH_MAX, "source %s %s %s %s",
				scriptpath, script, ver, oldver);
	} else {
		snprintf(cmdline, PATH_MAX, "source %s %s %s",
				scriptpath, script, ver);
	}
	_alpm_log(PM_LOG_DEBUG, "%s", cmdline);

	pid = fork();
	if(pid == -1) {
		_alpm_log(PM_LOG_ERROR, _("could not fork a new process (%s)"), strerror(errno));
		retval = 1;
		goto cleanup;
	}

	if(pid == 0) {
		FILE *pp;
		_alpm_log(PM_LOG_DEBUG, _("chrooting in %s"), root);
		if(chroot(root) != 0) {
			_alpm_log(PM_LOG_ERROR, _("could not change the root directory (%s)"), strerror(errno));
			return(1);
		}
		if(chdir("/") != 0) {
			_alpm_log(PM_LOG_ERROR, _("could not change directory to / (%s)"), strerror(errno));
			return(1);
		}
		umask(0022);
		_alpm_log(PM_LOG_DEBUG, _("executing \"%s\""), cmdline);
		pp = popen(cmdline, "r");
		if(!pp) {
			_alpm_log(PM_LOG_ERROR, _("call to popen failed (%s)"), strerror(errno));
			retval = 1;
			goto cleanup;
		}
		while(!feof(pp)) {
			char line[1024];
			if(fgets(line, 1024, pp) == NULL)
				break;
			/* "START <event desc>" */
			if((strlen(line) > strlen(STARTSTR)) && !strncmp(line, STARTSTR, strlen(STARTSTR))) {
				EVENT(trans, PM_TRANS_EVT_SCRIPTLET_START, _alpm_strtrim(line + strlen(STARTSTR)), NULL);
			/* "DONE <ret code>" */
			} else if((strlen(line) > strlen(DONESTR)) && !strncmp(line, DONESTR, strlen(DONESTR))) {
				EVENT(trans, PM_TRANS_EVT_SCRIPTLET_DONE, (void*)atol(_alpm_strtrim(line + strlen(DONESTR))), NULL);
			} else {
				EVENT(trans, PM_TRANS_EVT_SCRIPTLET_INFO, _alpm_strtrim(line), NULL);
			}
		}
		pclose(pp);
		exit(0);
	} else {
		if(waitpid(pid, 0, 0) == -1) {
			_alpm_log(PM_LOG_ERROR, _("call to waitpid failed (%s)"), strerror(errno));
			retval = 1;
			goto cleanup;
		}
	}

cleanup:
	if(strlen(tmpdir) && _alpm_rmrf(tmpdir)) {
		_alpm_log(PM_LOG_WARNING, _("could not remove tmpdir %s"), tmpdir);
	}
	if(strlen(cwd)) {
		chdir(cwd);
	}

	return(retval);
}

#ifndef __sun__
static long long get_freespace()
{
	struct mntent *mnt;
	char *table = MOUNTED;
	FILE *fp;
	long long ret=0;

	fp = setmntent (table, "r");
	if(!fp)
		        return(-1);
	while ((mnt = getmntent (fp)))
	{
		struct statvfs64 buf;

		statvfs64(mnt->mnt_dir, &buf);
		ret += buf.f_bavail * buf.f_bsize;
	}
	return(ret);
}

int _alpm_check_freespace(pmtrans_t *trans, pmlist_t **data)
{
	pmlist_t *i;
	long long pkgsize=0, freespace;

	for(i = trans->packages; i; i = i->next) {
		if(trans->type == PM_TRANS_TYPE_SYNC)
		{
			pmsyncpkg_t *sync = i->data;
			if(sync->type != PM_SYNC_TYPE_REPLACE) {
				pmpkg_t *pkg = sync->pkg;
				pkgsize += pkg->usize;
			}
		}
		else
		{
			pmpkg_t *pkg = i->data;
			pkgsize += pkg->size;
		}
	}
	freespace = get_freespace();
	_alpm_log(PM_LOG_DEBUG, _("check_freespace: total pkg size: %lld, disk space: %lld"), pkgsize, freespace);
	if(pkgsize > freespace) {
		if(data) {
			long long *ptr;
			if((ptr = (long long*)malloc(sizeof(long long)))==NULL) {
				_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(long long));
				pm_errno = PM_ERR_MEMORY;
				return(-1);
			}
			*ptr = pkgsize;
			*data = _alpm_list_add(*data, ptr);
			if((ptr = (long long*)malloc(sizeof(long long)))==NULL) {
				_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(long long));
				FREELIST(*data);
				pm_errno = PM_ERR_MEMORY;
				return(-1);
			}
			*ptr = freespace;
			*data = _alpm_list_add(*data, ptr);
		}
		pm_errno = PM_ERR_DISK_FULL;
		return(-1);
	}
	else {
		return(0);
	}
}

/* match a string against a regular expression */
int _alpm_reg_match(char *string, char *pattern)
{
	int result;
	regex_t reg;

	if(regcomp(&reg, pattern, REG_EXTENDED | REG_NOSUB | REG_ICASE) != 0) {
		RET_ERR(PM_ERR_INVALID_REGEX, -1);
	}
	result = regexec(&reg, string, 0, 0, 0);
	regfree(&reg);
	return(!(result));
}

#endif

/* vim: set ts=2 sw=2 noet: */
