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
	int retval = 0;
	alpm_list_t *i, *data = NULL;

	if(targets == NULL) {
		pm_printf(PM_LOG_ERROR, _("no targets specified (use -h for help)\n"));
		return(1);
	}

	/* Step 0: create a new transaction */
	if(trans_init(PM_TRANS_TYPE_REMOVE, config->flags) == -1) {
		return(1);
	}

	/* Step 1: add targets to the created transaction */
	for(i = targets; i; i = alpm_list_next(i)) {
		char *targ = alpm_list_getdata(i);
		if(alpm_trans_addtarget(targ) == -1) {
			if(pm_errno == PM_ERR_PKG_NOT_FOUND) {
				printf(_("%s not found, searching for group...\n"), targ);
				pmgrp_t *grp = alpm_db_readgrp(db_local, targ);
				if(grp == NULL) {
					pm_fprintf(stderr, PM_LOG_ERROR, _("'%s': not found in local db\n"), targ);
					retval = 1;
					goto cleanup;
				} else {
					alpm_list_t *p, *pkgnames = NULL;
					/* convert packages to package names */
					for(p = alpm_grp_get_pkgs(grp); p; p = alpm_list_next(p)) {
						pmpkg_t *pkg = alpm_list_getdata(p);
						pkgnames = alpm_list_add(pkgnames, (void *)alpm_pkg_get_name(pkg));
					}
					printf(_(":: group %s:\n"), targ);
					list_display("   ", pkgnames);
					int all = yesno(1, _("    Remove whole content?"));
					for(p = pkgnames; p; p = alpm_list_next(p)) {
						char *pkgn = alpm_list_getdata(p);
						if(all || yesno(1, _(":: Remove %s from group %s?"), pkgn, targ)) {
							if(alpm_trans_addtarget(pkgn) == -1) {
								pm_fprintf(stderr, PM_LOG_ERROR, "'%s': %s\n", targ,
								           alpm_strerrorlast());
								retval = 1;
								alpm_list_free(pkgnames);
								goto cleanup;
							}
						}
					}
					alpm_list_free(pkgnames);
				}
			} else {
				pm_fprintf(stderr, PM_LOG_ERROR, "'%s': %s\n", targ, alpm_strerrorlast());
				retval = 1;
				goto cleanup;
			}
		}
	}

	/* Step 2: prepare the transaction based on its type, targets and flags */
	if(alpm_trans_prepare(&data) == -1) {
		pm_fprintf(stderr, PM_LOG_ERROR, _("failed to prepare transaction (%s)\n"),
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
		retval = 1;
		goto cleanup;
	}

	/* Warn user in case of dangerous operation */
	if(config->flags & PM_TRANS_FLAG_RECURSE ||
	   config->flags & PM_TRANS_FLAG_CASCADE) {
		/* list transaction targets */
		alpm_list_t *pkglist = alpm_trans_get_pkgs();

		display_targets(pkglist, 0);
		printf("\n");

		/* get confirmation */
		if(yesno(1, _("Do you want to remove these packages?")) == 0) {
			retval = 1;
			goto cleanup;
		}
	}

	/* Step 3: actually perform the removal */
	if(alpm_trans_commit(NULL) == -1) {
		pm_fprintf(stderr, PM_LOG_ERROR, _("failed to commit transaction (%s)\n"),
		        alpm_strerrorlast());
		retval = 1;
	}

	/* Step 4: release transaction resources */
cleanup:
	if(trans_release() == -1) {
		retval = 1;
	}
	return(retval);
}

/* vim: set ts=2 sw=2 noet: */
