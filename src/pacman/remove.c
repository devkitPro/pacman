/*
 *  remove.c
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
#include <string.h>

#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "pacman.h"
#include "util.h"
#include "conf.h"

extern pmdb_t *db_local;

/**
 * @brief Remove a specified list of packages.
 *
 * @param targets a list of packages (as strings) to remove from the system
 *
 * @return 0 on success, 1 on failure
 */
int pacman_remove(alpm_list_t *targets)
{
	alpm_list_t *i, *data = NULL, *finaltargs = NULL;
	int retval = 0;

	if(targets == NULL) {
		pm_printf(PM_LOG_ERROR, _("no targets specified (use -h for help)\n"));
		return(1);
	}

	/* If the target is a group, ask if its packages should be removed
	 * (the library can't remove groups for now)
	 */
	for(i = targets; i; i = alpm_list_next(i)) {
		pmgrp_t *grp = alpm_db_readgrp(db_local, alpm_list_getdata(i));
		if(grp) {
			int all;
			const alpm_list_t *p, *pkgnames = alpm_grp_get_pkgs(grp);

			printf(_(":: group %s:\n"), alpm_grp_get_name(grp));
			list_display("   ", pkgnames);
			all = yesno(1, _("    Remove whole content?"));

			for(p = pkgnames; p; p = alpm_list_next(p)) {
				char *pkg = alpm_list_getdata(p);
				if(all || yesno(1, _(":: Remove %s from group %s?"), pkg, (char *)alpm_list_getdata(i))) {
					finaltargs = alpm_list_add(finaltargs, strdup(pkg));
				}
			}
		} else {
			/* not a group, so add it to the final targets */
			finaltargs = alpm_list_add(finaltargs, strdup(alpm_list_getdata(i)));
		}
	}

	/* Step 1: create a new transaction */
	if(trans_init(PM_TRANS_TYPE_REMOVE, config->flags) == -1) {
		FREELIST(finaltargs);
		return(1);
	}

	/* add targets to the created transaction */
	printf(_("loading package data...\n"));
	for(i = finaltargs; i; i = alpm_list_next(i)) {
		char *targ = alpm_list_getdata(i);
		if(alpm_trans_addtarget(targ) == -1) {
			fprintf(stderr, _("error: '%s': %s\n"),
					targ, alpm_strerrorlast());
			trans_release();
			FREELIST(finaltargs);
			return(1);
		}
	}

	/* Step 2: prepare the transaction based on its type, targets and flags */
	if(alpm_trans_prepare(&data) == -1) {
		fprintf(stderr, _("error: failed to prepare transaction (%s)\n"),
		        alpm_strerrorlast());
		switch(pm_errno) {
			case PM_ERR_UNSATISFIED_DEPS:
				for(i = data; i; i = alpm_list_next(i)) {
					pmdepmissing_t *miss = alpm_list_getdata(i);
					pmdepend_t *dep = alpm_miss_get_dep(miss);
					char *depstring = alpm_dep_get_string(dep);
					printf(_(":: %s: requires %s\n"), alpm_miss_get_target(miss),
							depstring);
					free(depstring);
				}
				FREELIST(data);
				break;
			default:
				break;
		}
		trans_release();
		FREELIST(finaltargs);
		return(1);
	}

	/* Warn user in case of dangerous operation */
	if(config->flags & PM_TRANS_FLAG_RECURSE ||
	   config->flags & PM_TRANS_FLAG_CASCADE) {
		/* list transaction targets */
		alpm_list_t *lst = NULL;
		/* create a new list of package names only */
		for(i = alpm_trans_get_pkgs(); i; i = alpm_list_next(i)) {
			pmpkg_t *pkg = alpm_list_getdata(i);
			lst = alpm_list_add(lst, strdup(alpm_pkg_get_name(pkg)));
		}
		printf("\n");
		list_display(_("Targets:"), lst);
		FREELIST(lst);
		/* get confirmation */
		if(yesno(1, _("\nDo you want to remove these packages?")) == 0) {
			trans_release();
			FREELIST(finaltargs);
			return(1);
		}
		printf("\n");
	}

	/* Step 3: actually perform the removal */
	if(alpm_trans_commit(NULL) == -1) {
		fprintf(stderr, _("error: failed to commit transaction (%s)\n"),
		        alpm_strerrorlast());
		trans_release();
		FREELIST(finaltargs);
		return(1);
	}

	/* Step 4: release transaction resources */
	if(trans_release() == -1) {
		retval = 1;
	}
	FREELIST(finaltargs);
	return(retval);
}

/* vim: set ts=2 sw=2 noet: */
