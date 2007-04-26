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
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <wchar.h>
#include <math.h>

#include <alpm.h>

/* pacman */
#include "callback.h"
#include "util.h"
#include "output.h"
#include "conf.h"

/* TODO this should not have to be defined twice- trans.c & log.c */
#define LOG_STR_LEN 256
#define FILENAME_TRIM_LEN 23

extern config_t *config;

/* download progress bar */
static float rate_last;
static int xfered_last;
static struct timeval initial_time;

/* transaction progress bar ? */
static int prevpercent=0; /* for less progressbar output */

/* refactored from cb_trans_progress */
static void fill_progress(const int percent, const int proglen)
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
		case PM_TRANS_EVT_CLEANUP_START:
			printf(_("cleaning up... "));
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
		case PM_TRANS_EVT_CHECKDEPS_DONE:
		case PM_TRANS_EVT_CLEANUP_DONE:
		case PM_TRANS_EVT_RESOLVEDEPS_DONE:
		case PM_TRANS_EVT_INTERCONFLICTS_DONE:
			printf(_("done.\n"));
			break;
		case PM_TRANS_EVT_EXTRACT_DONE:
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
			snprintf(str, LOG_STR_LEN, _("installed %s (%s)"),
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
			snprintf(str, LOG_STR_LEN, _("removed %s (%s)"),
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
			snprintf(str, LOG_STR_LEN, _("upgraded %s (%s -> %s)"),
			         (char *)alpm_pkg_get_name(data1),
			         (char *)alpm_pkg_get_version(data2),
			         (char *)alpm_pkg_get_version(data1));
			alpm_logaction(str);
			break;
		case PM_TRANS_EVT_INTEGRITY_START:
			printf(_("checking package integrity... "));
			break;
		case PM_TRANS_EVT_INTEGRITY_DONE:
			printf(_("done.\n"));
			break;
		case PM_TRANS_EVT_SCRIPTLET_INFO:
			printf("%s\n", (char*)data1);
			break;
		case PM_TRANS_EVT_SCRIPTLET_START:
			printf((char*)data1);
			printf("...");
			break;
		case PM_TRANS_EVT_SCRIPTLET_DONE:
			if(!(long)data1) {
				printf(_("done.\n"));
			} else {
				printf(_("failed.\n"));
			}
			break;
		case PM_TRANS_EVT_PRINTURI:
			printf("%s/%s\n", (char*)data1, (char*)data2);
			break;
		case PM_TRANS_EVT_RETRIEVE_START:
			printf(_(":: Retrieving packages from %s...\n"), (char*)data1);
			fflush(stdout);
			break;
	}
}

/* callback to handle questions from libalpm transactions (yes/no) */
/* TODO this is one of the worst ever functions written. void *data ? wtf */
void cb_trans_conv(pmtransconv_t event, void *data1, void *data2,
                   void *data3, int *response)
{
	char str[LOG_STR_LEN] = "";

	switch(event) {
		case PM_TRANS_CONV_INSTALL_IGNOREPKG:
			if(config->noask) {
				if(config->ask & PM_TRANS_CONV_INSTALL_IGNOREPKG) {
					*response = 1;
				} else {
					*response = 0;
				}
			} else if(data2) {
				/* TODO we take this route based on data2 being not null? WTF */
				snprintf(str, LOG_STR_LEN, _(":: %s requires installing %s from IgnorePkg. Install anyway? [Y/n] "),
				         alpm_pkg_get_name(data1),
				         alpm_pkg_get_name(data2));
				*response = yesno(str);
			} else {
				snprintf(str, LOG_STR_LEN, _(":: %s is in IgnorePkg. Install anyway? [Y/n] "),
				         alpm_pkg_get_name(data1));
				*response = yesno(str);
			}
			break;
		case PM_TRANS_CONV_REMOVE_HOLDPKG:
			if(config->noask) {
				if(config->ask & PM_TRANS_CONV_REMOVE_HOLDPKG) {
					*response = 1;
				} else {
					*response = 0;
				}
			} else {
				snprintf(str, LOG_STR_LEN, _(":: %s is designated as a HoldPkg. Remove anyway? [Y/n] "),
				         alpm_pkg_get_name(data1));
				*response = yesno(str);
			}
			break;
		case PM_TRANS_CONV_REPLACE_PKG:
			if(config->noask) {
				if(config->ask & PM_TRANS_CONV_REPLACE_PKG) {
					*response = 1;
				} else {
					*response = 0;
				}
			} else {
				snprintf(str, LOG_STR_LEN, _(":: Replace %s with %s/%s? [Y/n] "),
				         alpm_pkg_get_name(data1),
				         (char *)data3,
				         alpm_pkg_get_name(data2));
				*response = yesno(str);
			}
			break;
		case PM_TRANS_CONV_CONFLICT_PKG:
			if(config->noask) {
				if(config->ask & PM_TRANS_CONV_CONFLICT_PKG) {
					*response = 1;
				} else {
					*response = 0;
				}
			} else {
				snprintf(str, LOG_STR_LEN, _(":: %s conflicts with %s. Remove %s? [Y/n] "),
				         (char *)data1,
				         (char *)data2,
				         (char *)data2);
				*response = yesno(str);
			}
			break;
		case PM_TRANS_CONV_LOCAL_NEWER:
			if(config->noask) {
				if(config->ask & PM_TRANS_CONV_LOCAL_NEWER) {
					*response = 1;
				} else {
					*response = 0;
				}
			} else {
				if(!config->op_s_downloadonly) {
					snprintf(str, LOG_STR_LEN, _(":: %s-%s: local version is newer. Upgrade anyway? [Y/n] "),
				         alpm_pkg_get_name(data1),
				         alpm_pkg_get_version(data1));
					*response = yesno(str);
				} else {
					*response = 1;
				}
			}
			break;
		case PM_TRANS_CONV_LOCAL_UPTODATE:
			if(config->noask) {
				if(config->ask & PM_TRANS_CONV_LOCAL_UPTODATE) {
					*response = 1;
				} else {
					*response = 0;
				}
			} else {
				if(!config->op_s_downloadonly) {
					snprintf(str, LOG_STR_LEN, _(":: %s-%s: local version is up to date. Upgrade anyway? [Y/n] "),
				         alpm_pkg_get_name(data1),
				         alpm_pkg_get_version(data1));
					*response = yesno(str);
				} else {
					*response = 1;
				}
			}
			break;
		case PM_TRANS_CONV_CORRUPTED_PKG:
			if(config->noask) {
				if(config->ask & PM_TRANS_CONV_CORRUPTED_PKG) {
					*response = 1;
				} else {
					*response = 0;
				}
			} else {
				if(!config->noconfirm) {
					snprintf(str, LOG_STR_LEN, _(":: Archive %s is corrupted. Do you want to delete it? [Y/n] "),
				         (char *)data1);
					*response = yesno(str);
				} else {
					*response = 1;
				}
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
		/* print a newline before we start our progressbar */
		printf("\n");
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
	wcopr = (wchar_t*)calloc(oprlen, sizeof(wchar_t));
	if(!wcopr) {
		fprintf(stderr, "malloc failure: could not allocate %d bytes\n",
		        strlen(opr) * sizeof(wchar_t));
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
	fill_progress(percent, getcols() - infolen);
}

/* callback to handle display of download progress */
void cb_dl_progress(const char *filename, int xfered, int total)
{
	const int infolen = 50;
	char *fname, *p; 

	float rate = 0.0, timediff = 0.0, f_xfered = 0.0;
	unsigned int eta_h = 0, eta_m = 0, eta_s = 0;
	int percent;
	char rate_size = 'K', xfered_size = 'K';

	if(config->noprogressbar) {
		return;
	}

	/* this is basically a switch on xferred: 0, total, and anything else */
	if(xfered == 0) {
		/* print a newline before we start our progressbar */
		printf("\n");
		/* set default starting values */
		gettimeofday(&initial_time, NULL);
		xfered_last = 0;
		rate_last = 0.0;
		timediff = get_update_timediff(1);
		rate = 0.0;
		eta_s = 0;
	} else if(xfered == total) {
		/* compute final values */
		struct timeval current_time;
		float diff_sec, diff_usec;
		
		gettimeofday(&current_time, NULL);
		diff_sec = current_time.tv_sec - initial_time.tv_sec;
		diff_usec = current_time.tv_usec - initial_time.tv_usec;
		timediff = diff_sec + (diff_usec / 1000000.0);
		rate = (float)total / (timediff * 1024.0);

		/* round elapsed time to the nearest second */
		eta_s = (int)floorf(timediff + 0.5);
	} else {
		/* compute current average values */
		timediff = get_update_timediff(0);

		if(timediff < UPDATE_SPEED_SEC) {
			/* return if the calling interval was too short */
			return;
		}
		rate = (float)(xfered - xfered_last) / (timediff * 1024.0);
		/* average rate to reduce jumpiness */
		rate = (float)(rate + 2*rate_last) / 3;
		eta_s = (unsigned int)(total - xfered) / (rate * 1024.0);
		rate_last = rate;
		xfered_last = xfered;
	}

	percent = (int)((float)xfered) / ((float)total) * 100;

	/* fix up time for display */
	eta_h = eta_s / 3600;
	eta_s -= eta_h * 3600;
	eta_m = eta_s / 60;
	eta_s -= eta_m * 60;

	fname = strdup(filename);
	/* strip extension if it's there
	 * NOTE: in the case of package files, only the pkgname is sent now */
	if((p = strstr(fname, PM_EXT_PKG)) || (p = strstr(fname, PM_EXT_DB))) {
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

	f_xfered = (float) xfered / 1024.0; /* convert to K by default */
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
	
	fill_progress(percent, getcols() - infolen);
	return;
}

/* Callback to handle notifications from the library */
void cb_log(unsigned short level, char *msg)
{
	char str[LOG_STR_LEN] = "";

	if(!strlen(msg)) {
		return;
	}

	switch(level) {
		case PM_LOG_DEBUG:
			sprintf(str, _("debug"));
		break;
		case PM_LOG_ERROR:
			sprintf(str, _("error"));
		break;
		case PM_LOG_WARNING:
			sprintf(str, _("warning"));
		break;
		case PM_LOG_FUNCTION:
		  /* TODO we should increase the indent level when this occurs so we can see
			 * program flow easier.  It'll be fun
			 */
			sprintf(str, _("function"));
		break;
		default:
			sprintf(str, "???");
		break;
	}

#ifdef PACMAN_DEBUG
	/* If debug is on, we'll timestamp the output */
  if(alpm_option_get_logmask() & PM_LOG_DEBUG) {
		time_t t;
		struct tm *tmp;
		char timestr[10] = {0};

		t = time(NULL);
		tmp = localtime(&t);
		strftime(timestr, 9, "%H:%M:%S", tmp);
		timestr[8] = '\0';

		printf("[%s] %s: %s", timestr, str, msg);
	} else {
    printf("%s: %s", str, msg);
	}
#else
	printf("%s: %s", str, msg);
#endif
}

/* vim: set ts=2 sw=2 noet: */
