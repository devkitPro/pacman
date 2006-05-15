/*
 *  group.c
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libintl.h>
/* pacman */
#include "util.h"
#include "error.h"
#include "log.h"
#include "group.h"
#include "alpm.h"

pmgrp_t *_alpm_grp_new()
{
	pmgrp_t* grp;

	grp = (pmgrp_t *)malloc(sizeof(pmgrp_t));
	if(grp == NULL) {
		_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"),
		                        sizeof(pmgrp_t));
		RET_ERR(PM_ERR_MEMORY, NULL);
	}

	grp->name[0] = '\0';
	grp->packages = NULL;

	return(grp);
}

void _alpm_grp_free(void *data)
{
	pmgrp_t *grp = data;

	if(grp == NULL) {
		return;
	}

	FREELIST(grp->packages);
	free(grp);

	return;
}

/* Helper function for sorting groups
 */
int _alpm_grp_cmp(const void *g1, const void *g2)
{
	pmgrp_t *grp1 = (pmgrp_t *)g1;
	pmgrp_t *grp2 = (pmgrp_t *)g2;

	return(strcmp(grp1->name, grp2->name));
}

/* vim: set ts=2 sw=2 noet: */
