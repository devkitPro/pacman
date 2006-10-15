/*
 *  remove.c
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
#include "util.h"
#include "log.h"
#include "list.h"
#include "trans.h"
#include "remove.h"
#include "conf.h"

extern config_t *config;

extern PM_DB *db_local;

int pacman_remove(list_t *targets)
{
	PM_LIST *data;
	list_t *i;
	list_t *finaltargs = NULL;
	int retval = 0;

	if(targets == NULL) {
		return(0);
	}

	/* If the target is a group, ask if its packages should be removed 
	 * (the library can't remove groups for now)
	 */
	for(i = targets; i; i = i->next) {
		PM_GRP *grp;

		grp = alpm_db_readgrp(db_local, i->data);
		if(grp) {
			PM_LIST *lp, *pkgnames;
			int all;

			pkgnames = alpm_grp_getinfo(grp, PM_GRP_PKGNAMES);

			MSG(NL, _(":: group %s:\n"), alpm_grp_getinfo(grp, PM_GRP_NAME));
			PM_LIST_display("   ", pkgnames);
			all = yesno(_("    Remove whole content? [Y/n] "));
			for(lp = alpm_list_first(pkgnames); lp; lp = alpm_list_next(lp)) {
				if(all || yesno(_(":: Remove %s from group %s? [Y/n] "), (char *)alpm_list_getdata(lp), i->data)) {
					finaltargs = list_add(finaltargs, strdup(alpm_list_getdata(lp)));
				}
			}
		} else {
			/* not a group, so add it to the final targets */
			finaltargs = list_add(finaltargs, strdup(i->data));
		}
	}

	/* Step 1: create a new transaction
	 */
	if(alpm_trans_init(PM_TRANS_TYPE_REMOVE, config->flags, cb_trans_evt, cb_trans_conv, cb_trans_progress) == -1) {
		ERR(NL, _("failed to init transaction (%s)\n"), alpm_strerror(pm_errno));
		if(pm_errno == PM_ERR_HANDLE_LOCK) {
			MSG(NL, _("       if you're sure a package manager is not already running,\n"
			  			"       you can remove %s%s\n"), config->root, PM_LOCK);
		}
		FREELIST(finaltargs);
		return(1);
	}
	/* and add targets to it */
	for(i = finaltargs; i; i = i->next) {
		if(alpm_trans_addtarget(i->data) == -1) {
			ERR(NL, _("failed to add target '%s' (%s)\n"), (char *)i->data, alpm_strerror(pm_errno));
			retval = 1;
			goto cleanup;
		}
	}

	/* Step 2: prepare the transaction based on its type, targets and flags
	 */
	if(alpm_trans_prepare(&data) == -1) {
		PM_LIST *lp;
		ERR(NL, _("failed to prepare transaction (%s)\n"), alpm_strerror(pm_errno));
		switch(pm_errno) {
			case PM_ERR_UNSATISFIED_DEPS:
				for(lp = alpm_list_first(data); lp; lp = alpm_list_next(lp)) {
					PM_DEPMISS *miss = alpm_list_getdata(lp);
					MSG(NL, _("  %s: is required by %s\n"), alpm_dep_getinfo(miss, PM_DEP_TARGET),
					    alpm_dep_getinfo(miss, PM_DEP_NAME));
				}
				alpm_list_free(data);
			break;
			default:
			break;
		}
		retval = 1;
		goto cleanup;
	}

	/* Warn user in case of dangerous operation
	 */
	if(config->flags & PM_TRANS_FLAG_RECURSE || config->flags & PM_TRANS_FLAG_CASCADE) {
		PM_LIST *lp;
		/* list transaction targets */
		i = NULL;
		for(lp = alpm_list_first(alpm_trans_getinfo(PM_TRANS_PACKAGES)); lp; lp = alpm_list_next(lp)) {
			PM_PKG *pkg = alpm_list_getdata(lp);
			i = list_add(i, strdup(alpm_pkg_getinfo(pkg, PM_PKG_NAME)));
		}
		list_display(_("\nTargets:"), i);
		FREELIST(i);
		/* get confirmation */
		if(yesno(_("\nDo you want to remove these packages? [Y/n] ")) == 0) {
			retval = 1;
			goto cleanup;
		}
		MSG(NL, "\n");
	}

	/* Step 3: actually perform the removal
	 */
	if(alpm_trans_commit(NULL) == -1) {
		ERR(NL, _("failed to commit transaction (%s)\n"), alpm_strerror(pm_errno));
		retval = 1;
		goto cleanup;
	}

	/* Step 4: release transaction resources
	 */
cleanup:
	FREELIST(finaltargs);
	if(alpm_trans_release() == -1) {
		ERR(NL, _("failed to release transaction (%s)\n"), alpm_strerror(pm_errno));
		retval = 1;
	}

	return(retval);
}

/* vim: set ts=2 sw=2 noet: */
