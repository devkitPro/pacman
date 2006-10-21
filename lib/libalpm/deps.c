/*
 *  deps.c
 * 
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2005, 2006 by Miklos Vajna <vmiklos@frugalware.org>
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
#ifdef __sun__
#include <strings.h>
#endif
#include <libintl.h>
#include <math.h>
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
#include "versioncmp.h"
#include "handle.h"

extern pmhandle_t *handle;

pmdepmissing_t *_alpm_depmiss_new(const char *target, unsigned char type, unsigned char depmod,
                                  const char *depname, const char *depversion)
{
	pmdepmissing_t *miss;

	miss = (pmdepmissing_t *)malloc(sizeof(pmdepmissing_t));
	if(miss == NULL) {
		_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(pmdepmissing_t));
		RET_ERR(PM_ERR_MEMORY, NULL);
	}

	STRNCPY(miss->target, target, PKG_NAME_LEN);
	miss->type = type;
	miss->depend.mod = depmod;
	STRNCPY(miss->depend.name, depname, PKG_NAME_LEN);
	if(depversion) {
		STRNCPY(miss->depend.version, depversion, PKG_VERSION_LEN);
	} else {
		miss->depend.version[0] = 0;
	}

	return(miss);
}

int _alpm_depmiss_isin(pmdepmissing_t *needle, pmlist_t *haystack)
{
	pmlist_t *i;

	for(i = haystack; i; i = i->next) {
		pmdepmissing_t *miss = i->data;
		if(!memcmp(needle, miss, sizeof(pmdepmissing_t))
		   && !memcmp(&needle->depend, &miss->depend, sizeof(pmdepend_t))) {
			return(1);
		}
	}

	return(0);
}

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
 * This function returns the new pmlist_t* target list.
 *
 */ 
pmlist_t *_alpm_sortbydeps(pmlist_t *targets, int mode)
{
	pmlist_t *newtargs = NULL;
	pmlist_t *i, *j, *k, *l;
	int change = 1;
	int numscans = 0;
	int numtargs = 0;

	if(targets == NULL) {
		return(NULL);
	}

	for(i = targets; i; i = i->next) {
		newtargs = _alpm_list_add(newtargs, i->data);
		numtargs++;
	}

	_alpm_log(PM_LOG_DEBUG, _("started sorting dependencies"));
	while(change) {
		pmlist_t *tmptargs = NULL;
		change = 0;
		if(numscans > sqrt(numtargs)) {
			_alpm_log(PM_LOG_DEBUG, _("possible dependency cycle detected"));
			continue;
		}
		numscans++;
		/* run thru targets, moving up packages as necessary */
		for(i = newtargs; i; i = i->next) {
			pmpkg_t *p = (pmpkg_t*)i->data;
			for(j = p->depends; j; j = j->next) {
				pmdepend_t dep;
				pmpkg_t *q = NULL;
				if(_alpm_splitdep(j->data, &dep)) {
					continue;
				}
				/* look for dep.name -- if it's farther down in the list, then
				 * move it up above p
				 */
				for(k = i->next; k; k = k->next) {
					q = (pmpkg_t *)k->data;
					if(!strcmp(dep.name, q->name)) {
						if(!_alpm_pkg_isin(q->name, tmptargs)) {
							change = 1;
							tmptargs = _alpm_list_add(tmptargs, q);
						}
						break;
					}
					for(l = q->provides; l; l = l->next) {
						if(!strcmp(dep.name, (char*)l->data)) {
							if(!_alpm_pkg_isin((char*)l->data, tmptargs)) {
								change = 1;
								tmptargs = _alpm_list_add(tmptargs, q);
							}
							break;
						}
					}
				}
			}
			if(!_alpm_pkg_isin(p->name, tmptargs)) {
				tmptargs = _alpm_list_add(tmptargs, p);
			}
		}
		FREELISTPTR(newtargs);
		newtargs = tmptargs;
	}
	_alpm_log(PM_LOG_DEBUG, _("sorting dependencies finished"));

	if(mode == PM_TRANS_TYPE_REMOVE) {
		/* we're removing packages, so reverse the order */
		pmlist_t *tmptargs = _alpm_list_reverse(newtargs);
		/* free the old one */
		FREELISTPTR(newtargs);
		newtargs = tmptargs;
	}

	return(newtargs);
}

/* Returns a pmlist_t* of missing_t pointers.
 *
 * dependencies can include versions with depmod operators.
 *
 */
pmlist_t *_alpm_checkdeps(pmtrans_t *trans, pmdb_t *db, unsigned char op, pmlist_t *packages)
{
	pmdepend_t depend;
	pmlist_t *i, *j, *k;
	int cmp;
	int found = 0;
	pmlist_t *baddeps = NULL;
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

			if((oldpkg = _alpm_db_get_pkgfromcache(db, tp->name)) == NULL) {
				continue;
			}
			for(j = oldpkg->requiredby; j; j = j->next) {
				char *ver;
				pmpkg_t *p;
				found = 0;
				if((p = _alpm_db_get_pkgfromcache(db, j->data)) == NULL) {
					/* hmmm... package isn't installed.. */
					continue;
				}
				if(_alpm_pkg_isin(p->name, packages)) {
					/* this package is also in the upgrade list, so don't worry about it */
					continue;
				}
				for(k = p->depends; k && !found; k = k->next) {
					/* find the dependency info in p->depends */
					_alpm_splitdep(k->data, &depend);
					if(!strcmp(depend.name, oldpkg->name)) {
						found = 1;
					}
				}
				if(found == 0) {
					/* look for packages that list depend.name as a "provide" */
					pmlist_t *provides = _alpm_db_whatprovides(db, depend.name);
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
					cmp = _alpm_versioncmp(ver, depend.version);
					switch(depend.mod) {
						case PM_DEP_MOD_EQ: found = (cmp == 0); break;
						case PM_DEP_MOD_GE: found = (cmp >= 0); break;
						case PM_DEP_MOD_LE: found = (cmp <= 0); break;
					}
					FREE(ver);
				}
				if(!found) {
					_alpm_log(PM_LOG_DEBUG, _("checkdeps: found %s as required by %s"), depend.name, p->name);
					miss = _alpm_depmiss_new(p->name, PM_DEP_TYPE_REQUIRED, depend.mod, depend.name, depend.version);
					if(!_alpm_depmiss_isin(miss, baddeps)) {
						baddeps = _alpm_list_add(baddeps, miss);
					} else {
						FREE(miss);
					}
				}
			}
		}
	}
	if(op == PM_TRANS_TYPE_ADD || op == PM_TRANS_TYPE_UPGRADE) {
		/* DEPENDENCIES -- look for unsatisfied dependencies */
		for(i = packages; i; i = i->next) {
			pmpkg_t *tp = i->data;
			if(tp == NULL) {
				continue;
			}

			for(j = tp->depends; j; j = j->next) {
				/* split into name/version pairs */
				_alpm_splitdep((char *)j->data, &depend);
				found = 0;
				/* check database for literal packages */
				for(k = _alpm_db_get_pkgcache(db); k && !found; k = k->next) {
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
							cmp = _alpm_versioncmp(ver, depend.version);
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
 				if(!found) {
 					pmlist_t *m;
 					k = _alpm_db_whatprovides(db, depend.name);
 					for(m = k; m && !found; m = m->next) {
 						/* look for a match that isn't one of the packages we're trying
 						 * to install.  this way, if we match against a to-be-installed
 						 * package, we'll defer to the NEW one, not the one already
 						 * installed. */
 						pmpkg_t *p = m->data;
 						pmlist_t *n;
 						int skip = 0;
 						for(n = packages; n && !skip; n = n->next) {
 							pmpkg_t *ptp = n->data;
 							if(!strcmp(ptp->name, p->name)) {
 								skip = 1;
 							}
 						}
 						if(skip) {
 							continue;
 						}

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
							cmp = _alpm_versioncmp(ver, depend.version);
							switch(depend.mod) {
								case PM_DEP_MOD_EQ: found = (cmp == 0); break;
								case PM_DEP_MOD_GE: found = (cmp >= 0); break;
								case PM_DEP_MOD_LE: found = (cmp <= 0); break;
							}
							FREE(ver);
						}
					}
					FREELISTPTR(k);
				}
 				/* check other targets */
 				for(k = packages; k && !found; k = k->next) {
 					pmpkg_t *p = (pmpkg_t *)k->data;
 					/* see if the package names match OR if p provides depend.name */
 					if(!strcmp(p->name, depend.name) || _alpm_list_is_strin(depend.name, p->provides)) {
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
							cmp = _alpm_versioncmp(ver, depend.version);
							switch(depend.mod) {
								case PM_DEP_MOD_EQ: found = (cmp == 0); break;
								case PM_DEP_MOD_GE: found = (cmp >= 0); break;
								case PM_DEP_MOD_LE: found = (cmp <= 0); break;
							}
							FREE(ver);
						}
					}
				}
				/* else if still not found... */
				if(!found) {
					_alpm_log(PM_LOG_DEBUG, _("checkdeps: found %s as a dependency for %s"),
					          depend.name, tp->name);
					miss = _alpm_depmiss_new(tp->name, PM_DEP_TYPE_DEPEND, depend.mod, depend.name, depend.version);
					if(!_alpm_depmiss_isin(miss, baddeps)) {
						baddeps = _alpm_list_add(baddeps, miss);
					} else {
						FREE(miss);
					}
				}
			}
		}
	} else if(op == PM_TRANS_TYPE_REMOVE) {
		/* check requiredby fields */
		for(i = packages; i; i = i->next) {
			pmpkg_t *tp = i->data;
			if(tp == NULL) {
				continue;
			}

			found=0;
			for(j = tp->requiredby; j; j = j->next) {
				if(!_alpm_list_is_strin((char *)j->data, packages)) {
					/* check if a package in trans->packages provides this package */
					for(k=trans->packages; !found && k; k=k->next) {
						pmpkg_t *spkg = NULL;
					if(trans->type == PM_TRANS_TYPE_SYNC) {
						pmsyncpkg_t *sync = k->data;
						spkg = sync->pkg;
					} else {
						spkg = k->data;
					}
						if(spkg && _alpm_list_is_strin(tp->name, spkg->provides)) {
							found=1;
						}
					}
					if(!found) {
						_alpm_log(PM_LOG_DEBUG, _("checkdeps: found %s as required by %s"), (char *)j->data, tp->name);
						miss = _alpm_depmiss_new(tp->name, PM_DEP_TYPE_REQUIRED, PM_DEP_MOD_ANY, j->data, NULL);
						if(!_alpm_depmiss_isin(miss, baddeps)) {
							baddeps = _alpm_list_add(baddeps, miss);
						} else {
							FREE(miss);
						}
					}
				}
			}
		}
	}

	return(baddeps);
}

int _alpm_splitdep(char *depstr, pmdepend_t *depend)
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

/* return a new pmlist_t target list containing all packages in the original
 * target list, as well as all their un-needed dependencies.  By un-needed,
 * I mean dependencies that are *only* required for packages in the target
 * list, so they can be safely removed.  This function is recursive.
 */
pmlist_t *_alpm_removedeps(pmdb_t *db, pmlist_t *targs)
{
	pmlist_t *i, *j, *k;
	pmlist_t *newtargs = targs;

	if(db == NULL) {
		return(newtargs);
	}

	for(i = targs; i; i = i->next) {
		for(j = ((pmpkg_t *)i->data)->depends; j; j = j->next) {
			pmdepend_t depend;
			pmpkg_t *dep;
			int needed = 0;

			if(_alpm_splitdep(j->data, &depend)) {
				continue;
			}

			dep = _alpm_db_get_pkgfromcache(db, depend.name);
			if(dep == NULL) {
				/* package not found... look for a provisio instead */
				k = _alpm_db_whatprovides(db, depend.name);
				if(k == NULL) {
					_alpm_log(PM_LOG_WARNING, _("cannot find package \"%s\" or anything that provides it!"), depend.name);
					continue;
				}
				dep = _alpm_db_get_pkgfromcache(db, ((pmpkg_t *)k->data)->name);
				if(dep == NULL) {
					_alpm_log(PM_LOG_ERROR, _("dep is NULL!"));
					/* wtf */
					continue;
				}
				FREELISTPTR(k);
			}
			if(_alpm_pkg_isin(dep->name, targs)) {
				continue;
			}

			/* see if it was explicitly installed */
			if(dep->reason == PM_PKG_REASON_EXPLICIT) {
				_alpm_log(PM_LOG_FLOW2, _("excluding %s -- explicitly installed"), dep->name);
				needed = 1;
			}

			/* see if other packages need it */
			for(k = dep->requiredby; k && !needed; k = k->next) {
				pmpkg_t *dummy = _alpm_db_get_pkgfromcache(db, k->data);
				if(!_alpm_pkg_isin(dummy->name, targs)) {
					needed = 1;
				}
			}
			if(!needed) {
				pmpkg_t *pkg = _alpm_pkg_new(dep->name, dep->version);
				if(pkg == NULL) {
					continue;
				}
				/* add it to the target list */
				_alpm_log(PM_LOG_DEBUG, _("loading ALL info for '%s'"), pkg->name);
				_alpm_db_read(db, INFRQ_ALL, pkg);
				newtargs = _alpm_list_add(newtargs, pkg);
				_alpm_log(PM_LOG_FLOW2, _("adding '%s' to the targets"), pkg->name);
				newtargs = _alpm_removedeps(db, newtargs);
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
int _alpm_resolvedeps(pmdb_t *local, pmlist_t *dbs_sync, pmpkg_t *syncpkg, pmlist_t *list,
                      pmlist_t *trail, pmtrans_t *trans, pmlist_t **data)
{
	pmlist_t *i, *j;
	pmlist_t *targ;
	pmlist_t *deps = NULL;

	if(local == NULL || dbs_sync == NULL || syncpkg == NULL) {
		return(-1);
	}

	targ = _alpm_list_add(NULL, syncpkg);
	deps = _alpm_checkdeps(trans, local, PM_TRANS_TYPE_ADD, targ);
	FREELISTPTR(targ);

	if(deps == NULL) {
		return(0);
	}

	for(i = deps; i; i = i->next) {
		int found = 0;
		pmdepmissing_t *miss = i->data;
		pmpkg_t *sync = NULL;

		/* check if one of the packages in *list already provides this dependency */
		for(j = list; j && !found; j = j->next) {
			pmpkg_t *sp = (pmpkg_t *)j->data;
			if(_alpm_list_is_strin(miss->depend.name, sp->provides)) {
				_alpm_log(PM_LOG_DEBUG, _("%s provides dependency %s -- skipping"),
				          sp->name, miss->depend.name);
				found = 1;
			}
		}
		if(found) {
			continue;
		}

		/* find the package in one of the repositories */
		/* check literals */
		for(j = dbs_sync; !sync && j; j = j->next) {
			sync = _alpm_db_get_pkgfromcache(j->data, miss->depend.name);
		}
		/* check provides */
		for(j = dbs_sync; !sync && j; j = j->next) {
			pmlist_t *provides;
			provides = _alpm_db_whatprovides(j->data, miss->depend.name);
			if(provides) {
				sync = provides->data;
			}
			FREELISTPTR(provides);
		}
		if(sync == NULL) {
			_alpm_log(PM_LOG_ERROR, _("cannot resolve dependencies for \"%s\" (\"%s\" is not in the package set)"),
			          miss->target, miss->depend.name);
			if(data) {
				if((miss = (pmdepmissing_t *)malloc(sizeof(pmdepmissing_t))) == NULL) {
					_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(pmdepmissing_t));
					FREELIST(*data);
					pm_errno = PM_ERR_MEMORY;
					goto error;
				}
				*miss = *(pmdepmissing_t *)i->data;
				*data = _alpm_list_add(*data, miss);
			}
			pm_errno = PM_ERR_UNSATISFIED_DEPS;
			goto error;
		}
		if(_alpm_pkg_isin(sync->name, list)) {
			/* this dep is already in the target list */
			_alpm_log(PM_LOG_DEBUG, _("dependency %s is already in the target list -- skipping"),
			          sync->name);
			continue;
		}

		if(!_alpm_pkg_isin(sync->name, trail)) {
			/* check pmo_ignorepkg and pmo_s_ignore to make sure we haven't pulled in
			 * something we're not supposed to.
			 */
			int usedep = 1;
			if(_alpm_list_is_strin(sync->name, handle->ignorepkg)) {
				pmpkg_t *dummypkg = _alpm_pkg_new(miss->target, NULL);
				QUESTION(trans, PM_TRANS_CONV_INSTALL_IGNOREPKG, dummypkg, sync, NULL, &usedep);
				FREEPKG(dummypkg);
			}
			if(usedep) {
				trail = _alpm_list_add(trail, sync);
				if(_alpm_resolvedeps(local, dbs_sync, sync, list, trail, trans, data)) {
					goto error;
				}
				_alpm_log(PM_LOG_DEBUG, _("pulling dependency %s (needed by %s)"),
				          sync->name, syncpkg->name);
				list = _alpm_list_add(list, sync);
			} else {
				_alpm_log(PM_LOG_ERROR, _("cannot resolve dependencies for \"%s\""), miss->target);
				if(data) {
					if((miss = (pmdepmissing_t *)malloc(sizeof(pmdepmissing_t))) == NULL) {
						_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(pmdepmissing_t));
						FREELIST(*data);
						pm_errno = PM_ERR_MEMORY;
						goto error;
					}
					*miss = *(pmdepmissing_t *)i->data;
					*data = _alpm_list_add(*data, miss);
				}
				pm_errno = PM_ERR_UNSATISFIED_DEPS;
				goto error;
			}
		} else {
			/* cycle detected -- skip it */
			_alpm_log(PM_LOG_DEBUG, _("dependency cycle detected: %s"), sync->name);
		}
	}

	FREELIST(deps);

	return(0);

error:
	FREELIST(deps);
	return(-1);
}

/* vim: set ts=2 sw=2 noet: */
