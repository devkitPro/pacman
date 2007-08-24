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

/* libalpm */
#include "group.h"
#include "alpm_list.h"
#include "util.h"
#include "error.h"
#include "log.h"
#include "alpm.h"

pmgrp_t *_alpm_grp_new()
{
	pmgrp_t* grp;

	ALPM_LOG_FUNC;

	grp = calloc(1, sizeof(pmgrp_t));
	if(grp == NULL) {
		_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes\n"),
		                        sizeof(pmgrp_t));
		RET_ERR(PM_ERR_MEMORY, NULL);
	}

	return(grp);
}

void _alpm_grp_free(pmgrp_t *grp)
{
	ALPM_LOG_FUNC;

	if(grp == NULL) {
		return;
	}

	FREELIST(grp->packages);
	FREE(grp);
}

/* Helper function for sorting groups
 */
int _alpm_grp_cmp(const void *g1, const void *g2)
{
	pmgrp_t *grp1 = (pmgrp_t *)g1;
	pmgrp_t *grp2 = (pmgrp_t *)g2;

	return(strcmp(grp1->name, grp2->name));
}

const char SYMEXPORT *alpm_grp_get_name(const pmgrp_t *grp)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(grp != NULL, return(NULL));

	return grp->name;
}

const alpm_list_t SYMEXPORT *alpm_grp_get_pkgs(const pmgrp_t *grp)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(grp != NULL, return(NULL));

	return grp->packages;
}
/* vim: set ts=2 sw=2 noet: */
