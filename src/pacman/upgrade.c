/*
 *  upgrade.c
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
#include "conf.h"
#include "util.h"

/**
 * @brief Upgrade a specified list of packages.
 *
 * @param targets a list of packages (as strings) to upgrade
 *
 * @return 0 on success, 1 on failure
 */
int pacman_upgrade(alpm_list_t *targets)
{
	alpm_list_t *i, *data = NULL;
	pmtranstype_t transtype = PM_TRANS_TYPE_UPGRADE;
	int retval = 0;

	if(targets == NULL) {
		pm_printf(PM_LOG_ERROR, _("no targets specified (use -h for help)\n"));
		return(1);
	}

	/* Check for URL targets and process them
	 */
	for(i = targets; i; i = alpm_list_next(i)) {
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

	/* Step 1: create a new transaction */
	if(trans_init(transtype, config->flags) == -1) {
		return(1);
	}

	/* add targets to the created transaction */
	printf(_("loading package data...\n"));
	for(i = targets; i; i = alpm_list_next(i)) {
		char *targ = alpm_list_getdata(i);
		if(alpm_trans_addtarget(targ) == -1) {
			pm_fprintf(stderr, PM_LOG_ERROR, "'%s': %s\n",
					targ, alpm_strerrorlast());
			trans_release();
			return(1);
		}
	}

	/* Step 2: "compute" the transaction based on targets and flags */
	/* TODO: No, compute nothing. This is stupid. */
	if(alpm_trans_prepare(&data) == -1) {
		pm_fprintf(stderr, PM_LOG_ERROR, _("failed to prepare transaction (%s)\n"),
		        alpm_strerrorlast());
		switch(pm_errno) {
			case PM_ERR_UNSATISFIED_DEPS:
				for(i = data; i; i = alpm_list_next(i)) {
					pmdepmissing_t *miss = alpm_list_getdata(i);
					pmdepend_t *dep = alpm_miss_get_dep(miss);
					char *depstring = alpm_dep_get_string(dep);

					/* TODO indicate if the error was a virtual package or not:
					 *		:: %s: requires %s, provided by %s
					 */
					printf(_(":: %s: requires %s\n"), alpm_miss_get_target(miss),
							depstring);
					free(depstring);
				}
				break;
			case PM_ERR_CONFLICTING_DEPS:
				for(i = data; i; i = alpm_list_next(i)) {
					pmconflict_t *conflict = alpm_list_getdata(i);
					printf(_(":: %s: conflicts with %s\n"),
						alpm_conflict_get_package1(conflict), alpm_conflict_get_package2(conflict));
				}
				break;
			case PM_ERR_FILE_CONFLICTS:
				for(i = data; i; i = alpm_list_next(i)) {
					pmfileconflict_t *conflict = alpm_list_getdata(i);
					switch(alpm_fileconflict_get_type(conflict)) {
						case PM_FILECONFLICT_TARGET:
							printf(_("%s exists in both '%s' and '%s'\n"),
							        alpm_fileconflict_get_file(conflict),
							        alpm_fileconflict_get_target(conflict),
							        alpm_fileconflict_get_ctarget(conflict));
						break;
						case PM_FILECONFLICT_FILESYSTEM:
							printf(_("%s: %s exists in filesystem\n"),
							        alpm_fileconflict_get_target(conflict),
							        alpm_fileconflict_get_file(conflict));
						break;
					}
				}
				printf(_("\nerrors occurred, no packages were upgraded.\n"));
				break;
			default:
				break;
		}
		trans_release();
		FREELIST(data);
		return(1);
	}
	alpm_list_free(data);

	/* Step 3: perform the installation */
	if(alpm_trans_commit(NULL) == -1) {
		pm_fprintf(stderr, PM_LOG_ERROR, _("failed to commit transaction (%s)\n"),
			alpm_strerrorlast());
		trans_release();
		return(1);
	}

	if(trans_release() == -1) {
		retval = 1;
	}
	return(retval);
}

/* vim: set ts=2 sw=2 noet: */
