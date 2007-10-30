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
#include "handle.h"

extern pmhandle_t *handle;

static pmgraph_t *_alpm_graph_new(void)
{
	pmgraph_t *graph = NULL;

	graph = (pmgraph_t *)malloc(sizeof(pmgraph_t));
	if(graph) {
		graph->state = 0;
		graph->data = NULL;
		graph->parent = NULL;
		graph->children = NULL;
		graph->childptr = NULL;
	}
	return(graph);
}

static void _alpm_graph_free(void *data)
{
	pmgraph_t *graph = data;
	alpm_list_free(graph->children);
	free(graph);
}

pmdepmissing_t *_alpm_depmiss_new(const char *target, pmdeptype_t type,
                                  pmdepmod_t depmod, const char *depname,
                                  const char *depversion)
{
	pmdepmissing_t *miss;

	ALPM_LOG_FUNC;

	MALLOC(miss, sizeof(pmdepmissing_t), RET_ERR(PM_ERR_MEMORY, NULL));

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
		if(needle->type == miss->type &&
		   !strcmp(needle->target, miss->target) &&
		   needle->depend.mod == miss->depend.mod &&
		   !strcmp(needle->depend.name, miss->depend.name) &&
		   !strcmp(needle->depend.version, miss->depend.version)) {
			return(1);
		}
	}

	return(0);
}

/* Convert a list of pmpkg_t * to a graph structure,
 * with a edge for each dependency.
 * Returns a list of vertices (one vertex = one package)
 * (used by alpm_sortbydeps)
 */
static alpm_list_t *_alpm_graph_init(alpm_list_t *targets)
{
	alpm_list_t *i, *j, *k;
	alpm_list_t *vertices = NULL;
	/* We create the vertices */
	for(i = targets; i; i = i->next) {
		pmgraph_t *vertex = _alpm_graph_new();
		vertex->data = (void *)i->data;
		vertices = alpm_list_add(vertices, vertex);
	}

	/* We compute the edges */
	for(i = vertices; i; i = i->next) {
		pmgraph_t *vertex_i = i->data;
		pmpkg_t *p_i = vertex_i->data;
		/* TODO this should be somehow combined with _alpm_checkdeps */
		for(j = vertices; j; j = j->next) {
			pmgraph_t *vertex_j = j->data;
			pmpkg_t *p_j = vertex_j->data;
			int child = 0;
			for(k = alpm_pkg_get_depends(p_i); k && !child; k = k->next) {
				pmdepend_t *depend = alpm_splitdep(k->data);
				child = alpm_depcmp(p_j, depend);
				free(depend);
			}
			if(child) {
				vertex_i->children =
					alpm_list_add(vertex_i->children, vertex_j);
			}
		}
		vertex_i->childptr = vertex_i->children;
	}
	return(vertices);
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
	alpm_list_t *vertices = NULL;
	alpm_list_t *vptr;
	pmgraph_t *vertex;

	ALPM_LOG_FUNC;

	if(targets == NULL) {
		return(NULL);
	}

	_alpm_log(PM_LOG_DEBUG, "started sorting dependencies\n");

	vertices = _alpm_graph_init(targets);

	vptr = vertices;
	vertex = vertices->data;
	while(vptr) {
		/* mark that we touched the vertex */
		vertex->state = -1;
		int found = 0;
		while(vertex->childptr && !found) {
			pmgraph_t *nextchild = (vertex->childptr)->data;
			vertex->childptr = (vertex->childptr)->next;
			if (nextchild->state == 0) {
				found = 1;
				nextchild->parent = vertex;
				vertex = nextchild;
			}
			else if(nextchild->state == -1) {
				_alpm_log(PM_LOG_WARNING, _("dependency cycle detected\n"));
			}
		}
		if(!found) {
			newtargs = alpm_list_add(newtargs, vertex->data);
			/* mark that we've left this vertex */
			vertex->state = 1;
			vertex = vertex->parent;
			if(!vertex) {
				vptr = vptr->next;
				while(vptr) {
					vertex = vptr->data;
					if (vertex->state == 0) break;
					vptr = vptr->next;
				}
			}
		}
	}

	_alpm_log(PM_LOG_DEBUG, "sorting dependencies finished\n");

	if(mode == PM_TRANS_TYPE_REMOVE) {
		/* we're removing packages, so reverse the order */
		alpm_list_t *tmptargs = alpm_list_reverse(newtargs);
		/* free the old one */
		alpm_list_free(newtargs);
		newtargs = tmptargs;
	}

	alpm_list_free_inner(vertices, _alpm_graph_free);
	alpm_list_free(vertices);

	return(newtargs);
}

/** Checks dependencies and returns missing ones in a list.
 * Dependencies can include versions with depmod operators.
 * @param db pointer to the local package database
 * @param op transaction type
 * @param packages an alpm_list_t* of packages to be checked
 * @return an alpm_list_t* of pmpkg_t* of missing_t pointers.
 */
alpm_list_t SYMEXPORT *alpm_checkdeps(pmdb_t *db, pmtranstype_t op,
                             alpm_list_t *packages)
{
	return(_alpm_checkdeps(db, op, packages));
}

alpm_list_t *_alpm_checkdeps(pmdb_t *db, pmtranstype_t op,
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
				_alpm_log(PM_LOG_DEBUG, "null package found in package list\n");
				continue;
			}
			_alpm_log(PM_LOG_DEBUG, "checkdeps: package %s-%s\n",
					alpm_pkg_get_name(newpkg), alpm_pkg_get_version(newpkg));

			if((oldpkg = _alpm_db_get_pkgfromcache(db, alpm_pkg_get_name(newpkg))) == NULL) {
				_alpm_log(PM_LOG_DEBUG, "cannot find package installed '%s'\n",
									alpm_pkg_get_name(newpkg));
				continue;
			}
			for(j = alpm_pkg_get_requiredby(oldpkg); j; j = j->next) {
				pmpkg_t *p;
				found = 0;

				if(_alpm_pkg_find(j->data, packages)) {
					/* this package also in the upgrade list, so don't worry about it */
					continue;
				}
				if((p = _alpm_db_get_pkgfromcache(db, j->data)) == NULL) {
					/* hmmm... package isn't installed.. */
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
								_alpm_log(PM_LOG_DEBUG, "checkdeps: dependency '%s' has moved from '%s' to '%s'\n",
													(char*)k->data, alpm_pkg_get_name(oldpkg), alpm_pkg_get_name(pkg));
								satisfied = 1;
								break;
							}
						}

						if(!satisfied) {
							/* worst case... check installed packages to see if anything else
							 * satisfies this... */
							for(l = _alpm_db_get_pkgcache(db); l; l = l->next) {
								pmpkg_t *pkg = l->data;

								if(alpm_depcmp(pkg, depend) && !_alpm_pkg_find(alpm_pkg_get_name(pkg), packages)) {
									/* we ignore packages that will be updated because we know
									 * that the updated ones don't satisfy depend */
									_alpm_log(PM_LOG_DEBUG, "checkdeps: dependency '%s' satisfied by installed package '%s'\n",
														(char*)k->data, alpm_pkg_get_name(pkg));
									satisfied = 1;
									break;
								}
							}
						}

						if(!satisfied) {
							_alpm_log(PM_LOG_DEBUG, "checkdeps: updated '%s' won't satisfy a dependency of '%s'\n",
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
					FREE(depend);
				}
			}
		}
	}
	if(op == PM_TRANS_TYPE_ADD || op == PM_TRANS_TYPE_UPGRADE) {
		/* DEPENDENCIES -- look for unsatisfied dependencies */
		for(i = packages; i; i = i->next) {
			pmpkg_t *tp = i->data;
			if(tp == NULL) {
				_alpm_log(PM_LOG_DEBUG, "null package found in package list\n");
				continue;
			}
			_alpm_log(PM_LOG_DEBUG, "checkdeps: package %s-%s\n",
					alpm_pkg_get_name(tp), alpm_pkg_get_version(tp));

			for(j = alpm_pkg_get_depends(tp); j; j = j->next) {
				/* split into name/version pairs */
				pmdepend_t *depend = alpm_splitdep((char*)j->data);
				if(depend == NULL) {
					continue;
				}
				
				found = 0;
 				/* check other targets */
 				for(k = packages; k && !found; k = k->next) {
 					pmpkg_t *p = k->data;
					found = alpm_depcmp(p, depend);
				}

				/* check database for satisfying packages */
				/* we can ignore packages being updated, they were checked above */
				for(k = _alpm_db_get_pkgcache(db); k && !found; k = k->next) {
					pmpkg_t *p = k->data;
					found = alpm_depcmp(p, depend)
						&& !_alpm_pkg_find(alpm_pkg_get_name(p), packages);
				}

				/* else if still not found... */
				if(!found) {
					_alpm_log(PM_LOG_DEBUG, "missing dependency '%s' for package '%s'\n",
					                          (char*)j->data, alpm_pkg_get_name(tp));
					miss = _alpm_depmiss_new(alpm_pkg_get_name(tp), PM_DEP_TYPE_DEPEND, depend->mod,
					                         depend->name, depend->version);
					if(!_alpm_depmiss_isin(miss, baddeps)) {
						baddeps = alpm_list_add(baddeps, miss);
					} else {
						FREE(miss);
					}
				}
				FREE(depend);
			}
		}
	} else if(op == PM_TRANS_TYPE_REMOVE) {
		/* check requiredby fields */
		for(i = packages; i; i = i->next) {
			pmpkg_t *rmpkg = alpm_list_getdata(i);

			if(rmpkg == NULL) {
				_alpm_log(PM_LOG_DEBUG, "null package found in package list\n");
				continue;
			}
			for(j = alpm_pkg_get_requiredby(rmpkg); j; j = j->next) {
				pmpkg_t *p;
				found = 0;
				if(_alpm_pkg_find(j->data, packages)) {
					/* package also in the remove list, so don't worry about it */
					continue;
				}

				if((p = _alpm_db_get_pkgfromcache(db, j->data)) == NULL) {
					/* hmmm... package isn't installed... */
					continue;
				}
				for(k = alpm_pkg_get_depends(p); k; k = k->next) {
					pmdepend_t *depend = alpm_splitdep(k->data);
					if(depend == NULL) {
						continue;
					}
					/* if rmpkg satisfied this dep, try to find an other satisfier
					 * (which won't be removed)*/
					if(alpm_depcmp(rmpkg, depend)) {
						int satisfied = 0;
						for(l = _alpm_db_get_pkgcache(db); l; l = l->next) {
							pmpkg_t *pkg = l->data;
							if(alpm_depcmp(pkg, depend) && !_alpm_pkg_find(alpm_pkg_get_name(pkg), packages)) {
								_alpm_log(PM_LOG_DEBUG, "checkdeps: dependency '%s' satisfied by installed package '%s'\n",
										(char*)k->data, alpm_pkg_get_name(pkg));
								satisfied = 1;
								break;
							}
						}

						if(!satisfied) {
							_alpm_log(PM_LOG_DEBUG, "checkdeps: found %s which requires %s\n",
									alpm_pkg_get_name(p), alpm_pkg_get_name(rmpkg));
							miss = _alpm_depmiss_new(alpm_pkg_get_name(p),
									PM_DEP_TYPE_DEPEND, depend->mod, depend->name,
									depend->version);
							if(!_alpm_depmiss_isin(miss, baddeps)) {
								baddeps = alpm_list_add(baddeps, miss);
							} else {
								FREE(miss);
							}
						}
					}
					FREE(depend);
				}
			}
		}
	}

	return(baddeps);
}

static int dep_vercmp(const char *version1, pmdepmod_t mod,
		const char *version2)
{
	int equal = 0;

	if(mod == PM_DEP_MOD_ANY) {
		equal = 1;
	} else {
		int cmp = _alpm_versioncmp(version1, version2);
		switch(mod) {
			case PM_DEP_MOD_EQ: equal = (cmp == 0); break;
			case PM_DEP_MOD_GE: equal = (cmp >= 0); break;
			case PM_DEP_MOD_LE: equal = (cmp <= 0); break;
			default: equal = 1; break;
		}
	}
	return(equal);
}

int SYMEXPORT alpm_depcmp(pmpkg_t *pkg, pmdepend_t *dep)
{
	int equal = 0;

	ALPM_LOG_FUNC;

	const char *pkgname = alpm_pkg_get_name(pkg);
	const char *pkgversion = alpm_pkg_get_version(pkg);

	if(strcmp(pkgname, dep->name) == 0
			|| alpm_list_find_str(alpm_pkg_get_provides(pkg), dep->name)) {

		equal = dep_vercmp(pkgversion, dep->mod, dep->version);

		char *mod = "~=";
		switch(dep->mod) {
			case PM_DEP_MOD_EQ: mod = "=="; break;
			case PM_DEP_MOD_GE: mod = ">="; break;
			case PM_DEP_MOD_LE: mod = "<="; break;
			default: break;
		}

		if(strlen(dep->version) > 0) {
			_alpm_log(PM_LOG_DEBUG, "depcmp: %s-%s %s %s-%s => %s\n",
								pkgname, pkgversion,
								mod, dep->name, dep->version, (equal ? "match" : "no match"));
		} else {
			_alpm_log(PM_LOG_DEBUG, "depcmp: %s-%s %s %s => %s\n",
								pkgname, pkgversion,
								mod, dep->name, (equal ? "match" : "no match"));
		}
	}

	return(equal);
}

pmdepend_t SYMEXPORT *alpm_splitdep(const char *depstring)
{
	pmdepend_t *depend;
	char *ptr = NULL;
	char *newstr = NULL;

	if(depstring == NULL) {
		return(NULL);
	}
	newstr = strdup(depstring);
	
	MALLOC(depend, sizeof(pmdepend_t), return(NULL));

	/* Find a version comparator if one exists. If it does, set the type and
	 * increment the ptr accordingly so we can copy the right strings. */
	if((ptr = strstr(newstr, ">="))) {
		depend->mod = PM_DEP_MOD_GE;
		*ptr = '\0';
		ptr += 2;
	} else if((ptr = strstr(newstr, "<="))) {
		depend->mod = PM_DEP_MOD_LE;
		*ptr = '\0';
		ptr += 2;
	} else if((ptr = strstr(newstr, "="))) {
		depend->mod = PM_DEP_MOD_EQ;
		*ptr = '\0';
		ptr += 1;
	} else {
		/* no version specified - copy in the name and return it */
		depend->mod = PM_DEP_MOD_ANY;
		strncpy(depend->name, newstr, PKG_NAME_LEN);
		depend->version[0] = '\0';
		free(newstr);
		return(depend);
	}

	/* if we get here, we have a version comparator, copy the right parts
	 * to the right places */
	strncpy(depend->name, newstr, PKG_NAME_LEN);
	strncpy(depend->version, ptr, PKG_VERSION_LEN);
	free(newstr);

	return(depend);
}

/* These parameters are messy. We check if this package, given a list of
 * targets and a db is safe to remove. We do NOT remove it if it is in the
 * target list, or if if the package was explictly installed and
 * include_explicit == 0 */
static int can_remove_package(pmdb_t *db, pmpkg_t *pkg, alpm_list_t *targets,
		int include_explicit)
{
	alpm_list_t *i;

	if(_alpm_pkg_find(alpm_pkg_get_name(pkg), targets)) {
		return(0);
	}

	if(!include_explicit) {
		/* see if it was explicitly installed */
		if(alpm_pkg_get_reason(pkg) == PM_PKG_REASON_EXPLICIT) {
			_alpm_log(PM_LOG_DEBUG, "excluding %s -- explicitly installed\n",
					alpm_pkg_get_name(pkg));
			return(0);
		}
	}

	/* TODO: checkdeps could be used here, it handles multiple providers
	 * better, but that also makes it slower.
	 * Also this would require to first add the package to the targets list,
	 * then call checkdeps with it, then remove the package from the targets list
	 * if checkdeps detected it would break something */

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

/**
 * @brief Adds unneeded dependencies to an existing list of packages.
 * By unneeded, we mean dependencies that are only required by packages in the
 * target list, so they can be safely removed.
 *
 * @param db package database to do dependency tracing in
 * @param *targs pointer to a list of packages
 * @param include_explicit if 0, explicitly installed packages are not included
 */
void _alpm_recursedeps(pmdb_t *db, alpm_list_t **targs, int include_explicit)
{
	alpm_list_t *i, *j, *k;

	ALPM_LOG_FUNC;

	if(db == NULL || targs == NULL) {
		return;
	}

	/* TODO: the while loop should be removed if we can assume
	 * that alpm_list_add (or another function) adds to the end of the list,
	 * and that the target list is topo sorted (by _alpm_sortbydeps()).
	 */
	int ready = 0;
	while(!ready) {
		ready = 1;
		for(i = *targs; i; i = i->next) {
			pmpkg_t *pkg = i->data;
			for(j = alpm_pkg_get_depends(pkg); j; j = j->next) {
				pmdepend_t *depend = alpm_splitdep(j->data);
				if(depend == NULL) {
					continue;
				}
				for(k = _alpm_db_get_pkgcache(db); k; k = k->next) {
					pmpkg_t *deppkg = k->data;
					if(alpm_depcmp(deppkg,depend)
							&& can_remove_package(db, deppkg, *targs, include_explicit)) {
						_alpm_log(PM_LOG_DEBUG, "adding '%s' to the targets\n",
								alpm_pkg_get_name(deppkg));

						/* add it to the target list */
						*targs = alpm_list_add(*targs, _alpm_pkg_dup(deppkg));
						ready = 0;
					}
				}
				FREE(depend);
			}
		}
	}
}

/* populates *list with packages that need to be installed to satisfy all
 * dependencies (recursive) for syncpkg
 *
 * make sure **list is already initialized
 */
int _alpm_resolvedeps(pmdb_t *local, alpm_list_t *dbs_sync, pmpkg_t *syncpkg,
                      alpm_list_t **list, pmtrans_t *trans, alpm_list_t **data)
{
	alpm_list_t *i, *j, *k;
	alpm_list_t *targ;
	alpm_list_t *deps = NULL;

	ALPM_LOG_FUNC;

	if(local == NULL || dbs_sync == NULL || syncpkg == NULL || list == NULL) {
		return(-1);
	}

	_alpm_log(PM_LOG_DEBUG, "started resolving dependencies\n");
	targ = alpm_list_add(NULL, syncpkg);
	deps = _alpm_checkdeps(local, PM_TRANS_TYPE_ADD, targ);
	alpm_list_free(targ);

	if(deps == NULL) {
		return(0);
	}

	for(i = deps; i; i = i->next) {
		int found = 0;
		pmdepmissing_t *miss = i->data;
		pmdepend_t *missdep = &(miss->depend);
		pmpkg_t *sync = NULL;

		/* check if one of the packages in *list already satisfies this dependency */
		for(j = *list; j && !found; j = j->next) {
			pmpkg_t *sp = j->data;
			if(alpm_depcmp(sp, missdep)) {
				char *missdepstring = alpm_dep_get_string(missdep);
				_alpm_log(PM_LOG_DEBUG, "%s satisfies dependency %s -- skipping\n",
				          alpm_pkg_get_name(sp), missdepstring);
				free(missdepstring);
				found = 1;
			}
		}
		if(found) {
			continue;
		}

		/* find the package in one of the repositories */
		/* check literals */
		for(j = dbs_sync; j && !found; j = j->next) {
			if((sync = _alpm_db_get_pkgfromcache(j->data, missdep->name))) {
				found = alpm_depcmp(sync, missdep);
			}
		}
		/*TODO this autoresolves the first 'satisfier' package... we should fix this
		 * somehow */
		/* check provides */
		for(j = dbs_sync; j && !found; j = j->next) {
			for(k = _alpm_db_get_pkgcache(j->data); k && !found; k = k->next) {
				sync = k->data;
				found = alpm_depcmp(sync, missdep);
			}
		}

		if(!found) {
			char *missdepstring = alpm_dep_get_string(missdep);
			_alpm_log(PM_LOG_ERROR, _("cannot resolve \"%s\", a dependency of \"%s\"\n"),
			          missdepstring, miss->target);
			free(missdepstring);
			if(data) {
				if((miss = malloc(sizeof(pmdepmissing_t))) == NULL) {
					_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %zd bytes\n"), sizeof(pmdepmissing_t));
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
			_alpm_log(PM_LOG_DEBUG, "pulling dependency %s (needed by %s)\n",
					alpm_pkg_get_name(sync), alpm_pkg_get_name(syncpkg));
			*list = alpm_list_add(*list, sync);
			if(_alpm_resolvedeps(local, dbs_sync, sync, list, trans, data)) {
				goto error;
			}
		} else {
			_alpm_log(PM_LOG_ERROR, _("cannot resolve dependencies for \"%s\"\n"), miss->target);
			if(data) {
				if((miss = malloc(sizeof(pmdepmissing_t))) == NULL) {
					_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %zd bytes\n"), sizeof(pmdepmissing_t));
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
	}
	
	_alpm_log(PM_LOG_DEBUG, "finished resolving dependencies\n");

	FREELIST(deps);

	return(0);

error:
	FREELIST(deps);
	return(-1);
}

const char SYMEXPORT *alpm_miss_get_target(pmdepmissing_t *miss)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(miss != NULL, return(NULL));

	return miss->target;
}

pmdeptype_t SYMEXPORT alpm_miss_get_type(pmdepmissing_t *miss)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(-1));
	ASSERT(miss != NULL, return(-1));

	return miss->type;
}

pmdepend_t SYMEXPORT *alpm_miss_get_dep(pmdepmissing_t *miss)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(miss != NULL, return(NULL));

	return &miss->depend;
}

pmdepmod_t SYMEXPORT alpm_dep_get_mod(pmdepend_t *dep)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(-1));
	ASSERT(dep != NULL, return(-1));

	return dep->mod;
}

const char SYMEXPORT *alpm_dep_get_name(pmdepend_t *dep)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(dep != NULL, return(NULL));

	return dep->name;
}

const char SYMEXPORT *alpm_dep_get_version(pmdepend_t *dep)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(dep != NULL, return(NULL));

	return dep->version;
}

/* the return-string must be freed! */
char SYMEXPORT *alpm_dep_get_string(pmdepend_t *dep)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(dep != NULL, return(NULL));

	/* TODO redo the sprintf, change to snprintf and
	 * make it less hacky and dependent on sizeof, etc */
	char *ptr;
	char *depstring;
	MALLOC(depstring, sizeof(pmdepend_t), RET_ERR(PM_ERR_MEMORY, NULL));

	strcpy(depstring, dep->name);
	ptr = depstring + strlen(depstring);
	switch(dep->mod) {
		case PM_DEP_MOD_ANY:
			break;
		case PM_DEP_MOD_EQ:
			sprintf(ptr, "=%s", dep->version);
			break;
		case PM_DEP_MOD_GE:
			sprintf(ptr, ">=%s", dep->version);
			break;
		case PM_DEP_MOD_LE:
			sprintf(ptr, "<=%s", dep->version);
			break;
	}

	return(depstring);
}
/* vim: set ts=2 sw=2 noet: */
