/*
 *  upgrade.c
 *
 *  Copyright (c) 2006-2014 Pacman Development Team <pacman-dev@archlinux.org>
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
	int retval = 0;
	alpm_list_t *i, *j, *remote = NULL;

	if(targets == NULL) {
		pm_printf(ALPM_LOG_ERROR, _("no targets specified (use -h for help)\n"));
		return 1;
	}

	/* Check for URL targets and process them
	 */
	for(i = targets; i; i = alpm_list_next(i)) {
		int *r = malloc(sizeof(int));

		if(strstr(i->data, "://")) {
			char *str = alpm_fetch_pkgurl(config->handle, i->data);
			if(str == NULL) {
				pm_printf(ALPM_LOG_ERROR, "'%s': %s\n",
						(char *)i->data, alpm_strerror(alpm_errno(config->handle)));
				retval = 1;
			} else {
				free(i->data);
				i->data = str;
				*r = 1;
			}
		} else {
			*r = 0;
		}

		remote = alpm_list_add(remote, r);
	}

	if(retval) {
		return retval;
	}

	/* Step 1: create a new transaction */
	if(trans_init(config->flags, 1) == -1) {
		return 1;
	}

	printf(_("loading packages...\n"));
	/* add targets to the created transaction */
	for(i = targets, j = remote; i; i = alpm_list_next(i), j = alpm_list_next(j)) {
		const char *targ = i->data;
		alpm_pkg_t *pkg;
		alpm_siglevel_t level;

		if(*(int *)j->data) {
			level = alpm_option_get_remote_file_siglevel(config->handle);
		} else {
			level = alpm_option_get_local_file_siglevel(config->handle);
		}

		if(alpm_pkg_load(config->handle, targ, 1, level, &pkg) != 0) {
			pm_printf(ALPM_LOG_ERROR, "'%s': %s\n",
					targ, alpm_strerror(alpm_errno(config->handle)));
			retval = 1;
			continue;
		}
		if(alpm_add_pkg(config->handle, pkg) == -1) {
			pm_printf(ALPM_LOG_ERROR, "'%s': %s\n",
					targ, alpm_strerror(alpm_errno(config->handle)));
			alpm_pkg_free(pkg);
			retval = 1;
			continue;
		}
		config->explicit_adds = alpm_list_add(config->explicit_adds, pkg);
	}

	FREELIST(remote);

	if(retval) {
		trans_release();
		return retval;
	}

	/* now that targets are resolved, we can hand it all off to the sync code */
	return sync_prepare_execute();
}

/* vim: set ts=2 sw=2 noet: */
