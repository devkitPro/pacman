/*
 *  group.c
 *
 *  Copyright (c) 2006-2010 Pacman Development Team <pacman-dev@archlinux.org>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* libalpm */
#include "group.h"
#include "alpm_list.h"
#include "util.h"
#include "log.h"
#include "alpm.h"

pmgrp_t *_alpm_grp_new(const char *name)
{
	pmgrp_t* grp;

	ALPM_LOG_FUNC;

	CALLOC(grp, 1, sizeof(pmgrp_t), RET_ERR(PM_ERR_MEMORY, NULL));
	STRDUP(grp->name, name, RET_ERR(PM_ERR_MEMORY, NULL));

	return(grp);
}

void _alpm_grp_free(pmgrp_t *grp)
{
	ALPM_LOG_FUNC;

	if(grp == NULL) {
		return;
	}

	FREE(grp->name);
	/* do NOT free the contents of the list, just the nodes */
	alpm_list_free(grp->packages);
	FREE(grp);
}

const char SYMEXPORT *alpm_grp_get_name(const pmgrp_t *grp)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(grp != NULL, return(NULL));

	return grp->name;
}

alpm_list_t SYMEXPORT *alpm_grp_get_pkgs(const pmgrp_t *grp)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(grp != NULL, return(NULL));

	return grp->packages;
}
/* vim: set ts=2 sw=2 noet: */
