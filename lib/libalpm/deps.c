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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef __sun__
#include <strings.h>
#endif
#include <math.h>

/* libalpm */
#include "deps.h"
#include "alpm_list.h"
#include "util.h"
#include "log.h"
#include "error.h"
#include "package.h"
#include "db.h"
#include "cache.h"
#include "provide.h"
#include "versioncmp.h"
#include "handle.h"

extern pmhandle_t *handle;

pmdepmissing_t *_alpm_depmiss_new(const char *target, pmdeptype_t type,
                                  pmdepmod_t depmod, const char *depname,
                                  const char *depversion)
{
	pmdepmissing_t *miss;

	ALPM_LOG_FUNC;

	miss = malloc(sizeof(pmdepmissing_t));
	if(miss == NULL) {
		_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(pmdepmissing_t));
		RET_ERR(PM_ERR_MEMORY, NULL);
	}

	strncpy(miss->target, target, PKG_NAME_LEN);
	miss->type = type;
	miss->depend.mod = depmod;
	strncpy(miss->depend.name, depname, PKG_NAME_LEN);
	if(depversion) {
		strncpy(miss->depend.version, depversion, PKG_VERSION_LEN);
	} else {
		miss->depend.version[0] = 0;
	}

	return(miss);
}

int _alpm_depmiss_isin(pmdepmissing_t *needle, alpm_list_t *haystack)
{
	alpm_list_t *i;

	ALPM_LOG_FUNC;

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
 * This function returns the new alpm_list_t* target list.
 *
 */ 
alpm_list_t *_alpm_sortbydeps(alpm_list_t *targets, pmtranstype_t mode)
{
	alpm_list_t *newtargs = NULL;
	alpm_list_t *i, *j, *k, *l;
	int change = 1;
	int numscans = 0;
	int numtargs = 0;
	int maxscans;

	ALPM_LOG_FUNC;

	if(targets == NULL) {
		return(NULL);
	}

	for(i = targets; i; i = i->next) {
		newtargs = alpm_list_add(newtargs, i->data);
		numtargs++;
	}

	maxscans = (int)sqrt(numtargs);

	_alpm_log(PM_LOG_DEBUG, _("started sorting dependencies"));
	while(change) {
		alpm_list_t *tmptargs = NULL;
		change = 0;
		if(numscans > maxscans) {
			_alpm_log(PM_LOG_DEBUG, _("possible dependency cycle detected"));
			continue;
		}
		numscans++;
		/* run thru targets, moving up packages as necessary */
		for(i = newtargs; i; i = i->next) {
			pmpkg_t *p = i->data;
			_alpm_log(PM_LOG_DEBUG, "   sorting %s", alpm_pkg_get_name(p));
			for(j = alpm_pkg_get_depends(p); j; j = j->next) {
				pmdepend_t *depend = alpm_splitdep(j->data);
				pmpkg_t *q = NULL;
				if(depend == NULL) {
					continue;
				}
				/* look for depend->name -- if it's farther down in the list, then
				 * move it up above p
				 */
				for(k = i->next; k; k = k->next) {
					q = k->data;
					const char *qname = alpm_pkg_get_name(q);
					if(!strcmp(depend->name, qname)) {
						if(!_alpm_pkg_find(qname, tmptargs)) {
							change = 1;
							tmptargs = alpm_list_add(tmptargs, q);
						}
						break;
					}
					for(l = alpm_pkg_get_provides(q); l; l = l->next) {
						const char *provname = l->data;
						if(!strcmp(depend->name, provname)) {
							if(!_alpm_pkg_find(qname, tmptargs)) {
								change = 1;
								tmptargs = alpm_list_add(tmptargs, q);
							}
							break;
						}
					}
				}
				free(depend);
			}
			if(!_alpm_pkg_find(alpm_pkg_get_name(p), tmptargs)) {
				tmptargs = alpm_list_add(tmptargs, p);
			}
		}
		alpm_list_free(newtargs);
		newtargs = tmptargs;
	}
	_alpm_log(PM_LOG_DEBUG, _("sorting dependencies finished"));

	if(mode == PM_TRANS_TYPE_REMOVE) {
		/* we're removing packages, so reverse the order */
		alpm_list_t *tmptargs = alpm_list_reverse(newtargs);
		/* free the old one */
		alpm_list_free(newtargs);
		newtargs = tmptargs;
	}

	return(newtargs);
}

/** Checks dependencies and returns missing ones in a list. Dependencies can include versions with depmod operators.
 * @param trans pointer to the transaction object
 * @param db pointer to the local package database
 * @param op transaction type
 * @param packages an alpm_list_t* of packages to be checked
 * @return an alpm_list_t* of missing_t pointers.
 */
alpm_list_t *_alpm_checkdeps(pmtrans_t *trans, pmdb_t *db, pmtranstype_t op,
                             alpm_list_t *packages)
{
	alpm_list_t *i, *j, *k, *l;
	int found = 0;
	alpm_list_t *baddeps = NULL;
	pmdepmissing_t *miss = NULL;

	ALPM_LOG_FUNC;

	if(db == NULL) {
		return(NULL);
	}

	if(op == PM_TRANS_TYPE_UPGRADE) {
		/* PM_TRANS_TYPE_UPGRADE handles the backwards dependencies, ie, the packages
		 * listed in the requiredby field.
		 */
		for(i = packages; i; i = i->next) {
			pmpkg_t *newpkg = i->data;
			pmpkg_t *oldpkg;
			if(newpkg == NULL) {
				_alpm_log(PM_LOG_DEBUG, _("null package found in package list"));
				continue;
			}

			if((oldpkg = _alpm_db_get_pkgfromcache(db, alpm_pkg_get_name(newpkg))) == NULL) {
				_alpm_log(PM_LOG_DEBUG, _("cannot find package installed '%s'"),
									alpm_pkg_get_name(newpkg));
				continue;
			}
			for(j = alpm_pkg_get_requiredby(oldpkg); j; j = j->next) {
				pmpkg_t *p;
				found = 0;
				if((p = _alpm_db_get_pkgfromcache(db, j->data)) == NULL) {
					/* hmmm... package isn't installed.. */
					continue;
				}
				if(_alpm_pkg_find(alpm_pkg_get_name(p), packages)) {
					/* this package also in the upgrade list, so don't worry about it */
					continue;
				}
				for(k = alpm_pkg_get_depends(p); k; k = k->next) {
					/* don't break any existing dependencies (possible provides) */
					pmdepend_t *depend = alpm_splitdep(k->data);
					if(depend == NULL) {
						continue;
					}

					/* if oldpkg satisfied this dep, and newpkg doesn't */
					if(alpm_depcmp(oldpkg, depend) && !alpm_depcmp(newpkg, depend)) {
						/* we've found a dep that was removed... see if any other package
						 * still contains/provides the dep */
						int satisfied = 0;
						for(l = packages; l; l = l->next) {
							pmpkg_t *pkg = l->data;

							if(alpm_depcmp(pkg, depend)) {
								_alpm_log(PM_LOG_DEBUG, _("checkdeps: dependency '%s' has moved from '%s' to '%s'"),
													depend->name, alpm_pkg_get_name(oldpkg), alpm_pkg_get_name(pkg));
								satisfied = 1;
								break;
							}
						}

						if(!satisfied) {
							/* worst case... check installed packages to see if anything else
							 * satisfies this... */
							for(l = _alpm_db_get_pkgcache(db); l; l = l->next) {
								pmpkg_t *pkg = l->data;

								if(strcmp(alpm_pkg_get_name(pkg), alpm_pkg_get_name(oldpkg)) == 0) {
									/* well, we know this one succeeds, but we're removing it... skip it */
									continue;
								}

								if(alpm_depcmp(pkg, depend)) {
									_alpm_log(PM_LOG_DEBUG, _("checkdeps: dependency '%s' satisfied by installed package '%s'"),
														depend->name, alpm_pkg_get_name(pkg));
									satisfied = 1;
									break;
								}
							}
						}

						if(!satisfied) {
							_alpm_log(PM_LOG_DEBUG, _("checkdeps: updated '%s' won't satisfy a dependency of '%s'"),
												alpm_pkg_get_name(oldpkg), alpm_pkg_get_name(p));
							miss = _alpm_depmiss_new(p->name, PM_DEP_TYPE_DEPEND, depend->mod,
																			 depend->name, depend->version);
							if(!_alpm_depmiss_isin(miss, baddeps)) {
								baddeps = alpm_list_add(baddeps, miss);
							} else {
								FREE(miss);
							}
						}
					}
					free(depend);
				}
			}
		}
	}
	if(op == PM_TRANS_TYPE_ADD || op == PM_TRANS_TYPE_UPGRADE) {
		/* DEPENDENCIES -- look for unsatisfied dependencies */
		for(i = packages; i; i = i->next) {
			pmpkg_t *tp = i->data;
			if(tp == NULL) {
				_alpm_log(PM_LOG_DEBUG, _("null package found in package list"));
				continue;
			}

			for(j = alpm_pkg_get_depends(tp); j; j = j->next) {
				/* split into name/version pairs */
				pmdepend_t *depend = alpm_splitdep((char*)j->data);
				if(depend == NULL) {
					continue;
				}
				
				found = 0;
				/* check database for literal packages */
				for(k = _alpm_db_get_pkgcache(db); k && !found; k = k->next) {
					pmpkg_t *p = (pmpkg_t *)k->data;
					found = alpm_depcmp(p, depend);
				}
 				/* check database for provides matches */
 				if(!found) {
 					alpm_list_t *m;
 					for(m = _alpm_db_whatprovides(db, depend->name); m && !found; m = m->next) {
 						/* look for a match that isn't one of the packages we're trying
 						 * to install.  this way, if we match against a to-be-installed
 						 * package, we'll defer to the NEW one, not the one already
 						 * installed. */
 						pmpkg_t *p = m->data;
 						alpm_list_t *n;
 						int skip = 0;
 						for(n = packages; n && !skip; n = n->next) {
 							pmpkg_t *ptp = n->data;
 							if(strcmp(alpm_pkg_get_name(ptp), alpm_pkg_get_name(p)) == 0) {
 								skip = 1;
 							}
 						}
 						if(skip) {
 							continue;
 						}

						found = alpm_depcmp(p, depend);
					}
				}
 				/* check other targets */
 				for(k = packages; k && !found; k = k->next) {
 					pmpkg_t *p = k->data;
					found = alpm_depcmp(p, depend);
				}
				/* else if still not found... */
				if(!found) {
					_alpm_log(PM_LOG_DEBUG, _("missing dependency '%s' for package '%s'"),
					                          depend->name, alpm_pkg_get_name(tp));
					miss = _alpm_depmiss_new(alpm_pkg_get_name(tp), PM_DEP_TYPE_DEPEND, depend->mod,
					                         depend->name, depend->version);
					if(!_alpm_depmiss_isin(miss, baddeps)) {
						baddeps = alpm_list_add(baddeps, miss);
					} else {
						FREE(miss);
					}
				}
				free(depend);
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
			for(j = alpm_pkg_get_requiredby(tp); j; j = j->next) {
				/* Search for 'reqname' in packages for removal */
				char *reqname = j->data;
				alpm_list_t *x = NULL;
				for(x = packages; x; x = x->next) {
					pmpkg_t *xp = x->data;
					if(strcmp(reqname, alpm_pkg_get_name(xp)) == 0) {
						found = 1;
						break;
					}
				}
				if(!found) {
					/* check if a package in trans->packages provides this package */
					for(k = trans->packages; !found && k; k=k->next) {
						pmpkg_t *spkg = NULL;
						if(trans->type == PM_TRANS_TYPE_SYNC) {
							pmsyncpkg_t *sync = k->data;
							spkg = sync->pkg;
						} else {
							spkg = k->data;
						}
						if(spkg) {
							if(alpm_list_find_str(alpm_pkg_get_provides(spkg), tp->name)) {
								found = 1;
							}
						}
					}
					if(!found) {
						_alpm_log(PM_LOG_DEBUG, _("checkdeps: found %s as required by %s"),
								reqname, alpm_pkg_get_name(tp));
						miss = _alpm_depmiss_new(alpm_pkg_get_name(tp), PM_DEP_TYPE_DEPEND,
																		 PM_DEP_MOD_ANY, j->data, NULL);
						if(!_alpm_depmiss_isin(miss, baddeps)) {
							baddeps = alpm_list_add(baddeps, miss);
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

pmdepend_t SYMEXPORT *alpm_splitdep(const char *depstring)
{
	pmdepend_t *depend;
	char *ptr = NULL;

	if(depstring == NULL) {
		return(NULL);
	}
	
	depend = malloc(sizeof(pmdepend_t));
	if(depend == NULL) {
		_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(pmdepend_t));
		return(NULL);
	}

	/* Find a version comparator if one exists. If it does, set the type and
	 * increment the ptr accordingly so we can copy the right strings. */
	if((ptr = strstr(depstring, ">="))) {
		depend->mod = PM_DEP_MOD_GE;
		*ptr = '\0';
		ptr += 2;
	} else if((ptr = strstr(depstring, "<="))) {
		depend->mod = PM_DEP_MOD_LE;
		*ptr = '\0';
		ptr += 2;
	} else if((ptr = strstr(depstring, "="))) {
		depend->mod = PM_DEP_MOD_EQ;
		*ptr = '\0';
		ptr += 1;
	} else {
		/* no version specified - copy in the name and return it */
		depend->mod = PM_DEP_MOD_ANY;
		strncpy(depend->name, depstring, PKG_NAME_LEN);
		depend->version[0] = '\0';
		return(depend);
	}

	/* if we get here, we have a version comparator, copy the right parts
	 * to the right places */
	strncpy(depend->name, depstring, PKG_NAME_LEN);
	strncpy(depend->version, ptr, PKG_VERSION_LEN);

	return(depend);
}

/* These parameters are messy.  We check if this package, given a list of
 * targets (and a db), is safe to remove.  We do NOT remove it if it is in the
 * target list */
static int can_remove_package(pmdb_t *db, pmpkg_t *pkg, alpm_list_t *targets)
{
	alpm_list_t *i;

	if(_alpm_pkg_find(alpm_pkg_get_name(pkg), targets)) {
		return(0);
	}

	/* see if it was explicitly installed */
	if(alpm_pkg_get_reason(pkg) == PM_PKG_REASON_EXPLICIT) {
		_alpm_log(PM_LOG_DEBUG, _("excluding %s -- explicitly installed"), alpm_pkg_get_name(pkg));
		return(0);
	}

	/* see if other packages need it */
	for(i = alpm_pkg_get_requiredby(pkg); i; i = i->next) {
		pmpkg_t *reqpkg = _alpm_db_get_pkgfromcache(db, i->data);
		if(reqpkg && !_alpm_pkg_find(alpm_pkg_get_name(reqpkg), targets)) {
			return(0);
		}
	}

	/* it's ok to remove */
	return(1);
}

/* return a new alpm_list_t target list containing all packages in the original
 * target list, as well as all their un-needed dependencies.  By un-needed,
 * I mean dependencies that are *only* required for packages in the target
 * list, so they can be safely removed.  This function is recursive.
 */
alpm_list_t *_alpm_removedeps(pmdb_t *db, alpm_list_t *targs)
{
	alpm_list_t *i, *j, *k;
	alpm_list_t *newtargs = targs;

	ALPM_LOG_FUNC;

	if(db == NULL) {
		return(newtargs);
	}

	for(i = targs; i; i = i->next) {
		pmpkg_t *pkg = i->data;
		for(j = alpm_pkg_get_depends(pkg); j; j = j->next) {
			pmdepend_t *depend = alpm_splitdep(j->data);
			pmpkg_t *deppkg;
			if(depend == NULL) {
				continue;
			}

			deppkg = _alpm_db_get_pkgfromcache(db, depend->name);
			if(deppkg == NULL) {
				/* package not found... look for a provision instead */
				alpm_list_t *provides = _alpm_db_whatprovides(db, depend->name);
				if(!provides) {
					/* Not found, that's fine, carry on */
					_alpm_log(PM_LOG_DEBUG, _("cannot find package \"%s\" or anything that provides it!"), depend->name);
					continue;
				}
				for(k = provides; k; k = k->next) {
					pmpkg_t *provpkg = k->data;
					if(can_remove_package(db, provpkg, newtargs)) {
						pmpkg_t *pkg = _alpm_pkg_dup(provpkg);

						_alpm_log(PM_LOG_DEBUG, _("adding '%s' to the targets"), alpm_pkg_get_name(pkg));

						/* add it to the target list */
						newtargs = alpm_list_add(newtargs, pkg);
						newtargs = _alpm_removedeps(db, newtargs);
					}
				}
				alpm_list_free(provides);
			} else if(can_remove_package(db, deppkg, newtargs)) {
				pmpkg_t *pkg = _alpm_pkg_dup(deppkg);

				_alpm_log(PM_LOG_DEBUG, _("adding '%s' to the targets"), alpm_pkg_get_name(pkg));

				/* add it to the target list */
				newtargs = alpm_list_add(newtargs, pkg);
				newtargs = _alpm_removedeps(db, newtargs);
			}
			free(depend);
		}
	}

	return(newtargs);
}

/* populates *list with packages that need to be installed to satisfy all
 * dependencies (recursive) for syncpkg
 *
 * make sure *list and *trail are already initialized
 */
int _alpm_resolvedeps(pmdb_t *local, alpm_list_t *dbs_sync, pmpkg_t *syncpkg,
                      alpm_list_t *list, alpm_list_t *trail, pmtrans_t *trans,
                      alpm_list_t **data)
{
	alpm_list_t *i, *j;
	alpm_list_t *targ;
	alpm_list_t *deps = NULL;

	ALPM_LOG_FUNC;

	if(local == NULL || dbs_sync == NULL || syncpkg == NULL) {
		return(-1);
	}

	_alpm_log(PM_LOG_DEBUG, _("started resolving dependencies"));
	targ = alpm_list_add(NULL, syncpkg);
	deps = _alpm_checkdeps(trans, local, PM_TRANS_TYPE_ADD, targ);
	alpm_list_free(targ);

	if(deps == NULL) {
		return(0);
	}

	for(i = deps; i; i = i->next) {
		int found = 0;
		pmdepmissing_t *miss = i->data;
		pmpkg_t *sync = NULL;

		/* check if one of the packages in *list already provides this dependency */
		for(j = list; j && !found; j = j->next) {
			pmpkg_t *sp = j->data;
			if(alpm_list_find_str(alpm_pkg_get_provides(sp), miss->depend.name)) {
				_alpm_log(PM_LOG_DEBUG, _("%s provides dependency %s -- skipping"),
				          alpm_pkg_get_name(sp), miss->depend.name);
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
		/*TODO this autoresolves the first 'provides' package... we should fix this
		 * somehow */
		/* check provides */
		if(!sync) {
			for(j = dbs_sync; !sync && j; j = j->next) {
				alpm_list_t *provides;
				provides = _alpm_db_whatprovides(j->data, miss->depend.name);
				if(provides) {
					sync = provides->data;
				}
				alpm_list_free(provides);
			}
		}

		if(!sync) {
			_alpm_log(PM_LOG_ERROR, _("cannot resolve dependencies for \"%s\" (\"%s\" is not in the package set)"),
			          miss->target, miss->depend.name);
			if(data) {
				if((miss = malloc(sizeof(pmdepmissing_t))) == NULL) {
					_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(pmdepmissing_t));
					FREELIST(*data);
					pm_errno = PM_ERR_MEMORY;
					goto error;
				}
				*miss = *(pmdepmissing_t *)i->data;
				*data = alpm_list_add(*data, miss);
			}
			pm_errno = PM_ERR_UNSATISFIED_DEPS;
			goto error;
		}
		if(_alpm_pkg_find(alpm_pkg_get_name(sync), list)) {
			/* this dep is already in the target list */
			_alpm_log(PM_LOG_DEBUG, _("dependency %s is already in the target list -- skipping"),
								alpm_pkg_get_name(sync));
			continue;
		}

		if(!_alpm_pkg_find(alpm_pkg_get_name(sync), trail)) {
			/* check pmo_ignorepkg and pmo_s_ignore to make sure we haven't pulled in
			 * something we're not supposed to.
			 */
			int usedep = 1;
			if(alpm_list_find_str(handle->ignorepkg, alpm_pkg_get_name(sync))) {
				pmpkg_t *dummypkg = _alpm_pkg_new(miss->target, NULL);
				QUESTION(trans, PM_TRANS_CONV_INSTALL_IGNOREPKG, dummypkg, sync, NULL, &usedep);
				_alpm_pkg_free(dummypkg);
			}
			if(usedep) {
				trail = alpm_list_add(trail, sync);
				if(_alpm_resolvedeps(local, dbs_sync, sync, list, trail, trans, data)) {
					goto error;
				}
				_alpm_log(PM_LOG_DEBUG, _("pulling dependency %s (needed by %s)"),
									alpm_pkg_get_name(sync), alpm_pkg_get_name(syncpkg));
				list = alpm_list_add(list, sync);
			} else {
				_alpm_log(PM_LOG_ERROR, _("cannot resolve dependencies for \"%s\""), miss->target);
				if(data) {
					if((miss = malloc(sizeof(pmdepmissing_t))) == NULL) {
						_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(pmdepmissing_t));
						FREELIST(*data);
						pm_errno = PM_ERR_MEMORY;
						goto error;
					}
					*miss = *(pmdepmissing_t *)i->data;
					*data = alpm_list_add(*data, miss);
				}
				pm_errno = PM_ERR_UNSATISFIED_DEPS;
				goto error;
			}
		} else {
			/* cycle detected -- skip it */
			_alpm_log(PM_LOG_DEBUG, _("dependency cycle detected: %s"), sync->name);
		}
	}
	
	_alpm_log(PM_LOG_DEBUG, _("finished resolving dependencies"));

	FREELIST(deps);

	return(0);

error:
	FREELIST(deps);
	return(-1);
}

const char SYMEXPORT *alpm_dep_get_target(pmdepmissing_t *miss)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(miss != NULL, return(NULL));

	return miss->target;
}

pmdeptype_t SYMEXPORT alpm_dep_get_type(pmdepmissing_t *miss)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(-1));
	ASSERT(miss != NULL, return(-1));

	return miss->type;
}

pmdepmod_t SYMEXPORT alpm_dep_get_mod(pmdepmissing_t *miss)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(-1));
	ASSERT(miss != NULL, return(-1));

	return miss->depend.mod;
}

const char SYMEXPORT *alpm_dep_get_name(pmdepmissing_t *miss)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(miss != NULL, return(NULL));

	return miss->depend.name;
}

const char SYMEXPORT *alpm_dep_get_version(pmdepmissing_t *miss)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(miss != NULL, return(NULL));

	return miss->depend.version;
}
/* vim: set ts=2 sw=2 noet: */
