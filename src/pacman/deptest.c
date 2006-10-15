/*
 *  deptest.c
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
/* pacman */
#include "util.h"
#include "list.h"
#include "conf.h"
#include "log.h"
#include "sync.h"
#include "deptest.h"

extern config_t *config;

int pacman_deptest(list_t *targets)
{
	PM_LIST *data;
	list_t *i;
	char *str;
	int retval = 0;

	if(targets == NULL) {
		return(0);
	}

	if(config->op_d_vertest) {
		if(targets->data && targets->next && targets->next->data) {
			int ret = alpm_pkg_vercmp(targets->data, targets->next->data);
			printf("%d\n", ret);
			return(ret);
		}
		return(0);
	}

	/* we create a transaction to hold a dummy package to be able to use
	 * deps checkings from alpm_trans_prepare() */
	if(alpm_trans_init(PM_TRANS_TYPE_ADD, 0, NULL, NULL, NULL) == -1) {
		ERR(NL, "%s", alpm_strerror(pm_errno));
		if(pm_errno == PM_ERR_HANDLE_LOCK) {
			MSG(NL, _("       if you're sure a package manager is not already running,\n"
			  			"       you can remove %s%s\n"), config->root, PM_LOCK);
		}
		return(1);
	}

	/* We use a hidden facility from alpm_trans_addtarget() to add a dummy
	 * target to the transaction (see the library code for details).
	 * It allows us to use alpm_trans_prepare() to check dependencies of the
	 * given target.
	 */
	str = (char *)malloc(strlen("name=dummy|version=1.0-1")+1);
	if(str == NULL) {
		ERR(NL, _("memory allocation failure\n"));
		retval = 1;
		goto cleanup;
	}
	strcpy(str, "name=dummy|version=1.0-1");
	for(i = targets; i; i = i->next) {
		str = (char *)realloc(str, strlen(str)+8+strlen(i->data)+1);
		strcat(str, "|depend=");
		strcat(str, i->data);
	}
	vprint(_("add target %s\n"), str);
	if(alpm_trans_addtarget(str) == -1) {
		FREE(str);
		ERR(NL, _("could not add target (%s)\n"), alpm_strerror(pm_errno));
		retval = 1;
		goto cleanup;
	}
	FREE(str);

	if(alpm_trans_prepare(&data) == -1) {
		PM_LIST *lp;
		list_t *synctargs = NULL;
		retval = 126;
		/* return 126 = deps were missing, but successfully resolved
		 * return 127 = deps were missing, and failed to resolve; OR
		 *            = deps were missing, but no resolution was attempted; OR
		 *            = unresolvable conflicts were found
		 */
		switch(pm_errno) {
			case PM_ERR_UNSATISFIED_DEPS:
				for(lp = alpm_list_first(data); lp; lp = alpm_list_next(lp)) {
					PM_DEPMISS *miss = alpm_list_getdata(lp);
					if(!config->op_d_resolve) {
						MSG(NL, _("requires: %s"), alpm_dep_getinfo(miss, PM_DEP_NAME));
						switch((long)alpm_dep_getinfo(miss, PM_DEP_MOD)) {
							case PM_DEP_MOD_EQ: MSG(CL, "=%s", alpm_dep_getinfo(miss, PM_DEP_VERSION));  break;
							case PM_DEP_MOD_GE: MSG(CL, ">=%s", alpm_dep_getinfo(miss, PM_DEP_VERSION)); break;
							case PM_DEP_MOD_LE: MSG(CL, "<=%s", alpm_dep_getinfo(miss, PM_DEP_VERSION)); break;
						}
						MSG(CL, "\n");
					}
					synctargs = list_add(synctargs, strdup(alpm_dep_getinfo(miss, PM_DEP_NAME)));
				}
				alpm_list_free(data);
			break;
			case PM_ERR_CONFLICTING_DEPS:
				/* we can't auto-resolve conflicts */
				for(lp = alpm_list_first(data); lp; lp = alpm_list_next(lp)) {
					PM_DEPMISS *miss = alpm_list_getdata(lp);
					MSG(NL, _("conflict: %s"), alpm_dep_getinfo(miss, PM_DEP_NAME));
				}
				retval = 127;
				alpm_list_free(data);
			break;
			default:
				retval = 127;
			break;
		}

		/* attempt to resolve missing dependencies */
		/* TODO: handle version comparators (eg, glibc>=2.2.5) */
		if(retval == 126 && synctargs != NULL) {
			if(alpm_trans_release() == -1) {
				ERR(NL, _("could not release transaction (%s)"), alpm_strerror(pm_errno));
				FREELIST(synctargs);
				return(1);
			}
			if(!config->op_d_resolve || pacman_sync(synctargs) != 0) {
				/* error (or -D not used) */
				retval = 127;
			}
			FREELIST(synctargs);
			return(retval);
		}

		FREELIST(synctargs);
	}

cleanup:
	if(!config->op_d_resolve) {
		if(alpm_trans_release() == -1) {
			ERR(NL, _("could not release transaction (%s)"), alpm_strerror(pm_errno));
			retval = 1;
		}
	}

	return(retval);
}

/* vim: set ts=2 sw=2 noet: */
