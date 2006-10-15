/*
 *  download.c
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
#include <ftplib.h>
#include <libintl.h>

#include <alpm.h>
/* pacman */
#include "util.h"
#include "log.h"
#include "list.h"
#include "download.h"
#include "conf.h"

/* progress bar */
char sync_fnm[25];
int offset;
struct timeval t0, t;
float rate;
int xfered1;
unsigned char eta_h, eta_m, eta_s;

/* pacman options */
extern config_t *config;

extern unsigned int maxcols;

int log_progress(netbuf *ctl, int xfered, void *arg)
{
	int fsz = *(int*)arg;
	int pct = ((float)(xfered+offset) / fsz) * 100;
	static int lastpct=0;
	unsigned int i, cur;
	struct timeval t1;
	float timediff;
	/* a little hard to conceal easter eggs in open-source software, but
	 * they're still fun.  ;)
	 */
	int chomp;
	static unsigned short mouth;
	static unsigned int   lastcur = 0;

	/* we don't need that parameter */
	ctl=NULL;

	if(config->noprogressbar || (pct == 100 && lastpct == 100)) {
		return(1);
	}

	alpm_get_option(PM_OPT_CHOMP, (long *)&chomp);

	gettimeofday(&t1, NULL);
	if(xfered+offset == fsz) {
		t = t0;
	}
	timediff = t1.tv_sec-t.tv_sec + (float)(t1.tv_usec-t.tv_usec) / 1000000;

	if(xfered+offset == fsz) {
		/* average download rate */
		rate = xfered / (timediff * 1024);
		/* total download time */
		eta_s = (int)timediff;
		eta_h = eta_s / 3600;
		eta_s -= eta_h * 3600;
		eta_m = eta_s / 60;
		eta_s -= eta_m * 60;
	} else if(timediff > 1) {
		/* we avoid computing the rate & ETA on too small periods of time, so that
		   results are more significant */
		rate = (xfered-xfered1) / (timediff * 1024);
		xfered1 = xfered;
		gettimeofday(&t, NULL);
		eta_s = (fsz-(xfered+offset)) / (rate * 1024);
		eta_h = eta_s / 3600;
		eta_s -= eta_h * 3600;
		eta_m = eta_s / 60;
		eta_s -= eta_m * 60;
	}

	printf(" %s [", sync_fnm);
	cur = (int)((maxcols-64)*pct/100);
	for(i = 0; i < maxcols-64; i++) {
		if(chomp) {
			if(i < cur) {
				printf("-");
			} else {
				if(i == cur) {
					if(lastcur == cur) {
						if(mouth) {
							printf("\033[1;33mC\033[m");
						} else {
							printf("\033[1;33mc\033[m");
						}
					} else {
						mouth = mouth == 1 ? 0 : 1;
						if(mouth) {
							printf("\033[1;33mC\033[m");
						} else {
							printf("\033[1;33mc\033[m");
						}
					}
				} else {
					printf("\033[0;37m*\033[m");
				}
			}
		} else {
			(i < cur) ? printf("#") : printf(" ");
		}
	}
	if(rate > 1000) {
		printf("] %3d%%  %6dK  %6.0fK/s  %02d:%02d:%02d\r", pct, ((xfered+offset) / 1024), rate, eta_h, eta_m, eta_s);
	} else {
		printf("] %3d%%  %6dK  %6.1fK/s  %02d:%02d:%02d\r", pct, ((xfered+offset) / 1024), rate, eta_h, eta_m, eta_s);
	}
	if(lastpct != 100 && pct == 100) {
		printf("\n");
	}
	lastcur = cur;
	lastpct = pct;
	fflush(stdout);
	return(1);
}

/* vim: set ts=2 sw=2 noet: */
