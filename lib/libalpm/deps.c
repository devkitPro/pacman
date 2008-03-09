/*
 *  deps.c
 *
 *  Copyright (c) 2002-2007 by Judd Vinet <jvinet@zeroflux.org>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
#include "graph.h"
#include "package.h"
#include "db.h"
#include "cache.h"
#include "handle.h"

void _alpm_dep_free(pmdepend_t *dep)
{
	FREE(dep->name);
	FREE(dep->version);
	FREE(dep);
}

pmdepmissing_t *_alpm_depmiss_new(const char *target, pmdepend_t *dep,
		const char *causingpkg)
{
	pmdepmissing_t *miss;

	ALPM_LOG_FUNC;

	MALLOC(miss, sizeof(pmdepmissing_t), RET_ERR(PM_ERR_MEMORY, NULL));

	STRDUP(miss->target, target, RET_ERR(PM_ERR_MEMORY, NULL));
	miss->depend = _alpm_dep_dup(dep);
	STRDUP(miss->causingpkg, causingpkg, RET_ERR(PM_ERR_MEMORY, NULL));

	return(miss);
}

void _alpm_depmiss_free(pmdepmissing_t *miss)
{
	_alpm_dep_free(miss->depend);
	FREE(miss->target);
	FREE(miss->causingpkg);
	FREE(miss);
}

/* Convert a list of pmpkg_t * to a graph structure,
 * with a edge for each dependency.
 * Returns a list of vertices (one vertex = one package)
 * (used by alpm_sortbydeps)
 */
static alpm_list_t *dep_graph_init(alpm_list_t *targets)
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
		/* TODO this should be somehow combined with alpm_checkdeps */
		for(j = vertices; j; j = j->next) {
			pmgraph_t *vertex_j = j->data;
			pmpkg_t *p_j = vertex_j->data;
			int child = 0;
			for(k = alpm_pkg_get_depends(p_i); k && !child; k = k->next) {
				pmdepend_t *depend = k->data;
				child = alpm_depcmp(p_j, depend);
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

	vertices = dep_graph_init(targets);

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
				pmpkg_t *vertexpkg = vertex->data;
				pmpkg_t *childpkg = nextchild->data;
				_alpm_log(PM_LOG_WARNING, _("dependency cycle detected:\n"));
				if(mode == PM_TRANS_TYPE_REMOVE) {
					_alpm_log(PM_LOG_WARNING, _("%s will be removed after its %s dependency\n"), vertexpkg->name, childpkg->name);
				} else {
					_alpm_log(PM_LOG_WARNING, _("%s will be installed before its %s dependency\n"), vertexpkg->name, childpkg->name);
				}
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

/* Little helper function for alpm_list_find */
static int satisfycmp(const void *pkg, const void *depend)
{
	return(!alpm_depcmp((pmpkg_t*) pkg, (pmdepend_t*) depend));
}

/** Checks dependencies and returns missing ones in a list.
 * Dependencies can include versions with depmod operators.
 * @param db pointer to the local package database
 * @param targets an alpm_list_t* of dependencies strings to satisfy
 * @return an alpm_list_t* of missing dependencies strings
 */
alpm_list_t SYMEXPORT *alpm_deptest(pmdb_t *db, alpm_list_t *targets)
{
	alpm_list_t *i, *ret = NULL;

	for(i = targets; i; i = alpm_list_next(i)) {
		pmdepend_t *dep;
		char *target;

		target = alpm_list_getdata(i);
		dep = _alpm_splitdep(target);

		if(!alpm_list_find(_alpm_db_get_pkgcache(db), dep, satisfycmp)) {
			ret = alpm_list_add(ret, target);
		}
		_alpm_dep_free(dep);
	}
	return(ret);
}

/** Checks dependencies and returns missing ones in a list.
 * Dependencies can include versions with depmod operators.
 * @param db pointer to the local package database
 * @param reversedeps handles the backward dependencies
 * @param remove an alpm_list_t* of packages to be removed
 * @param upgrade an alpm_list_t* of packages to be upgraded (remove-then-upgrade)
 * @return an alpm_list_t* of pmpkg_t* of missing_t pointers.
 */
alpm_list_t SYMEXPORT *alpm_checkdeps(pmdb_t *db, int reversedeps,
		alpm_list_t *remove, alpm_list_t *upgrade)
{
	alpm_list_t *i, *j;
	alpm_list_t *targets, *dblist = NULL, *modified = NULL;
	alpm_list_t *baddeps = NULL;
	pmdepmissing_t *miss = NULL;

	ALPM_LOG_FUNC;

	if(db == NULL) {
		return(NULL);
	}

	targets = alpm_list_join(alpm_list_copy(remove), alpm_list_copy(upgrade));
	for(i = _alpm_db_get_pkgcache(db); i; i = i->next) {
		void *pkg = i->data;
		if(alpm_list_find(targets, pkg, _alpm_pkg_cmp)) {
			modified = alpm_list_add(modified, pkg);
		} else {
			dblist = alpm_list_add(dblist, pkg);
		}
	}
	alpm_list_free(targets);

	/* look for unsatisfied dependencies of the upgrade list */
	for(i = upgrade; i; i = i->next) {
		pmpkg_t *tp = i->data;
		_alpm_log(PM_LOG_DEBUG, "checkdeps: package %s-%s\n",
				alpm_pkg_get_name(tp), alpm_pkg_get_version(tp));

		for(j = alpm_pkg_get_depends(tp); j; j = j->next) {
			pmdepend_t *depend = j->data;
			/* 1. we check the upgrade list */
			/* 2. we check database for untouched satisfying packages */
			if(!alpm_list_find(upgrade, depend, satisfycmp) &&
			   !alpm_list_find(dblist, depend, satisfycmp)) {
				/* Unsatisfied dependency in the upgrade list */
				char *missdepstring = alpm_dep_get_string(depend);
				_alpm_log(PM_LOG_DEBUG, "checkdeps: missing dependency '%s' for package '%s'\n",
						missdepstring, alpm_pkg_get_name(tp));
				free(missdepstring);
				miss = _alpm_depmiss_new(alpm_pkg_get_name(tp), depend, "");
				baddeps = alpm_list_add(baddeps, miss);
			}
		}
	}

	if(reversedeps) {
		/* reversedeps handles the backwards dependencies, ie,
		 * the packages listed in the requiredby field. */
		for(i = dblist; i; i = i->next) {
			pmpkg_t *lp = i->data;
			for(j = alpm_pkg_get_depends(lp); j; j = j->next) {
				pmdepend_t *depend = j->data;
				pmpkg_t *causingpkg = alpm_list_find(modified, depend, satisfycmp);
				/* we won't break this depend, if it is already broken, we ignore it */
				/* 1. check upgrade list for satisfiers */
				/* 2. check dblist for satisfiers */
				if(causingpkg &&
				   !alpm_list_find(upgrade, depend, satisfycmp) &&
				   !alpm_list_find(dblist, depend, satisfycmp)) {
					char *missdepstring = alpm_dep_get_string(depend);
					_alpm_log(PM_LOG_DEBUG, "checkdeps: transaction would break '%s' dependency of '%s'\n",
							missdepstring, alpm_pkg_get_name(lp));
					free(missdepstring);
					miss = _alpm_depmiss_new(lp->name, depend, alpm_pkg_get_name(causingpkg));
					baddeps = alpm_list_add(baddeps, miss);
				}
			}
		}
	}
	alpm_list_free(modified);
	alpm_list_free(dblist);

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
			case PM_DEP_MOD_LT: equal = (cmp < 0); break;
			case PM_DEP_MOD_GT: equal = (cmp > 0); break;
			default: equal = 1; break;
		}
	}
	return(equal);
}

int SYMEXPORT alpm_depcmp(pmpkg_t *pkg, pmdepend_t *dep)
{
	alpm_list_t *i;

	ALPM_LOG_FUNC;

	const char *pkgname = alpm_pkg_get_name(pkg);
	const char *pkgversion = alpm_pkg_get_version(pkg);
	int satisfy = 0;

	/* check (pkg->name, pkg->version) */
	satisfy = (strcmp(pkgname, dep->name) == 0
			&& dep_vercmp(pkgversion, dep->mod, dep->version));

	/* check provisions, format : "name=version" */
	for(i = alpm_pkg_get_provides(pkg); i && !satisfy; i = i->next) {
		char *provname = strdup(i->data);
		char *provver = strchr(provname, '=');

		if(provver == NULL) { /* no provision version */
			satisfy = (dep->mod == PM_DEP_MOD_ANY
					&& strcmp(provname, dep->name) == 0);
		} else {
			*provver = '\0';
			provver += 1;
			satisfy = (strcmp(provname, dep->name) == 0
					&& dep_vercmp(provver, dep->mod, dep->version));
		}
		free(provname);
	}

	return(satisfy);
}

pmdepend_t *_alpm_splitdep(const char *depstring)
{
	pmdepend_t *depend;
	char *ptr = NULL;
	char *newstr = NULL;

	if(depstring == NULL) {
		return(NULL);
	}
	STRDUP(newstr, depstring, RET_ERR(PM_ERR_MEMORY, NULL));

	CALLOC(depend, 1, sizeof(pmdepend_t), RET_ERR(PM_ERR_MEMORY, NULL));

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
	} else if((ptr = strstr(newstr, "="))) { /* Note: we must do =,<,> checks after <=, >= checks */
		depend->mod = PM_DEP_MOD_EQ;
		*ptr = '\0';
		ptr += 1;
	} else if((ptr = strstr(newstr, "<"))) {
		depend->mod = PM_DEP_MOD_LT;
		*ptr = '\0';
		ptr += 1;
	} else if((ptr = strstr(newstr, ">"))) {
		depend->mod = PM_DEP_MOD_GT;
		*ptr = '\0';
		ptr += 1;
	} else {
		/* no version specified - copy the name and return it */
		depend->mod = PM_DEP_MOD_ANY;
		STRDUP(depend->name, newstr, RET_ERR(PM_ERR_MEMORY, NULL));
		depend->version = NULL;
		free(newstr);
		return(depend);
	}

	/* if we get here, we have a version comparator, copy the right parts
	 * to the right places */
	STRDUP(depend->name, newstr, RET_ERR(PM_ERR_MEMORY, NULL));
	STRDUP(depend->version, ptr, RET_ERR(PM_ERR_MEMORY, NULL));
	free(newstr);

	return(depend);
}

pmdepend_t *_alpm_dep_dup(const pmdepend_t *dep)
{
	pmdepend_t *newdep;
	CALLOC(newdep, 1, sizeof(pmdepend_t), RET_ERR(PM_ERR_MEMORY, NULL));

	STRDUP(newdep->name, dep->name, RET_ERR(PM_ERR_MEMORY, NULL));
	STRDUP(newdep->version, dep->version, RET_ERR(PM_ERR_MEMORY, NULL));
	newdep->mod = dep->mod;

	return(newdep);
}

/* These parameters are messy. We check if this package, given a list of
 * targets and a db is safe to remove. We do NOT remove it if it is in the
 * target list, or if if the package was explictly installed and
 * include_explicit == 0 */
static int can_remove_package(pmdb_t *db, pmpkg_t *pkg, alpm_list_t *targets,
		int include_explicit)
{
	alpm_list_t *i, *requiredby;

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
	requiredby = alpm_pkg_compute_requiredby(pkg);
	for(i = requiredby; i; i = i->next) {
		pmpkg_t *reqpkg = _alpm_db_get_pkgfromcache(db, i->data);
		if(reqpkg && !_alpm_pkg_find(alpm_pkg_get_name(reqpkg), targets)) {
			FREELIST(requiredby);
			return(0);
		}
	}
	FREELIST(requiredby);

	/* it's ok to remove */
	return(1);
}

/**
 * @brief Adds unneeded dependencies to an existing list of packages.
 * By unneeded, we mean dependencies that are only required by packages in the
 * target list, so they can be safely removed.
 * If the input list was topo sorted, the output list will be topo sorted too.
 *
 * @param db package database to do dependency tracing in
 * @param *targs pointer to a list of packages
 * @param include_explicit if 0, explicitly installed packages are not included
 */
void _alpm_recursedeps(pmdb_t *db, alpm_list_t *targs, int include_explicit)
{
	alpm_list_t *i, *j, *k;

	ALPM_LOG_FUNC;

	if(db == NULL || targs == NULL) {
		return;
	}

	for(i = targs; i; i = i->next) {
		pmpkg_t *pkg = i->data;
		for(j = alpm_pkg_get_depends(pkg); j; j = j->next) {
			pmdepend_t *depend = j->data;

			for(k = _alpm_db_get_pkgcache(db); k; k = k->next) {
				pmpkg_t *deppkg = k->data;
				if(alpm_depcmp(deppkg,depend)
						&& can_remove_package(db, deppkg, targs, include_explicit)) {
					_alpm_log(PM_LOG_DEBUG, "adding '%s' to the targets\n",
							alpm_pkg_get_name(deppkg));
						/* add it to the target list */
					targs = alpm_list_add(targs, _alpm_pkg_dup(deppkg));
				}
			}
		}
	}
}

/* populates *list with packages that need to be installed to satisfy all
 * dependencies (recursive) for syncpkg
 *
 * @param remove contains packages elected for removal
 * make sure **list is already initialized
 */
int _alpm_resolvedeps(pmdb_t *local, alpm_list_t *dbs_sync, pmpkg_t *syncpkg,
                      alpm_list_t **list, alpm_list_t *remove, pmtrans_t *trans, alpm_list_t **data)
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
	deps = alpm_checkdeps(local, 0, remove, targ);
	alpm_list_free(targ);

	if(deps == NULL) {
		return(0);
	}

	for(i = deps; i; i = i->next) {
		int found = 0;
		pmdepmissing_t *miss = i->data;
		pmdepend_t *missdep = alpm_miss_get_dep(miss);
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
			sync = _alpm_db_get_pkgfromcache(j->data, missdep->name);
			if(!sync) {
				continue;
			}
			found = alpm_depcmp(sync, missdep) && !_alpm_pkg_find(alpm_pkg_get_name(sync), remove);
			if(!found) {
				continue;
			}
			/* If package is in the ignorepkg list, ask before we pull it */
			if(_alpm_pkg_should_ignore(sync)) {
				pmpkg_t *dummypkg = _alpm_pkg_new(miss->target, NULL);
				QUESTION(trans, PM_TRANS_CONV_INSTALL_IGNOREPKG, dummypkg, sync, NULL, &found);
				_alpm_pkg_free(dummypkg);
			}
		}
		/*TODO this autoresolves the first 'satisfier' package... we should fix this
		 * somehow */
		/* check provides */
		/* we don't check literals again to avoid duplicated PM_TRANS_CONV_INSTALL_IGNOREPKG messages */
		for(j = dbs_sync; j && !found; j = j->next) {
			for(k = _alpm_db_get_pkgcache(j->data); k && !found; k = k->next) {
				sync = k->data;
				if(!sync) {
					continue;
				}
				found = alpm_depcmp(sync, missdep) && strcmp(sync->name, missdep->name)
					&& !_alpm_pkg_find(alpm_pkg_get_name(sync), remove);
				if(!found) {
					continue;
				}
				if(_alpm_pkg_should_ignore(sync)) {
					pmpkg_t *dummypkg = _alpm_pkg_new(miss->target, NULL);
					QUESTION(trans, PM_TRANS_CONV_INSTALL_IGNOREPKG, dummypkg, sync, NULL, &found);
					_alpm_pkg_free(dummypkg);
				}
			}
		}

		if(!found) {
			char *missdepstring = alpm_dep_get_string(missdep);
			_alpm_log(PM_LOG_ERROR, _("cannot resolve \"%s\", a dependency of \"%s\"\n"),
			          missdepstring, miss->target);
			free(missdepstring);
			if(data) {
				MALLOC(miss, sizeof(pmdepmissing_t),/*nothing*/);
				if(!miss) {
					pm_errno = PM_ERR_MEMORY;
					FREELIST(*data);
					goto error;
				}
				*miss = *(pmdepmissing_t *)i->data;
				*data = alpm_list_add(*data, miss);
			}
			pm_errno = PM_ERR_UNSATISFIED_DEPS;
			goto error;
		} else {
			_alpm_log(PM_LOG_DEBUG, "pulling dependency %s (needed by %s)\n",
					alpm_pkg_get_name(sync), alpm_pkg_get_name(syncpkg));
			*list = alpm_list_add(*list, sync);
			if(_alpm_resolvedeps(local, dbs_sync, sync, list, remove, trans, data)) {
				goto error;
			}
		}
	}

	_alpm_log(PM_LOG_DEBUG, "finished resolving dependencies\n");

	alpm_list_free_inner(deps, (alpm_list_fn_free)_alpm_depmiss_free);
	alpm_list_free(deps);

	return(0);

error:
	FREELIST(deps);
	return(-1);
}

const char SYMEXPORT *alpm_miss_get_target(const pmdepmissing_t *miss)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(miss != NULL, return(NULL));

	return(miss->target);
}

const char SYMEXPORT *alpm_miss_get_causingpkg(const pmdepmissing_t *miss)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(miss != NULL, return(NULL));

	return miss->causingpkg;
}

pmdepend_t SYMEXPORT *alpm_miss_get_dep(pmdepmissing_t *miss)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(miss != NULL, return(NULL));

	return(miss->depend);
}

pmdepmod_t SYMEXPORT alpm_dep_get_mod(const pmdepend_t *dep)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(dep != NULL, return(-1));

	return(dep->mod);
}

const char SYMEXPORT *alpm_dep_get_name(const pmdepend_t *dep)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(dep != NULL, return(NULL));

	return(dep->name);
}

const char SYMEXPORT *alpm_dep_get_version(const pmdepend_t *dep)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(dep != NULL, return(NULL));

	return(dep->version);
}

/** Reverse of splitdep; make a dep string from a pmdepend_t struct.
 * The string must be freed!
 * @param dep the depend to turn into a string
 * @return a string-formatted dependency with operator if necessary
 */
char SYMEXPORT *alpm_dep_get_string(const pmdepend_t *dep)
{
	char *name, *opr, *ver, *str = NULL;
	size_t len;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(dep != NULL, return(NULL));

	if(dep->name) {
		name = dep->name;
	} else {
		name = "";
	}

	switch(dep->mod) {
		case PM_DEP_MOD_ANY:
			opr = "";
			break;
		case PM_DEP_MOD_GE:
			opr = ">=";
			break;
		case PM_DEP_MOD_LE:
			opr = "<=";
			break;
		case PM_DEP_MOD_EQ:
			opr = "=";
			break;
		case PM_DEP_MOD_LT:
			opr = "<";
			break;
		case PM_DEP_MOD_GT:
			opr = ">";
			break;
		default:
			opr = "";
			break;
	}

	if(dep->version) {
		ver = dep->version;
	} else {
		ver = "";
	}

	/* we can always compute len and print the string like this because opr
	 * and ver will be empty when PM_DEP_MOD_ANY is the depend type. the
	 * reassignments above also ensure we do not do a strlen(NULL). */
	len = strlen(name) + strlen(opr) + strlen(ver) + 1;
	MALLOC(str, len, RET_ERR(PM_ERR_MEMORY, NULL));
	snprintf(str, len, "%s%s%s", name, opr, ver);

	return(str);
}
/* vim: set ts=2 sw=2 noet: */
