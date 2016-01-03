/*
 *  upgrade.c
 *
 *  Copyright (c) 2006-2016 Pacman Development Team <pacman-dev@archlinux.org>
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
	int retval = 0, *file_is_remote;
	alpm_list_t *i;
	unsigned int n, num_targets;

	if(targets == NULL) {
		pm_printf(ALPM_LOG_ERROR, _("no targets specified (use -h for help)\n"));
		return 1;
	}

	num_targets = alpm_list_count(targets);

	/* Check for URL targets and process them */
	file_is_remote = malloc(num_targets * sizeof(int));
	if(file_is_remote == NULL) {
		pm_printf(ALPM_LOG_ERROR, _("memory exhausted\n"));
		return 1;
	}

	for(i = targets, n = 0; i; i = alpm_list_next(i), n++) {
		if(strstr(i->data, "://")) {
			char *str = alpm_fetch_pkgurl(config->handle, i->data);
			if(str == NULL) {
				pm_printf(ALPM_LOG_ERROR, "'%s': %s\n",
						(char *)i->data, alpm_strerror(alpm_errno(config->handle)));
				retval = 1;
			} else {
				free(i->data);
				i->data = str;
				file_is_remote[n] = 1;
			}
		} else {
			file_is_remote[n] = 0;
		}
	}

	if(retval) {
		goto fail_free;
	}

	/* Step 1: create a new transaction */
	if(trans_init(config->flags, 1) == -1) {
		retval = 1;
		goto fail_free;
	}

	printf(_("loading packages...\n"));
	/* add targets to the created transaction */
	for(i = targets, n = 0; i; i = alpm_list_next(i), n++) {
		const char *targ = i->data;
		alpm_pkg_t *pkg;
		alpm_siglevel_t level;

		if(file_is_remote[n]) {
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

	if(retval) {
		goto fail_release;
	}

	free(file_is_remote);

	/* now that targets are resolved, we can hand it all off to the sync code */
	return sync_prepare_execute();

fail_release:
	trans_release();
fail_free:
	free(file_is_remote);

	return retval;
}

/* vim: set noet: */
