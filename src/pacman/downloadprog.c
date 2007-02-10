/*
 *  downloadprog.c
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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <libintl.h>
#include <math.h>

#include <alpm.h>
/* pacman */
#include "util.h"
#include "log.h"
#include "downloadprog.h"
#include "conf.h"

/* progress bar */
static float rate_last;
static int xfered_last;
static struct timeval initial_time;

/* pacman options */
extern config_t *config;

#define FILENAME_TRIM_LEN 23

void log_progress(const char *filename, int xfered, int total)
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
		/* set default starting values */
		gettimeofday(&initial_time, NULL);
		xfered_last = 0;
		rate_last = 0.0;
		timediff = get_update_timediff(1);
		rate = 0.0;
		eta_s = 0;
		set_output_padding(1); /* we need padding from pm_fprintf output */
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

		set_output_padding(0); /* shut off padding */
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

/* vim: set ts=2 sw=2 noet: */
