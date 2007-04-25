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

#if defined(__APPLE__) || defined(__OpenBSD__)
#include <sys/syslimits.h>
#include <sys/stat.h>
#endif
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#ifdef CYGWIN
#include <limits.h> /* PATH_MAX */
#endif

#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "util.h"
#include "conf.h"
#include "log.h"

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
			unsigned int len;
			p++;
			if(p == NULL || *p == ' ') continue;
			next = strchr(p, ' ');
			if(next == NULL) {
				next = p + strlen(p);
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

void list_display(const char *title, alpm_list_t *list)
{
	alpm_list_t *i;
	int cols, len;

	len = strlen(title);
	printf("%s ", title);

	if(list) {
		for(i = list, cols = len; i; i = alpm_list_next(i)) {
			char *str = alpm_list_getdata(i);
			int s = strlen(str) + 2;
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
void display_targets(alpm_list_t *syncpkgs)
{
	char *str;
	alpm_list_t *i, *j;
	alpm_list_t *targets = NULL, *to_remove = NULL;
	/* TODO these are some messy variable names */
	unsigned long size = 0, isize = 0, rsize = 0;
	double mbsize = 0.0, mbisize = 0.0, mbrsize = 0.0;

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

		size += alpm_pkg_get_size(pkg);
		isize += alpm_pkg_get_isize(pkg);

		asprintf(&str, "%s-%s", alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
		targets = alpm_list_add(targets, str);
	}

	/* Convert byte sizes to MB */
	mbsize = (double)(size) / (1024.0 * 1024.0);
	mbisize = (double)(isize) / (1024.0 * 1024.0);
	mbrsize = (double)(rsize) / (1024.0 * 1024.0);

	if(to_remove) {
		MSG(NL, "\n"); /* TODO ugly hack. printing a single NL should be easy */
		list_display(_("Remove:"), to_remove);
		FREELIST(to_remove);
	
		if(mbrsize > 0) {
			/* round up if size is really small */
			if(mbrsize < 0.1) {
				mbrsize = 0.1;
			}
			MSG(NL, _("\nTotal Removed Size:   %.2f MB\n"), mbrsize);
		}
	}

	MSG(NL, "\n"); /* TODO ugly hack. printing a single NL should be easy */ 
	list_display(_("Targets:"), targets);

	/* round up if size is really small */
	if(mbsize < 0.1) {
		mbsize = 0.1;
	}
	MSG(NL, _("\nTotal Package Size:   %.2f MB\n"), mbsize);
	
	if(mbisize > mbsize) {
		/*round up if size is really small */
		if(mbisize < 0.1) {
			mbisize = 0.1;
		}
		MSG(NL, _("Total Installed Size:   %.2f MB\n"), mbisize);
	}

	FREELIST(targets);
}

/* Silly little helper function, determines if the caller needs a visual update
 * since the last time this function was called.
 * This is made for the two progress bar functions, to prevent flicker
 *
 * first_call indicates if this is the first time it is called, for
 * initialization purposes */
float get_update_timediff(int first_call)
{
	float retval = 0.0;
	static struct timeval last_time = {0};

	/* on first call, simply set the last time and return */
	if(first_call) {
		gettimeofday(&last_time, NULL);
	} else {
		struct timeval this_time;
		float diff_sec, diff_usec;

		gettimeofday(&this_time, NULL);
		diff_sec = this_time.tv_sec - last_time.tv_sec;
		diff_usec = this_time.tv_usec - last_time.tv_usec;

		retval = diff_sec + (diff_usec / 1000000.0);

		/* return 0 and do not update last_time if interval was too short */
		if(retval < UPDATE_SPEED_SEC) {
			retval = 0.0;
		} else {
			last_time = this_time;
			/* printf("\nupdate retval: %f\n", retval); DEBUG*/
		}
	}
	
	return(retval);
}

/* refactored from cb_trans_progress */
void fill_progress(const int percent, const int proglen)
{
	const unsigned short chomp = alpm_option_get_chomp();
	const unsigned int hashlen = proglen - 8;
	const unsigned int hash = percent * hashlen / 100;
	static unsigned int lasthash = 0, mouth = 0;
	unsigned int i;

	/* printf("\ndebug: proglen: %i\n", proglen); DEBUG*/

	if(percent == 0) {
		lasthash = 0;
		mouth = 0;
	}

	/* magic numbers, how I loathe thee */
	if(proglen > 8) {
		printf(" [");
		for(i = hashlen; i > 1; --i) {
			/* if special progress bar enabled */
			if(chomp) {
				if(i > hashlen - hash) {
					printf("-");
				} else if(i == hashlen - hash) {
					if(lasthash == hash) {
						if(mouth) {
							printf("\033[1;33mC\033[m");
						} else {
							printf("\033[1;33mc\033[m");
						}
					} else {
						lasthash = hash;
						mouth = mouth == 1 ? 0 : 1;
						if(mouth) {
							printf("\033[1;33mC\033[m");
						} else {
							printf("\033[1;33mc\033[m");
						}
					}
				} else if(i%3 == 0) {
					printf("\033[0;37mo\033[m");
				} else {
					printf("\033[0;37m \033[m");
				}
			} /* else regular progress bar */
			else if(i > hashlen - hash) {
				printf("#");
			} else {
				printf("-");
			}
		}
		printf("]");
	}
	/* print percent after progress bar */
	if(proglen > 5) {
		printf(" %3d%%", percent);
	}

	if(percent == 100) {
		printf("\n");
	} else {
		printf("\r");
	}
	fflush(stdout);
}

/* vim: set ts=2 sw=2 noet: */
