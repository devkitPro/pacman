/*
 *  add.c
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 *  USA.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "pacman.h"
#include "callback.h"
#include "conf.h"
#include "util.h"

/* Free the current transaction and print an error if unsuccessful */
static int add_cleanup(void)
{
	int ret = alpm_trans_release();
	if(ret != 0) {
		pm_printf(PM_LOG_ERROR, _("failed to release transaction (%s)\n"),
		        alpm_strerrorlast());
		ret = 1;
	}

	return(ret);
}

/**
 * @brief Upgrade a specified list of packages.
 *
 * @param targets a list of packages (as strings) to upgrade
 *
 * @return 0 on success, 1 on failure
 */
int pacman_upgrade(alpm_list_t *targets)
{
	/* this is basically just a remove-then-add process. pacman_add() will */
	/* handle it */
	config->upgrade = 1;
	return(pacman_add(targets));
}

/**
 * @brief Add a specified list of packages which cannot already be installed.
 *
 * @param targets a list of packages (as strings) to add
 *
 * @return 0 on success, 1 on failure
 */
int pacman_add(alpm_list_t *targets)
{
	alpm_list_t *i, *data = NULL;
	pmtranstype_t transtype = PM_TRANS_TYPE_ADD;
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
	if(config->upgrade == 1) {
		/* if upgrade flag was set, change this to an upgrade transaction */
		transtype = PM_TRANS_TYPE_UPGRADE;
	}

	if(alpm_trans_init(transtype, config->flags, cb_trans_evt,
	   cb_trans_conv, cb_trans_progress) == -1) {
		/* TODO: error messages should be in the front end, not the back */
		fprintf(stderr, _("error: %s\n"), alpm_strerrorlast());
		if(pm_errno == PM_ERR_HANDLE_LOCK) {
			/* TODO this and the 2 other places should probably be on stderr */
			printf(_("  if you're sure a package manager is not already\n"
			         "  running, you can remove %s.\n"), alpm_option_get_lockfile());
		}
		return(1);
	}

	/* add targets to the created transaction */
	printf(_("loading package data... "));
	for(i = targets; i; i = alpm_list_next(i)) {
		char *targ = alpm_list_getdata(i);
		if(alpm_trans_addtarget(targ) == -1) {
			fprintf(stderr, _("error: failed to add target '%s' (%s)"), targ,
			        alpm_strerrorlast());
			add_cleanup();
			return(1);
		}
	}
	printf(_("done.\n"));

	/* Step 2: "compute" the transaction based on targets and flags */
	/* TODO: No, compute nothing. This is stupid. */
	if(alpm_trans_prepare(&data) == -1) {
		fprintf(stderr, _("error: failed to prepare transaction (%s)\n"),
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
					pmdepmissing_t *miss = alpm_list_getdata(i);
					pmdepend_t *dep = alpm_miss_get_dep(miss);
					printf(_(":: %s: conflicts with %s"),
						alpm_miss_get_target(miss), alpm_dep_get_name(dep));
				}
				break;
			case PM_ERR_FILE_CONFLICTS:
				for(i = data; i; i = alpm_list_next(i)) {
					pmconflict_t *conflict = alpm_list_getdata(i);
					switch(alpm_conflict_get_type(conflict)) {
						case PM_CONFLICT_TYPE_TARGET:
							printf(_("%s exists in both '%s' and '%s'\n"),
							        alpm_conflict_get_file(conflict),
							        alpm_conflict_get_target(conflict),
							        alpm_conflict_get_ctarget(conflict));
						break;
						case PM_CONFLICT_TYPE_FILE:
							printf(_("%s: %s exists in filesystem\n"),
							        alpm_conflict_get_target(conflict),
							        alpm_conflict_get_file(conflict));
						break;
					}
				}
				printf(_("\nerrors occurred, no packages were upgraded.\n"));
				break;
			default:
				break;
		}
		add_cleanup();
		alpm_list_free(data);
		return(1);
	}
	alpm_list_free(data);

	/* Step 3: perform the installation */
	if(alpm_trans_commit(NULL) == -1) {
		fprintf(stderr, _("error: failed to commit transaction (%s)\n"), alpm_strerrorlast());
		add_cleanup();
		return(1);
	}

	retval = add_cleanup();
	return(retval);
}

/* vim: set ts=2 sw=2 noet: */
