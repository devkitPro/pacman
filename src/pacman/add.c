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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libintl.h>

#include <alpm.h>
/* pacman */
#include "log.h"
#include "list.h"
#include "download.h"
#include "trans.h"
#include "add.h"
#include "conf.h"
#include "util.h"

extern config_t *config;

int pacman_add(list_t *targets)
{
	PM_LIST *data;
	list_t *i;
	int retval = 0;

	if(targets == NULL) {
		return(0);
	}

	/* Check for URL targets and process them
	 */
	for(i = targets; i; i = i->next) {
		if(strstr(i->data, "://")) {
			char *str = alpm_fetch_pkgurl(i->data);
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
	if(alpm_trans_init((config->upgrade == 0) ? PM_TRANS_TYPE_ADD : PM_TRANS_TYPE_UPGRADE,
	                   config->flags, cb_trans_evt, cb_trans_conv, cb_trans_progress) == -1) {
		ERR(NL, "%s\n", alpm_strerror(pm_errno));
		if(pm_errno == PM_ERR_HANDLE_LOCK) {
			MSG(NL, _("       if you're sure a package manager is not already running,\n"
			  "       you can remove %s%s\n"), config->root, PM_LOCK);
		}
		return(1);
	}

	/* and add targets to it */
	MSG(NL, _("loading package data... "));
	for(i = targets; i; i = i->next) {
		if(alpm_trans_addtarget(i->data) == -1) {
			ERR(NL, _("failed to add target '%s' (%s)\n"), (char *)i->data, alpm_strerror(pm_errno));
			retval = 1;
			goto cleanup;
		}
	}
	MSG(CL, _("done."));

	/* Step 2: "compute" the transaction based on targets and flags
	 */
	if(alpm_trans_prepare(&data) == -1) {
		long long *pkgsize, *freespace;
		PM_LIST *i;

		ERR(NL, _("failed to prepare transaction (%s)\n"), alpm_strerror(pm_errno));
		switch(pm_errno) {
			case PM_ERR_UNSATISFIED_DEPS:
				for(i = alpm_list_first(data); i; i = alpm_list_next(i)) {
					PM_DEPMISS *miss = alpm_list_getdata(i);
					MSG(NL, _(":: %s: requires %s"), alpm_dep_getinfo(miss, PM_DEP_TARGET),
					                              alpm_dep_getinfo(miss, PM_DEP_NAME));
					switch((long)alpm_dep_getinfo(miss, PM_DEP_MOD)) {
						case PM_DEP_MOD_EQ: MSG(CL, "=%s", alpm_dep_getinfo(miss, PM_DEP_VERSION));  break;
						case PM_DEP_MOD_GE: MSG(CL, ">=%s", alpm_dep_getinfo(miss, PM_DEP_VERSION)); break;
						case PM_DEP_MOD_LE: MSG(CL, "<=%s", alpm_dep_getinfo(miss, PM_DEP_VERSION)); break;
					}
					MSG(CL, "\n");
				}
				alpm_list_free(data);
			break;
			case PM_ERR_CONFLICTING_DEPS:
				for(i = alpm_list_first(data); i; i = alpm_list_next(i)) {
					PM_DEPMISS *miss = alpm_list_getdata(i);
					MSG(NL, _(":: %s: conflicts with %s"),
						alpm_dep_getinfo(miss, PM_DEP_TARGET), alpm_dep_getinfo(miss, PM_DEP_NAME));
				}
				alpm_list_free(data);
			break;
			case PM_ERR_FILE_CONFLICTS:
				for(i = alpm_list_first(data); i; i = alpm_list_next(i)) {
					PM_CONFLICT *conflict = alpm_list_getdata(i);
					switch((long)alpm_conflict_getinfo(conflict, PM_CONFLICT_TYPE)) {
						case PM_CONFLICT_TYPE_TARGET:
							MSG(NL, _("%s%s exists in \"%s\" (target) and \"%s\" (target)"),
											config->root,
							        (char *)alpm_conflict_getinfo(conflict, PM_CONFLICT_FILE),
							        (char *)alpm_conflict_getinfo(conflict, PM_CONFLICT_TARGET),
							        (char *)alpm_conflict_getinfo(conflict, PM_CONFLICT_CTARGET));
						break;
						case PM_CONFLICT_TYPE_FILE:
							MSG(NL, _("%s: %s%s exists in filesystem"),
							        (char *)alpm_conflict_getinfo(conflict, PM_CONFLICT_TARGET),
											config->root,
							        (char *)alpm_conflict_getinfo(conflict, PM_CONFLICT_FILE));
						break;
					}
				}
				alpm_list_free(data);
				MSG(NL, _("\nerrors occurred, no packages were upgraded.\n"));
			break;
			case PM_ERR_DISK_FULL:
				i = alpm_list_first(data);
				pkgsize = alpm_list_getdata(i);
				i = alpm_list_next(i);
				freespace = alpm_list_getdata(i);
					MSG(NL, _(":: %.1f MB required, have %.1f MB"),
						(double)(*pkgsize / 1048576.0), (double)(*freespace / 1048576.0));
				alpm_list_free(data);
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
	if(alpm_trans_release() == -1) {
		ERR(NL, _("failed to release transaction (%s)\n"), alpm_strerror(pm_errno));
		retval=1;
	}

	return(retval);
}

/* vim: set ts=2 sw=2 noet: */
