/*
 *  add.c
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

#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "pacman.h"
#include "log.h"
#include "downloadprog.h"
#include "trans.h"
#include "conf.h"
#include "util.h"

extern config_t *config;

int pacman_add(alpm_list_t *targets)
{
	alpm_list_t *i = targets, *data = NULL;
	int retval = 0;

	if(targets == NULL) {
		return(0);
	}

	/* Check for URL targets and process them
	 */
	while(i) {
		if(strstr(i->data, "://")) {
			char *str = alpm_fetch_pkgurl(i->data);
			if(str == NULL) {
				return(1);
			} else {
				free(i->data);
				i->data = str;
			}
		}
		i = i->next;
	}

	/* Step 1: create a new transaction
	 */
	if(alpm_trans_init((config->upgrade == 0) ? PM_TRANS_TYPE_ADD : PM_TRANS_TYPE_UPGRADE,
	                   config->flags, cb_trans_evt, cb_trans_conv, cb_trans_progress) == -1) {
		ERR(NL, "%s\n", alpm_strerror(pm_errno));
		if(pm_errno == PM_ERR_HANDLE_LOCK) {
			MSG(NL, _("       if you're sure a package manager is not already running,\n"
						    "       you can remove %s%s\n"), alpm_option_get_root(), PM_LOCK);
		}
		return(1);
	}

	/* and add targets to it */
	MSG(NL, _("loading package data... "));
	for(i = targets; i; i = i->next) {
		if(alpm_trans_addtarget(i->data) == -1) {
			MSG(NL, "\n");
			ERR(NL, _("failed to add target '%s' (%s)"), (char *)i->data, alpm_strerror(pm_errno));
			retval = 1;
			goto cleanup;
		}
	}
	MSG(CL, _("done.\n"));

	/* Step 2: "compute" the transaction based on targets and flags
	 */
	if(alpm_trans_prepare(&data) == -1) {
		long long *pkgsize, *freespace;

		ERR(NL, _("failed to prepare transaction (%s)\n"), alpm_strerror(pm_errno));
		switch(pm_errno) {
			case PM_ERR_UNSATISFIED_DEPS:
				for(i = data; i; i = alpm_list_next(i)) {
					pmdepmissing_t *miss = alpm_list_getdata(i);
				
					/* TODO indicate if the error was a virtual package or not:
					 *		:: %s: requires %s, provided by %s
					 */
					MSG(NL, _(":: %s: requires %s"), alpm_dep_get_target(miss),
					                              alpm_dep_get_name(miss));
					switch(alpm_dep_get_mod(miss)) {
						case PM_DEP_MOD_ANY:
							break;
						case PM_DEP_MOD_EQ:
							MSG(CL, "=%s", alpm_dep_get_version(miss));
							break;
						case PM_DEP_MOD_GE:
							MSG(CL, ">=%s", alpm_dep_get_version(miss));
							break;
						case PM_DEP_MOD_LE:
							MSG(CL, "<=%s", alpm_dep_get_version(miss));
							break;
					}
					MSG(CL, "\n");
				}
			break;
			case PM_ERR_CONFLICTING_DEPS:
				for(i = data; i; i = alpm_list_next(i)) {
					pmdepmissing_t *miss = alpm_list_getdata(i);
					MSG(NL, _(":: %s: conflicts with %s"),
						alpm_dep_get_target(miss), alpm_dep_get_name(miss));
				}
			break;
			case PM_ERR_FILE_CONFLICTS:
				for(i = data; i; i = alpm_list_next(i)) {
					pmconflict_t *conflict = alpm_list_getdata(i);
					switch(alpm_conflict_get_type(conflict)) {
						case PM_CONFLICT_TYPE_TARGET:
							MSG(NL, _("%s exists in both '%s' and '%s'\n"),
							        alpm_conflict_get_file(conflict),
							        alpm_conflict_get_target(conflict),
							        alpm_conflict_get_ctarget(conflict));
						break;
						case PM_CONFLICT_TYPE_FILE:
							MSG(NL, _("%s: %s exists in filesystem\n"),
							        alpm_conflict_get_target(conflict),
							        alpm_conflict_get_file(conflict));
						break;
					}
				}
				MSG(NL, _("\nerrors occurred, no packages were upgraded.\n"));
			break;
			/* TODO This is gross... we should not return these values in the same list we
			 * would get conflicts and such with... it's just silly
			 */
			case PM_ERR_DISK_FULL:
				i = data;
				pkgsize = alpm_list_getdata(i);
				i = alpm_list_next(i);
				freespace = alpm_list_getdata(i);
					MSG(NL, _(":: %.1f MB required, have %.1f MB"),
						(double)(*pkgsize / (1024.0*1024.0)), (double)(*freespace / (1024.0*1024.0)));
			break;
			default:
			break;
		}
		retval=1;
		goto cleanup;
	}

	/* Step 3: actually perform the installation
	 */
	if(alpm_trans_commit(NULL) == -1) {
		ERR(NL, _("failed to commit transaction (%s)\n"), alpm_strerror(pm_errno));
		retval=1;
		goto cleanup;
	}

cleanup:
	if(data) {
		alpm_list_free(data);
	}
	if(alpm_trans_release() == -1) {
		ERR(NL, _("failed to release transaction (%s)\n"), alpm_strerror(pm_errno));
		retval=1;
	}

	return(retval);
}

/* vim: set ts=2 sw=2 noet: */
