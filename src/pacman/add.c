/*
 *  add.c
 * 
 *  Copyright (c) 2002-2005 by Judd Vinet <jvinet@zeroflux.org>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <alpm.h>
/* pacman */
#include "log.h"
#include "list.h"
#include "download.h"
#include "trans.h"

extern unsigned char pmo_upgrade;
extern unsigned char pmo_flags;

int pacman_add(list_t *targets)
{
	PM_LIST *data;
	list_t *i;

	if(targets == NULL) {
		return(0);
	}

	/* Check for URL targets and process them
	 */
	for(i = targets; i; i = i->next) {
		if(strstr(i->data, "://")) {
			char *str = fetch_pkgurl(i->data);
			if(str == NULL) {
				return(1);
			} else {
				free(i->data);
				i->data = str;
			}
		}
	}

	/* Step 1: create a new transaction
	 */
	if(alpm_trans_init((pmo_upgrade == 0) ? PM_TRANS_TYPE_ADD : PM_TRANS_TYPE_UPGRADE,
	                   pmo_flags, cb_trans) == -1) {
		ERR(NL, "%s\n", alpm_strerror(pm_errno));
		return(1);
	}

	/* and add targets to it */
	MSG(NL, "loading package data... ");
	for(i = targets; i; i = i->next) {
		if(alpm_trans_addtarget(i->data) == -1) {
			ERR(NL, "failed to add target '%s' (%s)\n", (char *)i->data, alpm_strerror(pm_errno));
			return(1);
		}
	}
	MSG(CL, "done");

	/* Step 2: "compute" the transaction based on targets and flags
	 */
	if(alpm_trans_prepare(&data) == -1) {
		PM_LIST *i;

		ERR(NL, "failed to prepare transaction (%s)\n", alpm_strerror(pm_errno));
		switch(pm_errno) {
			case PM_ERR_UNSATISFIED_DEPS:
				for(i = alpm_list_first(data); i; i = alpm_list_next(i)) {
					pmdepmissing_t *miss = alpm_list_getdata(i);

					MSG(NL, ":: %s: requires %s", miss->target, miss->depend.name);
					switch(miss->depend.mod) {
						case PM_DEP_EQ: MSG(CL, "=%s", miss->depend.version);  break;
						case PM_DEP_GE: MSG(CL, ">=%s", miss->depend.version); break;
						case PM_DEP_LE: MSG(CL, "<=%s", miss->depend.version); break;
					}
					MSG(CL, "\n");
				}
				alpm_list_free(data);
			break;
			case PM_ERR_CONFLICTING_DEPS:
				for(i = alpm_list_first(data); i; i = alpm_list_next(i)) {
					pmdepmissing_t *miss = alpm_list_getdata(i);

					MSG(NL, ":: %s: conflicts with %s",
						miss->target, miss->depend.name, miss->depend.name);
				}
				alpm_list_free(data);
			break;
			case PM_ERR_FILE_CONFLICTS:
				for(i = alpm_list_first(data); i; i = alpm_list_next(i)) {
					MSG(NL, ":: %s\n", (char *)alpm_list_getdata(i));
				}
				alpm_list_free(data);
				MSG(NL, "\nerrors occurred, no packages were upgraded.\n\n");
			break;
			default:
			break;
		}
		alpm_trans_release();
		return(1);
	}

	/* Step 3: actually perform the installation
	 */
	if(alpm_trans_commit() == -1) {
		ERR(NL, "failed to commit transaction (%s)\n", alpm_strerror(pm_errno));
		return(1);
	}

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
