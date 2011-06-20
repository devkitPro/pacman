/*
 *  remove.c
 *
 *  Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
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

#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "pacman.h"
#include "util.h"
#include "conf.h"

static int remove_target(const char *target)
{
	pmpkg_t *info;
	pmdb_t *db_local = alpm_option_get_localdb(config->handle);
	alpm_list_t *p;

	if((info = alpm_db_get_pkg(db_local, target)) != NULL) {
		if(alpm_remove_pkg(config->handle, info) == -1) {
			pm_fprintf(stderr, PM_LOG_ERROR, "'%s': %s\n", target,
					alpm_strerror(alpm_errno(config->handle)));
			return -1;
		}
		return 0;
	}

		/* fallback to group */
	pmgrp_t *grp = alpm_db_readgrp(db_local, target);
	if(grp == NULL) {
		pm_fprintf(stderr, PM_LOG_ERROR, "'%s': target not found\n", target);
		return -1;
	}
	for(p = grp->packages; p; p = alpm_list_next(p)) {
		pmpkg_t *pkg = alpm_list_getdata(p);
		if(alpm_remove_pkg(config->handle, pkg) == -1) {
			pm_fprintf(stderr, PM_LOG_ERROR, "'%s': %s\n", target,
					alpm_strerror(alpm_errno(config->handle)));
			return -1;
		}
	}
	return 0;
}

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
		return 1;
	}

	/* Step 0: create a new transaction */
	if(trans_init(config->flags) == -1) {
		return 1;
	}

	/* Step 1: add targets to the created transaction */
	for(i = targets; i; i = alpm_list_next(i)) {
		char *target = alpm_list_getdata(i);
		char *targ = strchr(target, '/');
		if(targ && strncmp(target, "local", 5) == 0) {
			targ++;
		} else {
			targ = target;
		}
		if(remove_target(targ) == -1) {
			retval = 1;
			goto cleanup;
		}
	}

	/* Step 2: prepare the transaction based on its type, targets and flags */
	if(alpm_trans_prepare(config->handle, &data) == -1) {
		enum _pmerrno_t err = alpm_errno(config->handle);
		pm_fprintf(stderr, PM_LOG_ERROR, _("failed to prepare transaction (%s)\n"),
		        alpm_strerror(err));
		switch(err) {
			case PM_ERR_PKG_INVALID_ARCH:
				for(i = data; i; i = alpm_list_next(i)) {
					char *pkg = alpm_list_getdata(i);
					printf(_(":: package %s does not have a valid architecture\n"), pkg);
				}
				break;
			case PM_ERR_UNSATISFIED_DEPS:
				for(i = data; i; i = alpm_list_next(i)) {
					pmdepmissing_t *miss = alpm_list_getdata(i);
					char *depstring = alpm_dep_compute_string(miss->depend);
					printf(_(":: %s: requires %s\n"), miss->target, depstring);
					free(depstring);
				}
				break;
			default:
				break;
		}
		FREELIST(data);
		retval = 1;
		goto cleanup;
	}

	/* Search for holdpkg in target list */
	int holdpkg = 0;
	for(i = alpm_trans_get_remove(config->handle); i; i = alpm_list_next(i)) {
		pmpkg_t *pkg = alpm_list_getdata(i);
		if(alpm_list_find_str(config->holdpkg, alpm_pkg_get_name(pkg))) {
			pm_printf(PM_LOG_WARNING, _("%s is designated as a HoldPkg.\n"),
							alpm_pkg_get_name(pkg));
			holdpkg = 1;
		}
	}
	if(holdpkg && (noyes(_("HoldPkg was found in target list. Do you want to continue?")) == 0)) {
		retval = 1;
		goto cleanup;
	}

	/* Step 3: actually perform the removal */
	alpm_list_t *pkglist = alpm_trans_get_remove(config->handle);
	if(pkglist == NULL) {
		printf(_(" there is nothing to do\n"));
		goto cleanup; /* we are done */
	}

	if(config->print) {
		print_packages(pkglist);
		goto cleanup;
	}

	/* print targets and ask user confirmation */
	display_targets(pkglist, 0);
	printf("\n");
	if(yesno(_("Do you want to remove these packages?")) == 0) {
		retval = 1;
		goto cleanup;
	}

	if(alpm_trans_commit(config->handle, &data) == -1) {
		pm_fprintf(stderr, PM_LOG_ERROR, _("failed to commit transaction (%s)\n"),
		        alpm_strerror(alpm_errno(config->handle)));
		retval = 1;
	}

	FREELIST(data);

	/* Step 4: release transaction resources */
cleanup:
	if(trans_release() == -1) {
		retval = 1;
	}
	return retval;
}

/* vim: set ts=2 sw=2 noet: */
