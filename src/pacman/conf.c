/*
 *  conf.c
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h> /* strdup */

/* pacman */
#include "conf.h"
#include "util.h"

/* global config variable */
config_t *config = NULL;

config_t *config_new(void)
{
	config_t *newconfig = calloc(1, sizeof(config_t));
	if(!newconfig) {
			pm_fprintf(stderr, PM_LOG_ERROR,
					_("malloc failure: could not allocate %zd bytes\n"),
					sizeof(config_t));
			return(NULL);
	}
	/* defaults which may get overridden later */
	newconfig->op = PM_OP_MAIN;
	newconfig->logmask = PM_LOG_ERROR | PM_LOG_WARNING;
	/* CONFFILE is defined at compile-time */
	newconfig->configfile = strdup(CONFFILE);
	newconfig->rootdir = NULL;
	newconfig->dbpath = NULL;
	newconfig->logfile = NULL;
	newconfig->syncfirst = NULL;

	return(newconfig);
}

int config_free(config_t *oldconfig)
{
	if(oldconfig == NULL) {
		return(-1);
	}

	FREELIST(oldconfig->syncfirst);
	free(oldconfig->configfile);
	free(oldconfig->rootdir);
	free(oldconfig->dbpath);
	free(oldconfig->logfile);
	free(oldconfig);
	oldconfig = NULL;

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
