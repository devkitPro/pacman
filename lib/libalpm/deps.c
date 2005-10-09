/*
 *  deps.c
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/* pacman */
#include "util.h"
#include "log.h"
#include "error.h"
#include "list.h"
#include "package.h"
#include "db.h"
#include "cache.h"
#include "provide.h"
#include "deps.h"
#include "rpmvercmp.h"
#include "handle.h"

extern pmhandle_t *handle;

/* Re-order a list of target packages with respect to their dependencies.
 *
 * Example (PM_TRANS_TYPE_ADD):
 *   A depends on C
 *   B depends on A
 *   Target order is A,B,C,D
 *
 *   Should be re-ordered to C,A,B,D
 * 
 * mode should be either PM_TRANS_TYPE_ADD or PM_TRANS_TYPE_REMOVE.  This
 * affects the dependency order sortbydeps() will use.
 *
 * This function returns the new PMList* target list.
 *
 */ 
PMList *sortbydeps(PMList *targets, int mode)
{
	PMList *newtargs = NULL;
	PMList *i, *j, *k;
	int change = 1;
	int numscans = 0;
	int numtargs = 0;

	if(targets == NULL) {
		return(NULL);
	}

	for(i = targets; i; i = i->next) {
		newtargs = pm_list_add(newtargs, i->data);
		numtargs++;
	}

	while(change) {
		PMList *tmptargs = NULL;
		change = 0;
		if(numscans > numtargs) {
			_alpm_log(PM_LOG_WARNING, "possible dependency cycle detected");
			continue;
		}
		numscans++;
		/* run thru targets, moving up packages as necessary */
		for(i = newtargs; i; i = i->next) {
			pmpkg_t *p = (pmpkg_t*)i->data;
			for(j = p->depends; j; j = j->next) {
				pmdepend_t dep;
				pmpkg_t *q = NULL;
				if(splitdep(j->data, &dep)) {
					continue;
				}
				/* look for dep.name -- if it's farther down in the list, then
				 * move it up above p
				 */
				for(k = i->next; k; k = k->next) {
					q = (pmpkg_t *)k->data;
					if(!strcmp(dep.name, q->name)) {
						if(!pkg_isin(q, tmptargs)) {
							change = 1;
							tmptargs = pm_list_add(tmptargs, q);
						}
						break;
					}
				}
			}
			if(!pkg_isin(p, tmptargs)) {
				tmptargs = pm_list_add(tmptargs, p);
			}
		}
		FREELISTPTR(newtargs);
		newtargs = tmptargs;
	}

	if(mode == PM_TRANS_TYPE_REMOVE) {
		/* we're removing packages, so reverse the order */
		PMList *tmptargs = _alpm_list_reverse(newtargs);
		/* free the old one */
		FREELISTPTR(newtargs);
		newtargs = tmptargs;
	}

	return(newtargs);
}

/* Returns a PMList* of missing_t pointers.
 *
 * conflicts are always name only, but dependencies can include versions
 * with depmod operators.
 *
 */
PMList *checkdeps(pmdb_t *db, unsigned char op, PMList *packages)
{
	pmpkg_t *info = NULL;
	pmdepend_t depend;
	PMList *i, *j, *k;
	int cmp;
	int found = 0;
	PMList *baddeps = NULL;
	pmdepmissing_t *miss = NULL;

	if(db == NULL) {
		return(NULL);
	}

	if(op == PM_TRANS_TYPE_UPGRADE) {
		/* PM_TRANS_TYPE_UPGRADE handles the backwards dependencies, ie, the packages
		 * listed in the requiredby field.
		 */
		for(i = packages; i; i = i->next) {
			pmpkg_t *tp = i->data;
			pmpkg_t *oldpkg;
			if(tp == NULL) {
				continue;
			}

			if((oldpkg = db_get_pkgfromcache(db, tp->name)) == NULL) {
				continue;
			}
			for(j = oldpkg->requiredby; j; j = j->next) {
				char *ver;
				pmpkg_t *p;
				found = 0;
				if((p = db_get_pkgfromcache(db, j->data)) == NULL) {
					/* hmmm... package isn't installed.. */
					continue;
				}
				if(pkg_isin(p, packages)) {
					/* this package is also in the upgrade list, so don't worry about it */
					continue;
				}
				for(k = p->depends; k && !found; k = k->next) {
					/* find the dependency info in p->depends */
					splitdep(k->data, &depend);
					if(!strcmp(depend.name, oldpkg->name)) {
						found = 1;
					}
				}
				if(found == 0) {
					/* look for packages that list depend.name as a "provide" */
					PMList *provides = _alpm_db_whatprovides(db, depend.name);
					if(provides == NULL) {
						/* not found */
						continue;
					}
					/* we found an installed package that provides depend.name */
					FREELISTPTR(provides);
				}
				found = 0;
				if(depend.mod == PM_DEP_MOD_ANY) {
					found = 1;
				} else {
					/* note that we use the version from the NEW package in the check */
					ver = strdup(tp->version);
					if(!index(depend.version,'-')) {
						char *ptr;
						for(ptr = ver; *ptr != '-'; ptr++);
						*ptr = '\0';
					}
					cmp = rpmvercmp(ver, depend.version);
					switch(depend.mod) {
						case PM_DEP_MOD_EQ: found = (cmp == 0); break;
						case PM_DEP_MOD_GE: found = (cmp >= 0); break;
						case PM_DEP_MOD_LE: found = (cmp <= 0); break;
					}
					FREE(ver);
				}
				if(!found) {
					MALLOC(miss, sizeof(pmdepmissing_t));
					miss->type = PM_DEP_TYPE_REQUIRED;
					miss->depend.mod = depend.mod;
					STRNCPY(miss->target, p->name, PKG_NAME_LEN);
					STRNCPY(miss->depend.name, depend.name, PKG_NAME_LEN);
					STRNCPY(miss->depend.version, depend.version, PKG_VERSION_LEN);
					if(!pm_list_is_in(miss, baddeps)) {
						baddeps = pm_list_add(baddeps, miss);
					} else {
						FREE(miss);
					}
				}
			}
		}
	}
	if(op == PM_TRANS_TYPE_ADD || op == PM_TRANS_TYPE_UPGRADE) {
		for(i = packages; i; i = i->next) {
			pmpkg_t *tp = i->data;
			if(tp == NULL) {
				continue;
			}

			/* CONFLICTS */
			for(j = tp->conflicts; j; j = j->next) {
				/* check targets against database */
				for(k = db_get_pkgcache(db); k; k = k->next) {
					pmpkg_t *dp = (pmpkg_t *)k->data;
					if(!strcmp(j->data, dp->name)) {
						MALLOC(miss, sizeof(pmdepmissing_t));
						miss->type = PM_DEP_TYPE_CONFLICT;
						miss->depend.mod = PM_DEP_MOD_ANY;
						miss->depend.version[0] = '\0';
						STRNCPY(miss->target, tp->name, PKG_NAME_LEN);
						STRNCPY(miss->depend.name, dp->name, PKG_NAME_LEN);
						if(!pm_list_is_in(miss, baddeps)) {
							baddeps = pm_list_add(baddeps, miss);
						} else {
							FREE(miss);
						}
					}
				}
				/* check targets against targets */
				for(k = packages; k; k = k->next) {
					pmpkg_t *a = (pmpkg_t *)k->data;
					if(!strcmp(a->name, (char *)j->data)) {
						MALLOC(miss, sizeof(pmdepmissing_t));
						miss->type = PM_DEP_TYPE_CONFLICT;
						miss->depend.mod = PM_DEP_MOD_ANY;
						miss->depend.version[0] = '\0';
						STRNCPY(miss->target, tp->name, PKG_NAME_LEN);
						STRNCPY(miss->depend.name, a->name, PKG_NAME_LEN);
						if(!pm_list_is_in(miss, baddeps)) {
							baddeps = pm_list_add(baddeps, miss);
						} else {
							FREE(miss);
						}
					}
				}
			}
			/* check database against targets */
			for(k = db_get_pkgcache(db); k; k = k->next) {
				info = k->data;
				for(j = info->conflicts; j; j = j->next) {
					if(!strcmp((char *)j->data, tp->name)) {
						MALLOC(miss, sizeof(pmdepmissing_t));
						miss->type = PM_DEP_TYPE_CONFLICT;
						miss->depend.mod = PM_DEP_MOD_ANY;
						miss->depend.version[0] = '\0';
						STRNCPY(miss->target, tp->name, PKG_NAME_LEN);
						STRNCPY(miss->depend.name, info->name, PKG_NAME_LEN);
						if(!pm_list_is_in(miss, baddeps)) {
							baddeps = pm_list_add(baddeps, miss);
						} else {
							FREE(miss);
						}
					}
				}
			}

			/* PROVIDES -- check to see if another package already provides what
			 *             we offer
 			 */
			/* XXX: disabled -- we allow multiple packages to provide the same thing.
			 *      list packages in conflicts if they really do conflict.
			for(j = tp->provides; j; j = j->next) {
				PMList *provs = whatprovides(db, j->data);
				for(k = provs; k; k = k->next) {
					if(!strcmp(tp->name, k->data->name)) {
						// this is the same package -- skip it
						continue;
					}
					// we treat this just like a conflict
					MALLOC(miss, sizeof(pmdepmissing_t));
					miss->type = PM_DEP_TYPE_CONFLICT;
					miss->depend.mod = PM_DEP_MOD_ANY;
					miss->depend.version[0] = '\0';
					STRNCPY(miss->target, tp->name, PKG_NAME_LEN);
					STRNCPY(miss->depend.name, k->data, PKG_NAME_LEN);
					if(!pm_list_is_in(baddeps, miss)) {
						baddeps = pm_list_add(baddeps, miss);
					}
					k->data = NULL;
				}
				FREELIST(provs);
			}*/

			/* DEPENDENCIES -- look for unsatisfied dependencies */
			for(j = tp->depends; j; j = j->next) {
				/* split into name/version pairs */
				splitdep((char *)j->data, &depend);
				found = 0;
				/* check database for literal packages */
				for(k = db_get_pkgcache(db); k && !found; k = k->next) {
					pmpkg_t *p = (pmpkg_t *)k->data;
					if(!strcmp(p->name, depend.name)) {
						if(depend.mod == PM_DEP_MOD_ANY) {
							/* accept any version */
							found = 1;
						} else {
							char *ver = strdup(p->version);
							/* check for a release in depend.version.  if it's
							 * missing remove it from p->version as well.
							 */
							if(!index(depend.version,'-')) {
								char *ptr;
								for(ptr = ver; *ptr != '-'; ptr++);
								*ptr = '\0';
							}
							cmp = rpmvercmp(ver, depend.version);
							switch(depend.mod) {
								case PM_DEP_MOD_EQ: found = (cmp == 0); break;
								case PM_DEP_MOD_GE: found = (cmp >= 0); break;
								case PM_DEP_MOD_LE: found = (cmp <= 0); break;
							}
							FREE(ver);
						}
					}
				}
				/* check other targets */
				for(k = packages; k && !found; k = k->next) {
					pmpkg_t *p = (pmpkg_t *)k->data;
					/* see if the package names match OR if p provides depend.name */
					if(!strcmp(p->name, depend.name) || pm_list_is_strin(depend.name, p->provides)) {
						if(depend.mod == PM_DEP_MOD_ANY) {
							/* accept any version */
							found = 1;
						} else {
							char *ver = strdup(p->version);
							/* check for a release in depend.version.  if it's
							 * missing remove it from p->version as well.
							 */
							if(!index(depend.version,'-')) {
								char *ptr;
								for(ptr = ver; *ptr != '-'; ptr++);
								*ptr = '\0';
							}
							cmp = rpmvercmp(ver, depend.version);
							switch(depend.mod) {
								case PM_DEP_MOD_EQ: found = (cmp == 0); break;
								case PM_DEP_MOD_GE: found = (cmp >= 0); break;
								case PM_DEP_MOD_LE: found = (cmp <= 0); break;
							}
							FREE(ver);
						}
					}
				}
				/* check database for provides matches */
				if(!found){
					k = _alpm_db_whatprovides(db, depend.name);
					if(k) {
						/* grab the first one (there should only really be one, anyway) */
						pmpkg_t *p = k->data;
						if(depend.mod == PM_DEP_MOD_ANY) {
							/* accept any version */
							found = 1;
						} else {
							char *ver = strdup(p->version);
							/* check for a release in depend.version.  if it's
							 * missing remove it from p->version as well.
							 */
							if(!index(depend.version,'-')) {
								char *ptr;
								for(ptr = ver; *ptr != '-'; ptr++);
								*ptr = '\0';
							}
							cmp = rpmvercmp(ver, depend.version);
							switch(depend.mod) {
								case PM_DEP_MOD_EQ: found = (cmp == 0); break;
								case PM_DEP_MOD_GE: found = (cmp >= 0); break;
								case PM_DEP_MOD_LE: found = (cmp <= 0); break;
							}
							FREE(ver);
						}
						FREELISTPTR(k);
					}
				}
				/* else if still not found... */
				if(!found) {
					MALLOC(miss, sizeof(pmdepmissing_t));
					miss->type = PM_DEP_TYPE_DEPEND;
					miss->depend.mod = depend.mod;
					STRNCPY(miss->target, tp->name, PKG_NAME_LEN);
					STRNCPY(miss->depend.name, depend.name, PKG_NAME_LEN);
					STRNCPY(miss->depend.version, depend.version, PKG_VERSION_LEN);
					if(!pm_list_is_in(miss, baddeps)) {
						baddeps = pm_list_add(baddeps, miss);
					} else {
						FREE(miss);
					}
				}
			}
		}
	} else if(op == PM_TRANS_TYPE_REMOVE) {
		/* check requiredby fields */
		for(i = packages; i; i = i->next) {
			pmpkg_t *tp;
			if(i->data == NULL) {
				continue;
			}
			tp = (pmpkg_t*)i->data;
			for(j = tp->requiredby; j; j = j->next) {
				if(!pm_list_is_strin((char *)j->data, packages)) {
					MALLOC(miss, sizeof(pmdepmissing_t));
					miss->type = PM_DEP_TYPE_REQUIRED;
					miss->depend.mod = PM_DEP_MOD_ANY;
					miss->depend.version[0] = '\0';
					STRNCPY(miss->target, tp->name, PKG_NAME_LEN);
					STRNCPY(miss->depend.name, (char *)j->data, PKG_NAME_LEN);
					if(!pm_list_is_in(miss, baddeps)) {
						baddeps = pm_list_add(baddeps, miss);
					} else {
						FREE(miss);
					}
				}
			}
		}
	}

	return(baddeps);
}

int splitdep(char *depstr, pmdepend_t *depend)
{
	char *str = NULL;
	char *ptr = NULL;

	if(depstr == NULL || depend == NULL) {
		return(-1);
	}

	depend->mod = 0;
	depend->name[0] = 0;
	depend->version[0] = 0;

	str = strdup(depstr);

	if((ptr = strstr(str, ">="))) {
		depend->mod = PM_DEP_MOD_GE;
	} else if((ptr = strstr(str, "<="))) {
		depend->mod = PM_DEP_MOD_LE;
	} else if((ptr = strstr(str, "="))) {
		depend->mod = PM_DEP_MOD_EQ;
	} else {
		/* no version specified - accept any */
		depend->mod = PM_DEP_MOD_ANY;
		STRNCPY(depend->name, str, PKG_NAME_LEN);
	}

	if(ptr == NULL) {
		FREE(str);
		return(0);
	}
	*ptr = '\0';
	STRNCPY(depend->name, str, PKG_NAME_LEN);
	ptr++;
	if(depend->mod != PM_DEP_MOD_EQ) {
		ptr++;
	}
	STRNCPY(depend->version, ptr, PKG_VERSION_LEN);
	FREE(str);

	return(0);
}

/* return a new PMList target list containing all packages in the original
 * target list, as well as all their un-needed dependencies.  By un-needed,
 * I mean dependencies that are *only* required for packages in the target
 * list, so they can be safely removed.  This function is recursive.
 */
PMList* removedeps(pmdb_t *db, PMList *targs)
{
	PMList *i, *j, *k;
	PMList *newtargs = targs;

	if(db == NULL) {
		return(newtargs);
	}

	for(i = targs; i; i = i->next) {
		pmpkg_t *pkg = (pmpkg_t*)i->data;
		for(j = pkg->depends; j; j = j->next) {
			pmdepend_t depend;
			pmpkg_t *dep;
			int needed = 0;
			splitdep(j->data, &depend);
			dep = db_get_pkgfromcache(db, depend.name);
			if(pkg_isin(dep, targs)) {
				continue;
			}
			/* see if it was explicitly installed */
			if(dep->reason == PM_PKG_REASON_EXPLICIT) {
				_alpm_log(PM_LOG_FLOW2, "excluding %s -- explicitly installed", dep->name);
				needed = 1;
			}
			/* see if other packages need it */
			for(k = dep->requiredby; k && !needed; k = k->next) {
				pmpkg_t *dummy = db_get_pkgfromcache(db, k->data);
				if(!pkg_isin(dummy, targs)) {
					needed = 1;
				}
			}
			if(!needed) {
				char *name;
				asprintf(&name, "%s-%s", dep->name, dep->version);
				/* add it to the target list */
				db_read(db, name, INFRQ_ALL, dep);
				newtargs = pm_list_add(newtargs, dep);
				newtargs = removedeps(db, newtargs);
				FREE(name);
			}
		}
	}

	return(newtargs);
}

/* populates *list with packages that need to be installed to satisfy all
 * dependencies (recursive) for syncpkg
 *
 * make sure *list and *trail are already initialized
 */
int resolvedeps(pmdb_t *local, PMList *dbs_sync, pmpkg_t *syncpkg, PMList *list, PMList *trail, pmtrans_t *trans)
{
	PMList *i, *j;
	PMList *targ;
	PMList *deps = NULL;

	if(local == NULL || dbs_sync == NULL || syncpkg == NULL) {
		return(-1);
	}

	targ = pm_list_add(NULL, syncpkg);
	deps = checkdeps(local, PM_TRANS_TYPE_ADD, targ);
	FREELISTPTR(targ);

	if(deps == NULL) {
		return(0);
	}

	for(i = deps; i; i = i->next) {
		int found = 0;
		pmdepmissing_t *miss = i->data;

		/* XXX: conflicts are now treated specially in the _add and _sync functions */

		/*if(miss->type == PM_DEP_TYPE_CONFLICT) {
			_alpm_log(PM_LOG_ERROR, "cannot resolve dependencies for \"%s\" (it conflict with %s)", miss->target, miss->depend.name);
			RET_ERR(???, -1);
		} else*/

		if(miss->type == PM_DEP_TYPE_DEPEND) {
			pmpkg_t *sync = NULL;
			/* find the package in one of the repositories */
			/* check literals */
			for(j = dbs_sync; !sync && j; j = j->next) {
				PMList *k;
				pmdb_t *dbs = j->data;
				for(k = db_get_pkgcache(dbs); !sync && k; k = k->next) {
					pmpkg_t *pkg = k->data;
					if(!strcmp(miss->depend.name, pkg->name)) {
						sync = pkg;
					}
				}
			}
			/* check provides */
			for(j = dbs_sync; !sync && j; j = j->next) {
				PMList *provides;
				pmdb_t *dbs = j->data;
				provides = _alpm_db_whatprovides(dbs, miss->depend.name);
				if(provides) {
					sync = provides->data;
				}
				FREELISTPTR(provides);
			}
			if(sync == NULL) {
				_alpm_log(PM_LOG_ERROR, "cannot resolve dependencies for \"%s\" (\"%s\" is not in the package set", miss->target, miss->depend.name);
				pm_errno = PM_ERR_UNRESOLVABLE_DEPS;
				goto error;
			}
			found = 0;
			for(j = list; j && !found; j = j->next) {
				pmpkg_t *tmp = j->data;
				if(tmp && !strcmp(tmp->name, sync->name)) {
					found = 1;
				}
			}
			if(found) {
				/* this dep is already in the target list */
				continue;
			}
			_alpm_log(PM_LOG_DEBUG, "resolving %s", sync->name);
			found = 0;
			for(j = trail; j; j = j->next) {
				pmpkg_t *tmp = j->data;
				if(tmp && !strcmp(tmp->name, sync->name)) {
					found = 1;
				}
			}
			if(!found) {
				/* check pmo_ignorepkg and pmo_s_ignore to make sure we haven't pulled in
				 * something we're not supposed to.
				 */
				int usedep = 1;
				found = 0;
				for(j = handle->ignorepkg; j && !found; j = j->next) {
					if(!strcmp(j->data, sync->name)) {
						found = 1;
					}
				}
				if(found) {
					pmpkg_t *dummypkg = pkg_dummy(miss->target, NULL);
					QUESTION(trans, PM_TRANS_CONV_INSTALL_IGNOREPKG, dummypkg, sync, NULL, &usedep);
					FREEPKG(dummypkg);
				}
				if(usedep) {
					trail = pm_list_add(trail, sync);
					if(resolvedeps(local, dbs_sync, sync, list, trail, trans)) {
						goto error;
					}
					_alpm_log(PM_LOG_FLOW2, "adding dependency %s-%s", sync->name, sync->version);
					list = pm_list_add(list, sync);
				} else {
					_alpm_log(PM_LOG_ERROR, "cannot resolve dependencies for \"%s\"", miss->target);
					pm_errno = PM_ERR_UNRESOLVABLE_DEPS;
					goto error;
				}
			} else {
				/* cycle detected -- skip it */
				_alpm_log(PM_LOG_DEBUG, "dependency cycle detected: %s", sync->name);
			}
		}
	}

	FREELIST(deps);

	return(0);

error:
	FREELIST(deps);
	return(-1);
}

/* vim: set ts=2 sw=2 noet: */
