/*
 *  util.c
 *
 *  Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <time.h>
#include <syslog.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <locale.h> /* setlocale */

/* libarchive */
#include <archive.h>
#include <archive_entry.h>

#ifdef HAVE_LIBSSL
#include <openssl/md5.h>
#else
#include "md5.h"
#endif

/* libalpm */
#include "util.h"
#include "log.h"
#include "package.h"
#include "alpm.h"
#include "alpm_list.h"
#include "handle.h"

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
		size_t nwritten = 0;
		nwritten = fwrite(buf, 1, len, out);
		if((nwritten != len) || ferror(out)) {
			pm_errno = PM_ERR_WRITE;
			_alpm_log(PM_LOG_ERROR, _("error writing to file '%s': %s\n"),
					dest, strerror(errno));
			ret = -1;
			goto cleanup;
		}
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

	while(isspace((unsigned char)*pch)) {
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
	while(isspace((unsigned char)*pch)) {
		pch--;
	}
	*++pch = '\0';

	return(str);
}

/* Compression functions */

/**
 * @brief Unpack a specific file in an archive.
 *
 * @param archive  the archive to unpack
 * @param prefix   where to extract the files
 * @param fn       a file within the archive to unpack
 * @return 0 on success, 1 on failure
 */
int _alpm_unpack_single(const char *archive, const char *prefix, const char *fn)
{
	alpm_list_t *list = NULL;
	int ret = 0;
	if(fn == NULL) {
		return(1);
	}
	list = alpm_list_add(list, (void *)fn);
	ret = _alpm_unpack(archive, prefix, list, 1);
	alpm_list_free(list);
	return(ret);
}

/**
 * @brief Unpack a list of files in an archive.
 *
 * @param archive  the archive to unpack
 * @param prefix   where to extract the files
 * @param list     a list of files within the archive to unpack or
 * NULL for all
 * @param breakfirst break after the first entry found
 *
 * @return 0 on success, 1 on failure
 */
int _alpm_unpack(const char *archive, const char *prefix, alpm_list_t *list, int breakfirst)
{
	int ret = 0;
	mode_t oldmask;
	struct archive *_archive;
	struct archive_entry *entry;
	char cwd[PATH_MAX];
	int restore_cwd = 0;

	ALPM_LOG_FUNC;

	if((_archive = archive_read_new()) == NULL)
		RET_ERR(PM_ERR_LIBARCHIVE, 1);

	archive_read_support_compression_all(_archive);
	archive_read_support_format_all(_archive);

	if(archive_read_open_filename(_archive, archive,
				ARCHIVE_DEFAULT_BYTES_PER_BLOCK) != ARCHIVE_OK) {
		_alpm_log(PM_LOG_ERROR, _("could not open file %s: %s\n"), archive,
				archive_error_string(_archive));
		RET_ERR(PM_ERR_PKG_OPEN, 1);
	}

	oldmask = umask(0022);

	/* save the cwd so we can restore it later */
	if(getcwd(cwd, PATH_MAX) == NULL) {
		_alpm_log(PM_LOG_ERROR, _("could not get current working directory\n"));
	} else {
		restore_cwd = 1;
	}

	/* just in case our cwd was removed in the upgrade operation */
	if(chdir(prefix) != 0) {
		_alpm_log(PM_LOG_ERROR, _("could not change directory to %s (%s)\n"), prefix, strerror(errno));
		ret = 1;
		goto cleanup;
	}

	while(archive_read_next_header(_archive, &entry) == ARCHIVE_OK) {
		const struct stat *st;
		const char *entryname; /* the name of the file in the archive */

		st = archive_entry_stat(entry);
		entryname = archive_entry_pathname(entry);

		if(S_ISREG(st->st_mode)) {
			archive_entry_set_perm(entry, 0644);
		} else if(S_ISDIR(st->st_mode)) {
			archive_entry_set_perm(entry, 0755);
		}

		/* If specific files were requested, skip entries that don't match. */
		if(list) {
			char *prefix = strdup(entryname);
			char *p = strstr(prefix,"/");
			if(p) {
				*(p+1) = '\0';
			}
			char *found = alpm_list_find_str(list, prefix);
			free(prefix);
			if(!found) {
				if (archive_read_data_skip(_archive) != ARCHIVE_OK) {
					ret = 1;
					goto cleanup;
				}
				continue;
			} else {
				_alpm_log(PM_LOG_DEBUG, "extracting: %s\n", entryname);
			}
		}

		/* Extract the archive entry. */
		int readret = archive_read_extract(_archive, entry, 0);
		if(readret == ARCHIVE_WARN) {
			/* operation succeeded but a non-critical error was encountered */
			_alpm_log(PM_LOG_WARNING, _("warning given when extracting %s (%s)\n"),
					entryname, archive_error_string(_archive));
		} else if(readret != ARCHIVE_OK) {
			_alpm_log(PM_LOG_ERROR, _("could not extract %s (%s)\n"),
					entryname, archive_error_string(_archive));
			ret = 1;
			goto cleanup;
		}

		if(breakfirst) {
			break;
		}
	}

cleanup:
	umask(oldmask);
	archive_read_finish(_archive);
	if(restore_cwd && chdir(cwd) != 0) {
		_alpm_log(PM_LOG_ERROR, _("could not change directory to %s (%s)\n"), cwd, strerror(errno));
	}
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
			dirp = opendir(path);
			if(!dirp) {
				return(1);
			}
			for(dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
				if(dp->d_ino) {
					sprintf(name, "%s/%s", path, dp->d_name);
					if(strcmp(dp->d_name, "..") != 0 && strcmp(dp->d_name, ".") != 0) {
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

int _alpm_logaction(int usesyslog, FILE *f, const char *fmt, va_list args)
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

int _alpm_run_chroot(const char *root, const char *path, char *const argv[])
{
	char cwd[PATH_MAX];
	pid_t pid;
	int pipefd[2];
	int restore_cwd = 0;
	int retval = 0;

	ALPM_LOG_FUNC;

	/* save the cwd so we can restore it later */
	if(getcwd(cwd, PATH_MAX) == NULL) {
		_alpm_log(PM_LOG_ERROR, _("could not get current working directory\n"));
	} else {
		restore_cwd = 1;
	}

	/* just in case our cwd was removed in the upgrade operation */
	if(chdir(root) != 0) {
		_alpm_log(PM_LOG_ERROR, _("could not change directory to %s (%s)\n"), root, strerror(errno));
		goto cleanup;
	}

	_alpm_log(PM_LOG_DEBUG, "executing \"%s\" under chroot \"%s\"\n", path, root);

	/* Flush open fds before fork() to avoid cloning buffers */
	fflush(NULL);

	if(pipe(pipefd) == -1) {
		_alpm_log(PM_LOG_ERROR, _("could not create pipe (%s)\n"), strerror(errno));
		retval = 1;
		goto cleanup;
	}

	/* fork- parent and child each have seperate code blocks below */
	pid = fork();
	if(pid == -1) {
		_alpm_log(PM_LOG_ERROR, _("could not fork a new process (%s)\n"), strerror(errno));
		retval = 1;
		goto cleanup;
	}

	if(pid == 0) {
		/* this code runs for the child only (the actual chroot/exec) */
		close(1);
		close(2);
		while(dup2(pipefd[1], 1) == -1 && errno == EINTR);
		while(dup2(pipefd[1], 2) == -1 && errno == EINTR);
		close(pipefd[0]);
		close(pipefd[1]);

		/* use fprintf instead of _alpm_log to send output through the parent */
		if(chroot(root) != 0) {
			fprintf(stderr, _("could not change the root directory (%s)\n"), strerror(errno));
			exit(1);
		}
		if(chdir("/") != 0) {
			fprintf(stderr, _("could not change directory to %s (%s)\n"),
					"/", strerror(errno));
			exit(1);
		}
		umask(0022);
		execv(path, argv);
		fprintf(stderr, _("call to execv failed (%s)\n"), strerror(errno));
		exit(1);
	} else {
		/* this code runs for the parent only (wait on the child) */
		int status;
		FILE *pipe;

		close(pipefd[1]);
		pipe = fdopen(pipefd[0], "r");
		if(pipe == NULL) {
			close(pipefd[0]);
			retval = 1;
		} else {
			while(!feof(pipe)) {
				char line[PATH_MAX];
				if(fgets(line, PATH_MAX, pipe) == NULL)
					break;
				alpm_logaction("%s", line);
				EVENT(handle->trans, PM_TRANS_EVT_SCRIPTLET_INFO, line, NULL);
			}
			fclose(pipe);
		}

		while(waitpid(pid, &status, 0) == -1) {
			if(errno != EINTR) {
				_alpm_log(PM_LOG_ERROR, _("call to waitpid failed (%s)\n"), strerror(errno));
				retval = 1;
				goto cleanup;
			}
		}

		/* report error from above after the child has exited */
		if(retval != 0) {
			_alpm_log(PM_LOG_ERROR, _("could not open pipe (%s)\n"), strerror(errno));
			goto cleanup;
		}
		/* check the return status, make sure it is 0 (success) */
		if(WIFEXITED(status)) {
			_alpm_log(PM_LOG_DEBUG, "call to waitpid succeeded\n");
			if(WEXITSTATUS(status) != 0) {
				_alpm_log(PM_LOG_ERROR, _("command failed to execute correctly\n"));
				retval = 1;
			}
		}
	}

cleanup:
	if(restore_cwd && chdir(cwd) != 0) {
		_alpm_log(PM_LOG_ERROR, _("could not change directory to %s (%s)\n"), cwd, strerror(errno));
	}

	return(retval);
}

int _alpm_ldconfig(const char *root)
{
	char line[PATH_MAX];

	_alpm_log(PM_LOG_DEBUG, "running ldconfig\n");

	snprintf(line, PATH_MAX, "%setc/ld.so.conf", root);
	if(access(line, F_OK) == 0) {
		snprintf(line, PATH_MAX, "%ssbin/ldconfig", root);
		if(access(line, X_OK) == 0) {
			char *argv[] = { "ldconfig", NULL };
			_alpm_run_chroot(root, "/sbin/ldconfig", argv);
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
	struct stat buf;

	/* Loop through the cache dirs until we find a matching file */
	for(i = alpm_option_get_cachedirs(); i; i = alpm_list_next(i)) {
		snprintf(path, PATH_MAX, "%s%s", (char*)alpm_list_getdata(i),
				filename);
		if(stat(path, &buf) == 0 && S_ISREG(buf.st_mode)) {
			retpath = strdup(path);
			_alpm_log(PM_LOG_DEBUG, "found cached pkg: %s\n", retpath);
			return(retpath);
		}
	}
	/* package wasn't found in any cachedir */
	RET_ERR(PM_ERR_PKG_NOT_FOUND, NULL);
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
	size_t len = strlen(newpath);

	/* strip the trailing slash if one exists */
	if(len != 0 && newpath[len - 1] == '/') {
			newpath[len - 1] = '\0';
	}

	ret = lstat(newpath, buf);

	FREE(newpath);
	return(ret);
}

#ifdef HAVE_LIBSSL
static int md5_file(const char *path, unsigned char output[16])
{
	FILE *f;
	size_t n;
	MD5_CTX ctx;
	unsigned char *buf;

	CALLOC(buf, 8192, sizeof(unsigned char), return(1));

	if((f = fopen(path, "rb")) == NULL) {
		free(buf);
		return(1);
	}

	MD5_Init(&ctx);

	while((n = fread(buf, 1, sizeof(buf), f)) > 0) {
		MD5_Update(&ctx, buf, n);
	}

	MD5_Final(output, &ctx);

	memset(&ctx, 0, sizeof(MD5_CTX));
	free(buf);

	if(ferror(f) != 0) {
		fclose(f);
		return(2);
	}

	fclose(f);
	return(0);
}
#endif

/** Get the md5 sum of file.
 * @param filename name of the file
 * @return the checksum on success, NULL on error
 * @addtogroup alpm_misc
 */
char SYMEXPORT *alpm_compute_md5sum(const char *filename)
{
	unsigned char output[16];
	char *md5sum;
	int ret, i;

	ALPM_LOG_FUNC;

	ASSERT(filename != NULL, return(NULL));

	/* allocate 32 chars plus 1 for null */
	md5sum = calloc(33, sizeof(char));
	/* defined above for OpenSSL, otherwise defined in md5.h */
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

	md5sum2 = alpm_compute_md5sum(filepath);

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

/* Note: does NOT handle sparse files on purpose for speed. */
int _alpm_archive_fgets(struct archive *a, struct archive_read_buffer *b)
{
	char *i = NULL;
	int64_t offset;
	int done = 0;

	while(1) {
		/* have we processed this entire block? */
		if(b->block + b->block_size == b->block_offset) {
			if(b->ret == ARCHIVE_EOF) {
				/* reached end of archive on the last read, now we are out of data */
				goto cleanup;
			}

			/* zero-copy - this is the entire next block of data. */
			b->ret = archive_read_data_block(a, (void*)&b->block,
					&b->block_size, &offset);
			b->block_offset = b->block;

			/* error or end of archive with no data read, cleanup */
			if(b->ret < ARCHIVE_OK ||
					(b->block_size == 0 && b->ret == ARCHIVE_EOF)) {
				goto cleanup;
			}
		}

		/* loop through the block looking for EOL characters */
		for(i = b->block_offset; i < (b->block + b->block_size); i++) {
			/* check if read value was null or newline */
			if(*i == '\0' || *i == '\n') {
				done = 1;
				break;
			}
		}

		/* allocate our buffer, or ensure our existing one is big enough */
		if(!b->line) {
			/* set the initial buffer to the read block_size */
			CALLOC(b->line, b->block_size + 1, sizeof(char),
					RET_ERR(PM_ERR_MEMORY, -1));
			b->line_size = b->block_size + 1;
			b->line_offset = b->line;
		} else {
			size_t needed = (size_t)((b->line_offset - b->line)
					+ (i - b->block_offset) + 1);
			if(needed > b->max_line_size) {
				RET_ERR(PM_ERR_MEMORY, -1);
			}
			if(needed > b->line_size) {
				/* need to realloc + copy data to fit total length */
				char *new;
				CALLOC(new, needed, sizeof(char), RET_ERR(PM_ERR_MEMORY, -1));
				memcpy(new, b->line, b->line_size);
				b->line_size = needed;
				b->line_offset = new + (b->line_offset - b->line);
				free(b->line);
				b->line = new;
			}
		}

		if(done) {
			size_t len = (size_t)(i - b->block_offset);
			memcpy(b->line_offset, b->block_offset, len);
			b->line_offset[len] = '\0';
			b->block_offset = ++i;
			/* this is the main return point; from here you can read b->line */
			return(ARCHIVE_OK);
		} else {
			/* we've looked through the whole block but no newline, copy it */
			size_t len = (size_t)(b->block + b->block_size - b->block_offset);
			memcpy(b->line_offset, b->block_offset, len);
			b->line_offset += len;
			b->block_offset = i;
		}
	}

cleanup:
	{
		int ret = b->ret;
		FREE(b->line);
		memset(b, 0, sizeof(b));
		return(ret);
	}
}

int _alpm_splitname(const char *target, pmpkg_t *pkg)
{
	/* the format of a db entry is as follows:
	 *    package-version-rel/
	 * package name can contain hyphens, so parse from the back- go back
	 * two hyphens and we have split the version from the name.
	 */
	const char *version, *end;

	if(target == NULL || pkg == NULL) {
		return(-1);
	}
	end = target + strlen(target);

	/* remove any trailing '/' */
	while (*(end - 1) == '/') {
	  --end;
	}

	/* do the magic parsing- find the beginning of the version string
	 * by doing two iterations of same loop to lop off two hyphens */
	for(version = end - 1; *version && *version != '-'; version--);
	for(version = version - 1; *version && *version != '-'; version--);
	if(*version != '-' || version == target) {
		return(-1);
	}

	/* copy into fields and return */
	if(pkg->version) {
		FREE(pkg->version);
	}
	/* version actually points to the dash, so need to increment 1 and account
	 * for potential end character */
	STRNDUP(pkg->version, version + 1, end - version - 1,
			RET_ERR(PM_ERR_MEMORY, -1));

	if(pkg->name) {
		FREE(pkg->name);
	}
	STRNDUP(pkg->name, target, version - target, RET_ERR(PM_ERR_MEMORY, -1));
	pkg->name_hash = _alpm_hash_sdbm(pkg->name);

	return(0);
}

/**
 * Hash the given string to an unsigned long value.
 * This is the standard sdbm hashing algorithm.
 * @param str string to hash
 * @return the hash value of the given string
 */
unsigned long _alpm_hash_sdbm(const char *str)
{
	unsigned long hash = 0;
	int c;

	if(!str) {
		return(hash);
	}
	while((c = *str++)) {
		hash = c + (hash << 6) + (hash << 16) - hash;
	}

	return(hash);
}

long _alpm_parsedate(const char *line)
{
	if(isalpha((unsigned char)line[0])) {
		/* initialize to null in case of failure */
		struct tm tmp_tm = { 0 };
		setlocale(LC_TIME, "C");
		strptime(line, "%a %b %e %H:%M:%S %Y", &tmp_tm);
		setlocale(LC_TIME, "");
		return(mktime(&tmp_tm));
	}
	return(atol(line));
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
