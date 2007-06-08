/*
 *  testpkg.c : Test a pacman package for validity
 *
 *  Copyright (c) 2007 by Aaron Griffin <aaronmgriffin@gmail.com>
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
#include <stdarg.h>
#include <string.h>
#include <libgen.h>

#include <alpm.h>

void output_cb(pmloglevel_t level, char *fmt, va_list args)
{
	if(strlen(fmt)) {
        switch(level) {
        case PM_LOG_ERROR: printf("error: "); break;
        case PM_LOG_WARNING: printf("warning: "); break;
		default: break;
        }
		vprintf(fmt, args);
    }
}

int main(int argc, char **argv)
{
    int retval = 1; /* default = false */
    pmpkg_t *pkg = NULL;

    if(argc != 2) {
		fprintf(stderr, "usage: %s <package file>\n", basename(argv[0]));
		return(1);
	}

	if(alpm_initialize() == -1) {
		fprintf(stderr, "cannot initilize alpm: %s\n", alpm_strerror(pm_errno));
        return(1);
	}

    /* let us get log messages from libalpm */
	alpm_option_set_logcb(output_cb);

	if(alpm_pkg_load(argv[1], &pkg) == -1 || pkg == NULL) {
        retval = 1;
	} else {
		alpm_pkg_free(pkg);
        retval = 0;
	}
    
	if(alpm_release() == -1) {
		fprintf(stderr, "error releasing alpm: %s\n", alpm_strerror(pm_errno));
	}

    return(retval);
}
