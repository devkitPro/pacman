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

	struct timeval current_time;
	float rate = 0.0;
	unsigned int eta_h = 0, eta_m = 0, eta_s = 0;
	float total_timediff, timediff;

	if(config->noprogressbar) {
		return;
	}

	int percent = (int)((float)xfered) / ((float)total) * 100;

  if(xfered == 0) {
		set_output_padding(1); /* we need padding from pm_fprintf output */
		gettimeofday(&initial_time, NULL);
		xfered_last = 0;
		rate_last = 0.0;
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

	gettimeofday(&current_time, NULL);
	total_timediff = current_time.tv_sec-initial_time.tv_sec
		+ (float)(current_time.tv_usec-initial_time.tv_usec) / 1000000;

	if(xfered == total) {
		/* compute final values */
		rate = (float)total / (total_timediff * 1024);
		eta_s = (unsigned int)total_timediff;
		set_output_padding(0); /* shut off padding */
	} else {
		rate = (float)(xfered - xfered_last) / (timediff * 1024);
		rate = (float)(rate + 2*rate_last) / 3;
		eta_s = (unsigned int)(total - xfered) / (rate * 1024);
	}

	rate_last = rate;
	xfered_last = xfered;
	
	/* fix up time for display */
	eta_h = eta_s / 3600;
	eta_s -= eta_h * 3600;
	eta_m = eta_s / 60;
	eta_s -= eta_m * 60;

	fname = strdup(filename);
	if((p = strstr(fname, PM_EXT_PKG)) || (p = strstr(fname, PM_EXT_DB))) {
			*p = '\0';
	}
	if(strlen(fname) > FILENAME_TRIM_LEN) {
		fname[FILENAME_TRIM_LEN] = '\0';
	}

	/* DL rate cap, for printf formatting - this should be sane for a while
	 * if anything we can change to MB/s if we need a higher rate */
	if(rate > 9999.9) {
		rate = 9999.9;
	}

	printf(" %-*s %6dK %#6.1fK/s %02u:%02u:%02u", FILENAME_TRIM_LEN, fname, 
				 xfered/1024, rate, eta_h, eta_m, eta_s);

	free(fname);
	
	fill_progress(percent, getcols() - infolen);
	return;
}

/* vim: set ts=2 sw=2 noet: */
