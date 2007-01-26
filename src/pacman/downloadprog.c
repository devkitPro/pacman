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
static struct timeval last_time;
static struct timeval initial_time;

/* pacman options */
extern config_t *config;

#define FILENAME_TRIM_LEN 21
#define UPDATE_SPEED_SEC 0.1

void log_progress(const char *filename, int xfered, int total)
{
	static unsigned int lasthash = 0, mouth = 0;
	unsigned int i, hash;
	/* a little hard to conceal easter eggs in open-source software,
	 * but they're still fun. ;) */
	const unsigned short chomp = alpm_option_get_chomp();
	char *fname, *p; 
	unsigned int maxcols = getcols();
	unsigned int progresslen = maxcols - 57;
	int percent = (int)((float)xfered) / ((float)total) * 100;
	struct timeval current_time;
	float rate = 0.0;
	unsigned int eta_h = 0, eta_m = 0, eta_s = 0;
	float total_timediff, timediff;

  if(xfered == 0) {
		set_output_padding(1); /* we need padding from pm_fprintf output */
		gettimeofday(&initial_time, NULL);
		gettimeofday(&last_time, NULL);
		xfered_last = 0;
		rate_last = 0.0;
	}

	if(config->noprogressbar) {
		return;
	}

	gettimeofday(&current_time, NULL);
	total_timediff = current_time.tv_sec-initial_time.tv_sec
		+ (float)(current_time.tv_usec-initial_time.tv_usec) / 1000000;
	timediff = current_time.tv_sec-last_time.tv_sec
		+ (float)(current_time.tv_usec-last_time.tv_usec) / 1000000;

	if(xfered == total) {
		/* compute final values */
		rate = (float)total / (total_timediff * 1024);
		eta_s = (unsigned int)total_timediff;
		set_output_padding(0); /* shut off padding */
	} else if(timediff < UPDATE_SPEED_SEC) {
	/* we avoid computing the ETA on too small periods of time, so that
		 results are more significant */
		return;
	} else {
		rate = (float)(xfered - xfered_last) / (timediff * 1024);
		rate = (float)(rate + 2*rate_last) / 3;
		eta_s = (unsigned int)(total - xfered) / (rate * 1024);
	}

	rate_last = rate;
	last_time = current_time;
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

	/* hide the cursor i - prevent flicker
	printf("\033[?25l\033[?1c");
	*/

	/*
	 * DL rate cap, for printf formatting - this should be sane for a while
	 * if anything we can change to MB/s if we need a higher rate
	 */
	if(rate > 9999.9) {
		rate = 9999.9;
	}

	printf(" %-*s %6dK %#6.1fK/s %02u:%02u:%02u [", FILENAME_TRIM_LEN, fname, 
				 xfered/1024, rate, eta_h, eta_m, eta_s);

	free(fname);
	
	hash = (unsigned int)percent*progresslen/100;
	for(i = progresslen; i > 0; --i) {
		if(chomp) {
			if(i > progresslen - hash) {
				printf("-");
			} else if(i == progresslen - hash) {
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
		} else if(i > progresslen - hash) {
			printf("#");
		} else {
			printf("-");
		}
	}
	printf("] %3d%%\r", percent);

	if(percent == 100) {
		printf("\n");
	}
	fflush(stdout);
	return;
}

/* vim: set ts=2 sw=2 noet: */
