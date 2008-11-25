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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h> /* off_t */
#include <unistd.h>
#include <dirent.h>
#include <wchar.h>

#include <alpm.h>

/* pacman */
#include "callback.h"
#include "util.h"
#include "conf.h"

/* download progress bar */
static float rate_last;
static off_t xfered_last;
static off_t list_xfered = 0.0;
static off_t list_total = 0.0;
static struct timeval initial_time;

/* transaction progress bar */
static int prevpercent = 0; /* for less progressbar output */

/* delayed output during progress bar */
static int on_progress = 0;
static alpm_list_t *output = NULL;

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
static void fill_progress(const int bar_percent, const int disp_percent,
		const int proglen)
{
	const unsigned int hashlen = proglen - 8;
	const unsigned int hash = bar_percent * hashlen / 100;
	static unsigned int lasthash = 0, mouth = 0;
	unsigned int i;

	if(bar_percent == 0) {
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
	/* print display percent after progress bar */
	if(proglen > 5) {
		printf(" %3d%%", disp_percent);
	}

	if(bar_percent == 100) {
		printf("\n");
	} else {
		printf("\r");
	}
	fflush(stdout);
}



/* callback to handle messages/notifications from libalpm transactions */
void cb_trans_evt(pmtransevt_t event, void *data1, void *data2)
{
	switch(event) {
		case PM_TRANS_EVT_CHECKDEPS_START:
		  printf(_("checking dependencies...\n"));
			break;
		case PM_TRANS_EVT_FILECONFLICTS_START:
			if(config->noprogressbar) {
			printf(_("checking for file conflicts...\n"));
			}
			break;
		case PM_TRANS_EVT_RESOLVEDEPS_START:
			printf(_("resolving dependencies...\n"));
			break;
		case PM_TRANS_EVT_INTERCONFLICTS_START:
			printf(_("looking for inter-conflicts...\n"));
			break;
		case PM_TRANS_EVT_ADD_START:
			if(config->noprogressbar) {
				printf(_("installing %s...\n"), alpm_pkg_get_name(data1));
			}
			break;
		case PM_TRANS_EVT_ADD_DONE:
			alpm_logaction("installed %s (%s)\n",
			         alpm_pkg_get_name(data1),
			         alpm_pkg_get_version(data1));
			display_optdepends(data1);
			break;
		case PM_TRANS_EVT_REMOVE_START:
			if(config->noprogressbar) {
			printf(_("removing %s...\n"), alpm_pkg_get_name(data1));
			}
			break;
		case PM_TRANS_EVT_REMOVE_DONE:
			alpm_logaction("removed %s (%s)\n",
			         alpm_pkg_get_name(data1),
			         alpm_pkg_get_version(data1));
			break;
		case PM_TRANS_EVT_UPGRADE_START:
			if(config->noprogressbar) {
				printf(_("upgrading %s...\n"), alpm_pkg_get_name(data1));
			}
			break;
		case PM_TRANS_EVT_UPGRADE_DONE:
			alpm_logaction("upgraded %s (%s -> %s)\n",
			         (char *)alpm_pkg_get_name(data1),
			         (char *)alpm_pkg_get_version(data2),
			         (char *)alpm_pkg_get_version(data1));
			display_optdepends(data1);
			break;
		case PM_TRANS_EVT_INTEGRITY_START:
			printf(_("checking package integrity...\n"));
			break;
		case PM_TRANS_EVT_DELTA_INTEGRITY_START:
			printf(_("checking delta integrity...\n"));
			break;
		case PM_TRANS_EVT_DELTA_PATCHES_START:
			printf(_("applying deltas...\n"));
			break;
		case PM_TRANS_EVT_DELTA_PATCH_START:
			printf(_("generating %s with %s... "), (char *)data1, (char *)data2);
			break;
		case PM_TRANS_EVT_DELTA_PATCH_DONE:
			printf(_("success!\n"));
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
		case PM_TRANS_EVT_FILECONFLICTS_DONE:
		case PM_TRANS_EVT_CHECKDEPS_DONE:
		case PM_TRANS_EVT_RESOLVEDEPS_DONE:
		case PM_TRANS_EVT_INTERCONFLICTS_DONE:
		case PM_TRANS_EVT_INTEGRITY_DONE:
		case PM_TRANS_EVT_DELTA_INTEGRITY_DONE:
		case PM_TRANS_EVT_DELTA_PATCHES_DONE:
			/* nothing */
			break;
	}
	fflush(stdout);
}

/* callback to handle questions from libalpm transactions (yes/no) */
/* TODO this is one of the worst ever functions written. void *data ? wtf */
void cb_trans_conv(pmtransconv_t event, void *data1, void *data2,
                   void *data3, int *response)
{
	switch(event) {
		case PM_TRANS_CONV_INSTALL_IGNOREPKG:
			if(data2) {
				/* TODO we take this route based on data2 being not null? WTF */
				*response = yesno(1, _(":: %s requires installing %s from IgnorePkg/IgnoreGroup. Install anyway?"),
						alpm_pkg_get_name(data2),
						alpm_pkg_get_name(data1));
			} else {
				*response = yesno(1, _(":: %s is in IgnorePkg/IgnoreGroup. Install anyway?"),
						alpm_pkg_get_name(data1));
			}
			break;
		case PM_TRANS_CONV_REMOVE_HOLDPKG:
			*response = yesno(1, _(":: %s is designated as a HoldPkg. Remove anyway?"),
					alpm_pkg_get_name(data1));
			break;
		case PM_TRANS_CONV_REPLACE_PKG:
			*response = yesno(1, _(":: Replace %s with %s/%s?"),
					alpm_pkg_get_name(data1),
					(char *)data3,
					alpm_pkg_get_name(data2));
			break;
		case PM_TRANS_CONV_CONFLICT_PKG:
			*response = yesno(1, _(":: %s conflicts with %s. Remove %s?"),
					(char *)data1,
					(char *)data2,
					(char *)data2);
			break;
		case PM_TRANS_CONV_LOCAL_NEWER:
			if(!config->op_s_downloadonly) {
				*response = yesno(1, _(":: %s-%s: local version is newer. Upgrade anyway?"),
						alpm_pkg_get_name(data1),
						alpm_pkg_get_version(data1));
			} else {
				*response = 1;
			}
			break;
		case PM_TRANS_CONV_CORRUPTED_PKG:
			*response = yesno(1, _(":: File %s is corrupted. Do you want to delete it?"),
					(char *)data1);
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
	int tmp, digits, textlen;
	char *opr = NULL;
	/* used for wide character width determination and printing */
	int len, wclen, wcwid, padwid;
	wchar_t *wcstr;

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

	/* find # of digits in package counts to scale output */
	digits = 1;
	tmp = howmany;
	while((tmp /= 10)) {
		++digits;
	}
	/* determine room left for non-digits text [not ( 1/12) part] */
	textlen = infolen - 3 - (2 * digits);

	/* In order to deal with characters from all locales, we have to worry
	 * about wide characters and their column widths. A lot of stuff is
	 * done here to figure out the actual number of screen columns used
	 * by the output, and then pad it accordingly so we fill the terminal.
	 */
	/* len = opr len + pkgname len (if available) + space  + null */
	len = strlen(opr) + ((pkgname) ? strlen(pkgname) : 0) + 2;
	wcstr = calloc(len, sizeof(wchar_t));
	/* print our strings to the alloc'ed memory */
#if defined(HAVE_SWPRINTF)
	wclen = swprintf(wcstr, len, L"%s %s", opr, pkgname);
#else
	/* because the format string was simple, we can easily do this without
	 * using swprintf, although it is probably not as safe/fast. The max
	 * chars we can copy is decremented each time by subtracting the length
	 * of the already printed/copied wide char string. */
	wclen = mbstowcs(wcstr, opr, len);
	wclen += mbstowcs(wcstr + wclen, " ", len - wclen);
	wclen += mbstowcs(wcstr + wclen, pkgname, len - wclen);
#endif
	wcwid = wcswidth(wcstr, wclen);
	padwid = textlen - wcwid;
	/* if padwid is < 0, we need to trim the string so padwid = 0 */
	if(padwid < 0) {
		int i = textlen - 3;
		wchar_t *p = wcstr;
		/* grab the max number of char columns we can fill */
		while(i > 0 && wcwidth(*p) < i) {
			i -= wcwidth(*p);
			p++;
		}
		/* then add the ellipsis and fill out any extra padding */
		wcscpy(p, L"...");
		padwid = i;

	}

	printf("(%*d/%*d) %ls%-*s", digits, remain, digits, howmany,
			wcstr, padwid, "");

	free(wcstr);

	/* call refactored fill progress function */
	fill_progress(percent, percent, getcols() - infolen);

	if(percent == 100) {
		alpm_list_t *i = NULL;
		on_progress = 0;
		for(i = output; i; i = i->next) {
			printf("%s", (char *)i->data);
		}
		fflush(stdout);
		FREELIST(output);
	} else {
		on_progress = 1;
	}
}

/* callback to handle receipt of total download value */
void cb_dl_total(off_t total)
{
	list_total = total;
	/* if we get a 0 value, it means this list has finished downloading,
	 * so clear out our list_xfered as well */
	if(total == 0) {
		list_xfered = 0;
	}
}

/* callback to handle display of download progress */
void cb_dl_progress(const char *filename, off_t file_xfered, off_t file_total)
{
	const int infolen = 50;
	const int filenamelen = infolen - 27;
	char *fname, *p;
	/* used for wide character width determination and printing */
	int len, wclen, wcwid, padwid;
	wchar_t *wcfname;

	int totaldownload;
	off_t xfered, total;
	float rate = 0.0, timediff = 0.0, f_xfered = 0.0;
	unsigned int eta_h = 0, eta_m = 0, eta_s = 0;
	int file_percent = 0, total_percent = 0;
	char rate_size = 'K', xfered_size = 'K';

	if(config->noprogressbar || file_total == -1) {
		if(file_xfered == 0) {
			printf(_("downloading %s...\n"), filename);
			fflush(stdout);
		}
		return;
	}

	/* only use TotalDownload if enabled and we have a callback value */
	if(config->totaldownload && list_total) {
		totaldownload = 1;
	} else {
		totaldownload = 0;
	}

	if(totaldownload) {
		xfered = list_xfered + file_xfered;
		total = list_total;
	} else {
		xfered = file_xfered;
		total = file_total;
	}

	/* this is basically a switch on xfered: 0, total, and
	 * anything else */
	if(file_xfered == 0) {
		/* set default starting values, ensure we only call this once
		 * if TotalDownload is enabled */
		if(!totaldownload || (totaldownload && list_xfered == 0)) {
			gettimeofday(&initial_time, NULL);
			xfered_last = (off_t)0;
			rate_last = 0.0;
			timediff = get_update_timediff(1);
		}
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
		rate = (rate + 2 * rate_last) / 3;
		eta_s = (total - xfered) / (rate * 1024.0);
		rate_last = rate;
		xfered_last = xfered;
	}

	file_percent = (int)((float)file_xfered) / ((float)file_total) * 100;

	if(totaldownload) {
		total_percent = (int)((float)list_xfered + file_xfered) /
			((float)list_total) * 100;

		/* if we are at the end, add the completed file to list_xfered */
		if(file_xfered == file_total) {
			list_xfered += file_total;
		}
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
	/* In order to deal with characters from all locales, we have to worry
	 * about wide characters and their column widths. A lot of stuff is
	 * done here to figure out the actual number of screen columns used
	 * by the output, and then pad it accordingly so we fill the terminal.
	 */
	/* len = filename len + null */
	len = strlen(filename) + 1;
	wcfname = calloc(len, sizeof(wchar_t));
	wclen = mbstowcs(wcfname, fname, len);
	wcwid = wcswidth(wcfname, wclen);
	padwid = filenamelen - wcwid;
	/* if padwid is < 0, we need to trim the string so padwid = 0 */
	if(padwid < 0) {
		int i = filenamelen - 3;
		wchar_t *p = wcfname;
		/* grab the max number of char columns we can fill */
		while(i > 0 && wcwidth(*p) < i) {
			i -= wcwidth(*p);
			p++;
		}
		/* then add the ellipsis and fill out any extra padding */
		wcscpy(p, L"...");
		padwid = i;

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

	printf(" %ls%-*s %6.1f%c %#6.1f%c/s %02u:%02u:%02u", wcfname,
			padwid, "", f_xfered, xfered_size,
			rate, rate_size, eta_h, eta_m, eta_s);

	free(fname);
	free(wcfname);

	if(totaldownload) {
		fill_progress(file_percent, total_percent, getcols() - infolen);
	} else {
		fill_progress(file_percent, file_percent, getcols() - infolen);
	}
	return;
}

/* Callback to handle notifications from the library */
void cb_log(pmloglevel_t level, char *fmt, va_list args)
{
	if(!fmt || strlen(fmt) == 0) {
		return;
	}

	if(on_progress) {
		char *string = NULL;
		pm_vasprintf(&string, level, fmt, args);
		if(string != NULL) {
			output = alpm_list_add(output, string);
		}
	} else {
		pm_vfprintf(stdout, level, fmt, args);
	}
}

/* vim: set ts=2 sw=2 noet: */
