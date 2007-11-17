/*
 *  callback.c
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 *  USA.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <dirent.h>
#include <wchar.h>

#include <alpm.h>

/* pacman */
#include "callback.h"
#include "util.h"
#include "conf.h"

/* TODO this should not have to be defined twice- trans.c & log.c */
#define LOG_STR_LEN 256
#define FILENAME_TRIM_LEN 23

/* download progress bar */
static float rate_last;
static int xfered_last;
static struct timeval initial_time;

/* transaction progress bar ? */
static int prevpercent=0; /* for less progressbar output */

/* Silly little helper function, determines if the caller needs a visual update
 * since the last time this function was called.
 * This is made for the two progress bar functions, to prevent flicker
 *
 * first_call indicates if this is the first time it is called, for
 * initialization purposes */
static float get_update_timediff(int first_call)
{
	float retval = 0.0;
	static struct timeval last_time = {0, 0};

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
static void fill_progress(const int graph_percent, const int display_percent,
		const int proglen)
{
	const unsigned int hashlen = proglen - 8;
	const unsigned int hash = graph_percent * hashlen / 100;
	static unsigned int lasthash = 0, mouth = 0;
	unsigned int i;

	/* printf("\ndebug: proglen: %i\n", proglen); DEBUG*/

	if(graph_percent == 0) {
		lasthash = 0;
		mouth = 0;
	}

	/* magic numbers, how I loathe thee */
	if(proglen > 8) {
		printf(" [");
		for(i = hashlen; i > 1; --i) {
			/* if special progress bar enabled */
			if(config->chomp) {
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
		printf(" %3d%%", display_percent);
	}

	if(graph_percent == 100) {
		printf("\n");
	} else {
		printf("\r");
	}
	fflush(stdout);
}



/* callback to handle messages/notifications from libalpm transactions */
void cb_trans_evt(pmtransevt_t event, void *data1, void *data2)
{
	char str[LOG_STR_LEN] = "";

	switch(event) {
		case PM_TRANS_EVT_CHECKDEPS_START:
		  printf(_("checking dependencies... "));
			break;
		case PM_TRANS_EVT_FILECONFLICTS_START:
			if(config->noprogressbar) {
			printf(_("checking for file conflicts... "));
			}
			break;
		case PM_TRANS_EVT_RESOLVEDEPS_START:
			printf(_("resolving dependencies... "));
			break;
		case PM_TRANS_EVT_INTERCONFLICTS_START:
			printf(_("looking for inter-conflicts... "));
			break;
		case PM_TRANS_EVT_FILECONFLICTS_DONE:
			if(config->noprogressbar) {
				printf(_("done.\n"));
			}
			break;
		case PM_TRANS_EVT_EXTRACT_DONE:
			/* nothing */
			break;
		case PM_TRANS_EVT_ADD_START:
			if(config->noprogressbar) {
				printf(_("installing %s... "), alpm_pkg_get_name(data1));
			}
			break;
		case PM_TRANS_EVT_ADD_DONE:
			if(config->noprogressbar) {
				printf(_("done.\n"));
			}
			snprintf(str, LOG_STR_LEN, "installed %s (%s)\n",
			         alpm_pkg_get_name(data1),
			         alpm_pkg_get_version(data1));
			alpm_logaction(str);
			break;
		case PM_TRANS_EVT_REMOVE_START:
			if(config->noprogressbar) {
			printf(_("removing %s... "), alpm_pkg_get_name(data1));
			}
			break;
		case PM_TRANS_EVT_REMOVE_DONE:
			if(config->noprogressbar) {
			    printf(_("done.\n"));
			}
			snprintf(str, LOG_STR_LEN, "removed %s (%s)\n",
			         alpm_pkg_get_name(data1),
			         alpm_pkg_get_version(data1));
			alpm_logaction(str);
			break;
		case PM_TRANS_EVT_UPGRADE_START:
			if(config->noprogressbar) {
				printf(_("upgrading %s... "), alpm_pkg_get_name(data1));
			}
			break;
		case PM_TRANS_EVT_UPGRADE_DONE:
			if(config->noprogressbar) {
				printf(_("done.\n"));
			}
			snprintf(str, LOG_STR_LEN, "upgraded %s (%s -> %s)\n",
			         (char *)alpm_pkg_get_name(data1),
			         (char *)alpm_pkg_get_version(data2),
			         (char *)alpm_pkg_get_version(data1));
			alpm_logaction(str);
			break;
		case PM_TRANS_EVT_INTEGRITY_START:
			printf(_("checking package integrity... "));
			break;
		case PM_TRANS_EVT_DELTA_INTEGRITY_START:
			printf(_("checking delta integrity... "));
			break;
		case PM_TRANS_EVT_DELTA_PATCHES_START:
			printf(_("applying deltas...\n"));
			break;
		case PM_TRANS_EVT_DELTA_PATCHES_DONE:
			/* nothing */
			break;
		case PM_TRANS_EVT_DELTA_PATCH_START:
			printf(_("generating %s with %s... "), (char *)data1, (char *)data2);
			break;
		case PM_TRANS_EVT_DELTA_PATCH_FAILED:
			printf(_("failed.\n"));
			break;
		case PM_TRANS_EVT_SCRIPTLET_INFO:
			printf("%s", (char*)data1);
			break;
		case PM_TRANS_EVT_PRINTURI:
			printf("%s/%s\n", (char*)data1, (char*)data2);
			break;
		case PM_TRANS_EVT_RETRIEVE_START:
			printf(_(":: Retrieving packages from %s...\n"), (char*)data1);
			break;
		/* all the simple done events, with fallthrough for each */
		case PM_TRANS_EVT_CHECKDEPS_DONE:
		case PM_TRANS_EVT_RESOLVEDEPS_DONE:
		case PM_TRANS_EVT_INTERCONFLICTS_DONE:
		case PM_TRANS_EVT_INTEGRITY_DONE:
		case PM_TRANS_EVT_DELTA_INTEGRITY_DONE:
		case PM_TRANS_EVT_DELTA_PATCH_DONE:
			printf(_("done.\n"));
			break;
	}
	fflush(stdout);
}

/* callback to handle questions from libalpm transactions (yes/no) */
/* TODO this is one of the worst ever functions written. void *data ? wtf */
void cb_trans_conv(pmtransconv_t event, void *data1, void *data2,
                   void *data3, int *response)
{
	char str[LOG_STR_LEN] = "";

	switch(event) {
		case PM_TRANS_CONV_INSTALL_IGNOREPKG:
			if(data2) {
				/* TODO we take this route based on data2 being not null? WTF */
				snprintf(str, LOG_STR_LEN, _(":: %s requires installing %s from IgnorePkg/IgnoreGroup. Install anyway? [Y/n] "),
						alpm_pkg_get_name(data1),
						alpm_pkg_get_name(data2));
				*response = yesno(str);
			} else {
				snprintf(str, LOG_STR_LEN, _(":: %s is in IgnorePkg/IgnoreGroup. Install anyway? [Y/n] "),
						alpm_pkg_get_name(data1));
				*response = yesno(str);
			}
			break;
		case PM_TRANS_CONV_REMOVE_HOLDPKG:
			snprintf(str, LOG_STR_LEN, _(":: %s is designated as a HoldPkg. Remove anyway? [Y/n] "),
					alpm_pkg_get_name(data1));
			*response = yesno(str);
			break;
		case PM_TRANS_CONV_REPLACE_PKG:
			snprintf(str, LOG_STR_LEN, _(":: Replace %s with %s/%s? [Y/n] "),
					alpm_pkg_get_name(data1),
					(char *)data3,
					alpm_pkg_get_name(data2));
			*response = yesno(str);
			break;
		case PM_TRANS_CONV_CONFLICT_PKG:
			snprintf(str, LOG_STR_LEN, _(":: %s conflicts with %s. Remove %s? [Y/n] "),
					(char *)data1,
					(char *)data2,
					(char *)data2);
			*response = yesno(str);
			break;
		case PM_TRANS_CONV_LOCAL_NEWER:

			if(!config->op_s_downloadonly) {
				snprintf(str, LOG_STR_LEN, _(":: %s-%s: local version is newer. Upgrade anyway? [Y/n] "),
						alpm_pkg_get_name(data1),
						alpm_pkg_get_version(data1));
				*response = yesno(str);
			} else {
				*response = 1;
			}
			break;
		case PM_TRANS_CONV_LOCAL_UPTODATE:
			if(!config->op_s_downloadonly) {
				snprintf(str, LOG_STR_LEN, _(":: %s-%s: local version is up to date. Upgrade anyway? [Y/n] "),
						alpm_pkg_get_name(data1),
						alpm_pkg_get_version(data1));
				*response = yesno(str);
			} else {
				*response = 1;
			}
			break;
		case PM_TRANS_CONV_CORRUPTED_PKG:
			if(!config->noconfirm) {
				snprintf(str, LOG_STR_LEN, _(":: File %s is corrupted. Do you want to delete it? [Y/n] "),
						(char *)data1);
				*response = yesno(str);
			} else {
				*response = 1;
			}
			break;
	}
}

/* callback to handle display of transaction progress */
void cb_trans_progress(pmtransprog_t event, const char *pkgname, int percent,
                       int howmany, int remain)
{
	float timediff;

	/* size of line to allocate for text printing (e.g. not progressbar) */
	const int infolen = 50;
	int tmp, digits, oprlen, textlen, pkglen;
	char *opr = NULL;
	wchar_t *wcopr = NULL;

	if(config->noprogressbar) {
		return;
	}

	if(percent == 0) {
		timediff = get_update_timediff(1);
	} else {
		timediff = get_update_timediff(0);
	}

	if(percent > 0 && percent < 100 && !timediff) {
		/* only update the progress bar when
		 * a) we first start
		 * b) we end the progress
		 * c) it has been long enough since the last call
		 */
		return;
	}

	/* if no pkgname, percent is too high or unchanged, then return */
	if(!pkgname || percent == prevpercent) {
		return;
	}

	prevpercent=percent;
	/* set text of message to display */
	switch (event) {
		case PM_TRANS_PROGRESS_ADD_START:
			opr = _("installing");
			break;
		case PM_TRANS_PROGRESS_UPGRADE_START:
			opr = _("upgrading");
			break;
		case PM_TRANS_PROGRESS_REMOVE_START:
			opr = _("removing");
			break;
		case PM_TRANS_PROGRESS_CONFLICTS_START:
			opr = _("checking for file conflicts");
			break;
	}
	/* convert above strings to wide chars */
	oprlen = strlen(opr);
	wcopr = calloc(oprlen, sizeof(wchar_t));
	if(!wcopr) {
		fprintf(stderr, "malloc failure: could not allocate %zd bytes\n",
		        strlen(opr) * sizeof(wchar_t));
		return;
	}
	oprlen = mbstowcs(wcopr, opr, oprlen);

	/* find # of digits in package counts to scale output */
	digits = 1;
	tmp = howmany;
	while((tmp /= 10)) {
		++digits;
	}

	/* determine room left for non-digits text [not ( 1/12) part] */
	textlen = infolen - 3 - (2 * digits);
	/* room left for package name */
	pkglen = textlen - oprlen - 1;

	switch (event) {
		case PM_TRANS_PROGRESS_ADD_START:
		case PM_TRANS_PROGRESS_UPGRADE_START:
		case PM_TRANS_PROGRESS_REMOVE_START:
			/* old way of doing it, but ISO C does not recognize it
			printf("(%2$*1$d/%3$*1$d) %4$s %6$-*5$.*5$s", digits, remain, howmany,
			       opr, pkglen, pkgname);*/
			printf("(%*d/%*d) %s %-*.*s", digits, remain, digits, howmany,
			       opr, pkglen, pkglen, pkgname);
			break;
		case PM_TRANS_PROGRESS_CONFLICTS_START:
			/* old way of doing it, but ISO C does not recognize it
			printf("(%2$*1$d/%3$*1$d) %5$-*4$s", digits, remain, howmany,
			       textlen, opr);*/
			printf("(%*d/%*d) %-*s", digits, remain, digits, howmany,
			       textlen, opr);
			break;
	}

	free(wcopr);

	/* call refactored fill progress function */
	fill_progress(percent, percent, getcols() - infolen);
}

/* callback to handle display of download progress */
void cb_dl_progress(const char *filename, int file_xfered, int file_total,
		int list_xfered, int list_total)
{
	const int infolen = 50;
	char *fname, *p;

	float rate = 0.0, timediff = 0.0, f_xfered = 0.0;
	unsigned int eta_h = 0, eta_m = 0, eta_s = 0;
	int graph_percent = 0, display_percent = 0;
	char rate_size = 'K', xfered_size = 'K';
	int xfered = 0, total = 0;

	/* Need this variable when TotalDownload is set to know if we should
	 * reset xfered_last and rate_last. */
	static int has_init = 0;

	if(config->noprogressbar) {
		return;
	}

	/* Choose how to display the amount downloaded, rate, ETA, and
	 * percentage depending on the TotalDownload option. */
	if (config->totaldownload && list_total > 0) {
		xfered = list_xfered;
		total = list_total;
	} else {
		xfered = file_xfered;
		total = file_total;
	}

	/* this is basically a switch on file_xferred: 0, file_total, and
	 * anything else */
	if(file_xfered == 0) {
		/* set default starting values, but only once for TotalDownload */
		if (!(config->totaldownload && list_total > 0) ||
				(config->totaldownload && list_total > 0 && !has_init)) {
			gettimeofday(&initial_time, NULL);
			timediff = get_update_timediff(1);
			xfered_last = 0;
			rate_last = 0.0;
			has_init = 1;
		}
		rate = 0.0;
		eta_s = 0;
	} else if(file_xfered == file_total) {
		/* compute final values */
		struct timeval current_time;
		float diff_sec, diff_usec;

		gettimeofday(&current_time, NULL);
		diff_sec = current_time.tv_sec - initial_time.tv_sec;
		diff_usec = current_time.tv_usec - initial_time.tv_usec;
		timediff = diff_sec + (diff_usec / 1000000.0);
		rate = xfered / (timediff * 1024.0);

		/* round elapsed time to the nearest second */
		eta_s = (int)(timediff + 0.5);
	} else {
		/* compute current average values */
		timediff = get_update_timediff(0);

		if(timediff < UPDATE_SPEED_SEC) {
			/* return if the calling interval was too short */
			return;
		}
		rate = (xfered - xfered_last) / (timediff * 1024.0);
		/* average rate to reduce jumpiness */
		rate = (rate + 2*rate_last) / 3;
		eta_s = (total - xfered) / (rate * 1024.0);
		rate_last = rate;
		xfered_last = xfered;
	}

	/* fix up time for display */
	eta_h = eta_s / 3600;
	eta_s -= eta_h * 3600;
	eta_m = eta_s / 60;
	eta_s -= eta_m * 60;

	fname = strdup(filename);
	/* strip package or DB extension for cleaner look */
	if((p = strstr(fname, PKGEXT)) || (p = strstr(fname, DBEXT))) {
			*p = '\0';
	}
	if(strlen(fname) > FILENAME_TRIM_LEN) {
		strcpy(fname + FILENAME_TRIM_LEN -3,"...");
	}

	/* Awesome formatting for progress bar.  We need a mess of Kb->Mb->Gb stuff
	 * here. We'll use limit of 2048 for each until we get some empirical */
	/* rate_size = 'K'; was set above */
	if(rate > 2048.0) {
		rate /= 1024.0;
		rate_size = 'M';
		if(rate > 2048.0) {
			rate /= 1024.0;
			rate_size = 'G';
			/* we should not go higher than this for a few years (9999.9 Gb/s?)*/
		}
	}

	f_xfered = xfered / 1024.0; /* convert to K by default */
	/* xfered_size = 'K'; was set above */
	if(f_xfered > 2048.0) {
		f_xfered /= 1024.0;
		xfered_size = 'M';
		if(f_xfered > 2048.0) {
			f_xfered /= 1024.0;
			xfered_size = 'G';
			/* I should seriously hope that archlinux packages never break
			 * the 9999.9GB mark... we'd have more serious problems than the progress
			 * bar in pacman */
		}
	}

	printf(" %-*s %6.1f%c %#6.1f%c/s %02u:%02u:%02u", FILENAME_TRIM_LEN, fname,
				 f_xfered, xfered_size, rate, rate_size, eta_h, eta_m, eta_s);

	free(fname);

	/* The progress bar is based on the file percent regardless of the
	 * TotalDownload option. */
	graph_percent = (int)((float)file_xfered) / ((float)file_total) * 100;
	display_percent = (int)((float)xfered) / ((float)total) * 100;
	fill_progress(graph_percent, display_percent, getcols() - infolen);
	return;
}

/* Callback to handle notifications from the library */
void cb_log(pmloglevel_t level, char *fmt, va_list args)
{
	if(!strlen(fmt)) {
		return;
	}

	pm_vfprintf(stdout, level, fmt, args);
}

/* vim: set ts=2 sw=2 noet: */
