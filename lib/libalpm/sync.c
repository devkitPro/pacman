/*
 *  sync.c
 * 
 *  Copyright (c) 2002-2005 by Judd Vinet <jvinet@zeroflux.org>
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
/* pacman */
#include "log.h"
#include "util.h"
#include "list.h"
#include "package.h"
#include "db.h"
#include "cache.h"
#include "deps.h"
#include "trans.h"
#include "sync.h"
#include "rpmvercmp.h"
#include "handle.h"

extern pmhandle_t *handle;

pmsync_t *sync_new(int type, pmpkg_t *lpkg, pmpkg_t *spkg)
{
	pmsync_t *sync;

	if((sync = (pmsync_t *)malloc(sizeof(pmsync_t))) == NULL) {
		return(NULL);
	}

	sync->type = type;
	sync->lpkg = lpkg;
	sync->spkg = spkg;
	
	return(sync);
}

int sync_sysupgrade(PMList **data)
{
	PMList *i, *j, *k;
	PMList *targets = NULL;

	*data = NULL;

	/* check for "recommended" package replacements */
	for(i = handle->dbs_sync; i; i = i->next) {
		PMList *j;

		for(j = db_get_pkgcache(i->data); j; j = j->next) {
			pmpkg_t *spkg = j->data;

			for(k = spkg->replaces; k; k = k->next) {
				PMList *m;

				for(m = db_get_pkgcache(handle->db_local); m; m = m->next) {
					pmpkg_t *lpkg = m->data;

					if(!strcmp(k->data, lpkg->name)) {
						if(pm_list_is_strin(lpkg->name, handle->ignorepkg)) {
							_alpm_log(PM_LOG_WARNING, "%s-%s: ignoring package upgrade (to be replaced by %s-%s)",
								lpkg->name, lpkg->version, spkg->name, spkg->version);
						} else {
							pmsync_t *sync = sync_new(PM_SYSUPG_REPLACE, lpkg, spkg);

							if(sync == NULL) {
								pm_errno = PM_ERR_MEMORY;
								goto error;
							}

							targets = pm_list_add(targets, sync);
						}
					}
				}
			}
		}
	}

	/* match installed packages with the sync dbs and compare versions */
	for(i = db_get_pkgcache(handle->db_local); i; i = i->next) {
		int cmp;
		pmpkg_t *local = i->data;
		pmpkg_t *spkg = NULL;
		pmsync_t *sync;

		for(j = handle->dbs_sync; !spkg && j; j = j->next) {

			for(k = db_get_pkgcache(j->data); !spkg && k; k = k->next) {
				pmpkg_t *sp = k->data;

				if(!strcmp(local->name, sp->name)) {
					spkg = sp;
				}
			}
		}
		if(spkg == NULL) {
			/*fprintf(stderr, "%s: not found in sync db.  skipping.", local->name);*/
			continue;
		}

		/* compare versions and see if we need to upgrade */
		cmp = rpmvercmp(local->version, spkg->version);
		if(cmp > 0 && !spkg->force) {
			/* local version is newer */
			_alpm_log(PM_LOG_FLOW1, "%s-%s: local version is newer",
				local->name, local->version);
			continue;
		} else if(cmp == 0) {
			/* versions are identical */
			continue;
		} else if(pm_list_is_strin(i->data, handle->ignorepkg)) {
			/* package should be ignored (IgnorePkg) */
			_alpm_log(PM_LOG_FLOW1, "%s-%s: ignoring package upgrade (%s)",
				local->name, local->version, spkg->version);
			continue;
		}

		sync = sync_new(PM_SYSUPG_UPGRADE, local, spkg);
		if(sync == NULL) {
			pm_errno = PM_ERR_MEMORY;
			goto error;
		}

		targets = pm_list_add(targets, sync);
	}

	*data = targets;

	return(0);

error:
	FREELIST(targets);
	return(-1);
}

int sync_resolvedeps(PMList **syncs)
{
	return(0);
}

int sync_prepare(pmdb_t *db, pmtrans_t *trans, PMList **data)
{
	PMList *i;
	PMList *trail = NULL;

	/* Resolve targets dependencies */
	for(i = trans->targets; i; i = i->next) {
		if(resolvedeps(handle->db_local, handle->dbs_sync, i->data, trans->targets, trail, data) == -1) {
			/* pm_errno is set by resolvedeps */
			goto error;
		}
	}

	/* ORE
	check for inter-conflicts and whatnot */

	/* ORE
	any packages in rmtargs need to be removed from final.
	rather than ripping out nodes from final, we just copy over
	our "good" nodes to a new list and reassign. */

	/* ORE
	Check dependencies of packages in rmtargs and make sure
	we won't be breaking anything by removing them.
	If a broken dep is detected, make sure it's not from a
	package that's in our final (upgrade) list. */

	return(0);

error:
	return(-1);
}

int sync_commit(pmdb_t *db, pmtrans_t *trans)
{
	PMList *i, *files = NULL;
	PMList *final = NULL;
	PMList *data;
	pmtrans_t *tr;

	/* remove any conflicting packages (WITHOUT dep checks) */

	/* remove to-be-replaced packages */

	/* install targets */
	tr = trans_new(PM_TRANS_TYPE_UPGRADE, 0);
	for(i = files; i; i = i->next) {
		trans_addtarget(tr, i->data);
	}

	trans_prepare(tr, &data);

	trans_commit(tr);

	trans_free(tr);

	/* propagate replaced packages' requiredby fields to their new owners */
	for(i = final; i; i = i->next) {
		/*syncpkg_t *sync = (syncpkg_t*)i->data;
		if(sync->replaces) {
			pkginfo_t *new = db_scan(db, sync->pkg->name, INFRQ_DEPENDS);
			for(j = sync->replaces; j; j = j->next) {
				pkginfo_t *old = (pkginfo_t*)j->data;
				// merge lists
				for(k = old->requiredby; k; k = k->next) {
					if(!is_in(k->data, new->requiredby)) {
						// replace old's name with new's name in the requiredby's dependency list
						PMList *m;
						pkginfo_t *depender = db_scan(db, k->data, INFRQ_DEPENDS);
						for(m = depender->depends; m; m = m->next) {
							if(!strcmp(m->data, old->name)) {
								FREE(m->data);
								m->data = strdup(new->name);
							}
						}
						db_write(db, depender, INFRQ_DEPENDS);

						// add the new requiredby
						new->requiredby = list_add(new->requiredby, strdup(k->data));
					}
				}
			}
			db_write(db, new, INFRQ_DEPENDS);
			FREEPKG(new);
		}*/
	}

	/* cache needs to be rebuilt */
	db_free_pkgcache(db);

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
