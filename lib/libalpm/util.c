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
#include <time.h>
#include <syslog.h>
#include <zlib.h>
#include <libtar.h>
/* pacman */
#include "log.h"
#include "util.h"
#include "alpm.h"

/* borrowed and modified from Per Liden's pkgutils (http://crux.nu) */
long _alpm_gzopen_frontend(char *pathname, int oflags, int mode)
{
	char* gzoflags;
	int fd;
	gzFile gzf;

	switch (oflags & O_ACCMODE) {
		case O_WRONLY:
			gzoflags = "w";
			break;
		case O_RDONLY:
			gzoflags = "r";
			break;
		case O_RDWR:
		default:
			errno = EINVAL;
			return -1;
	}
	
	if((fd = open(pathname, oflags, mode)) == -1) {
		return -1;
	}
	if((oflags & O_CREAT) && fchmod(fd, mode)) {
		return -1;
	}
	if(!(gzf = gzdopen(fd, gzoflags))) {
		errno = ENOMEM;
		return -1;
	}

	return (long)gzf;
}

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

	pch = (char*)(str + (strlen(str) - 1));
	while(isspace(*pch)) {
		pch--;
	}
	*++pch = '\0';

	return(str);
}

/* A cheap grep for text files, returns 1 if a substring
 * was found in the text file fn, 0 if it wasn't
 */
int _alpm_grep(const char *fn, const char *needle)
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

/* Create a lock file
 */
int _alpm_lckmk(char *file)
{
	int fd, count = 0;

	while((fd = open(file, O_WRONLY | O_CREAT | O_EXCL, 0000)) == -1 && errno == EACCES) { 
		if(++count < 1) {
			sleep(1);
		}	else {
			return(-1);
		}
	}
	return(fd > 0 ? 0 : -1);

	return(0);
}

/* Remove a lock file
 */
int _alpm_lckrm(char *file)
{
	return(unlink(file) == -1);
}

int _alpm_unpack(char *archive, const char *prefix, const char *fn)
{
	TAR *tar = NULL;
	char expath[PATH_MAX];
	tartype_t gztype = {
		(openfunc_t) _alpm_gzopen_frontend,
		(closefunc_t)gzclose,
		(readfunc_t) gzread,
		(writefunc_t)gzwrite
	};

	/* open the .tar.gz package */
	if(tar_open(&tar, archive, &gztype, O_RDONLY, 0, TAR_GNU) == -1) {
		perror(archive);
		return(1);
	}
	while(!th_read(tar)) {
		if(fn && strcmp(fn, th_get_pathname(tar))) {
			if(TH_ISREG(tar) && tar_skip_regfile(tar)) {
				_alpm_log(PM_LOG_ERROR, "bad tar archive: %s", archive);
				tar_close(tar);
				return(1);
			}
			continue;
		}
		snprintf(expath, PATH_MAX, "%s/%s", prefix, th_get_pathname(tar));
		if(tar_extract_file(tar, expath)) {
			_alpm_log(PM_LOG_ERROR, "could not extract %s (%s)", th_get_pathname(tar), strerror(errno));
		}
		if(fn) break;
	}
	tar_close(tar);

	return(0);
}

/* does the same thing as 'rm -rf' */
int _alpm_rmrf(char *path)
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

int _alpm_log_action(unsigned char usesyslog, FILE *f, char *fmt, ...)
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

		fprintf(f, "[%02d/%02d/%02d %02d:%02d] %s\n", tm->tm_mon+1, tm->tm_mday,
		        tm->tm_year-100, tm->tm_hour, tm->tm_min, msg);
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

int _alpm_runscriptlet(char *root, char *installfn, char *script, char *ver, char *oldver)
{
	char scriptfn[PATH_MAX];
	char cmdline[PATH_MAX];
	char tmpdir[PATH_MAX] = "";
	char *scriptpath;
	struct stat buf;
	char cwd[PATH_MAX];

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
			_alpm_log(PM_LOG_ERROR, "could not create temp directory");
			return(1);
		}
		_alpm_unpack(installfn, tmpdir, ".INSTALL");
		snprintf(scriptfn, PATH_MAX, "%s/.INSTALL", tmpdir);
		/* chop off the root so we can find the tmpdir in the chroot */
		scriptpath = scriptfn + strlen(root) - 1;
		return(0);
	} else {
		STRNCPY(scriptfn, installfn, PATH_MAX);
		/* chop off the root so we can find the tmpdir in the chroot */
		scriptpath = scriptfn + strlen(root) - 1;
	}

	if(!_alpm_grep(scriptfn, script)) {
		/* script not found in scriptlet file */
		if(strlen(tmpdir) && _alpm_rmrf(tmpdir)) {
			_alpm_log(PM_LOG_WARNING, "could not remove tmpdir %s", tmpdir);
		}
		return(0);
	}

	/* save the cwd so we can restore it later */
	getcwd(cwd, PATH_MAX);
	/* just in case our cwd was removed in the upgrade operation */
	chdir("/");

	_alpm_log(PM_LOG_FLOW2, "executing %s script...", script);
	if(oldver) {
		snprintf(cmdline, PATH_MAX, "echo \"umask 0022; source %s %s %s %s\" | chroot %s /bin/sh",
				scriptpath, script, ver, oldver, root);
	} else {
		snprintf(cmdline, PATH_MAX, "echo \"umask 0022; source %s %s %s\" | chroot %s /bin/sh",
				scriptpath, script, ver, root);
	}
	_alpm_log(PM_LOG_DEBUG, "%s", cmdline);
	system(cmdline);
	
	if(strlen(tmpdir) && _alpm_rmrf(tmpdir)) {
		_alpm_log(PM_LOG_WARNING, "could not remove tmpdir %s", tmpdir);
	}

	chdir(cwd);
	return(0);
}

/* vim: set ts=2 sw=2 noet: */
