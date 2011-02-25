/*
 *  deps.c
 *
 *  Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
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
#include "graph.h"
#include "package.h"
#include "db.h"
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
	alpm_list_t *i, *j;
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
			if(_alpm_dep_edge(p_i, p_j)) {
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
 * Example (reverse == 0):
 *   A depends on C
 *   B depends on A
 *   Target order is A,B,C,D
 *
 *   Should be re-ordered to C,A,B,D
 *
 * if reverse is > 0, the dependency order will be reversed.
 *
 * This function returns the new alpm_list_t* target list.
 *
 */
alpm_list_t *_alpm_sortbydeps(alpm_list_t *targets, int reverse)
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
			pmgraph_t *nextchild = vertex->childptr->data;
			vertex->childptr = vertex->childptr->next;
			if (nextchild->state == 0) {
				found = 1;
				nextchild->parent = vertex;
				vertex = nextchild;
			}
			else if(nextchild->state == -1) {
				pmpkg_t *vertexpkg = vertex->data;
				pmpkg_t *childpkg = nextchild->data;
				const char *message;

				_alpm_log(PM_LOG_WARNING, _("dependency cycle detected:\n"));
				if(reverse) {
					message =_("%s will be removed after its %s dependency\n");
				} else {
					message =_("%s will be installed before its %s dependency\n");
				}
				_alpm_log(PM_LOG_WARNING, message, vertexpkg->name, childpkg->name);
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

	if(reverse) {
		/* reverse the order */
		alpm_list_t *tmptargs = alpm_list_reverse(newtargs);
		/* free the old one */
		alpm_list_free(newtargs);
		newtargs = tmptargs;
	}

	alpm_list_free_inner(vertices, _alpm_graph_free);
	alpm_list_free(vertices);

	return(newtargs);
}

pmpkg_t *_alpm_find_dep_satisfier(alpm_list_t *pkgs, pmdepend_t *dep)
{
	alpm_list_t *i;

	for(i = pkgs; i; i = alpm_list_next(i)) {
		pmpkg_t *pkg = i->data;
		if(_alpm_depcmp_tolerant(pkg, dep)) {
			return(pkg);
		}
	}
	return(NULL);
}

/** Find a package satisfying a specified dependency.
 * The dependency can include versions with depmod operators.
 * @param pkgs an alpm_list_t* of pmpkg_t where the satisfier will be searched
 * @param depstring package or provision name, versioned or not
 * @return a pmpkg_t* satisfying depstring
 */
pmpkg_t SYMEXPORT *alpm_find_satisfier(alpm_list_t *pkgs, const char *depstring)
{
	pmdepend_t *dep = _alpm_splitdep(depstring);
	pmpkg_t *pkg = _alpm_find_dep_satisfier(pkgs, dep);
	_alpm_dep_free(dep);
	return(pkg);
}

/** Checks dependencies and returns missing ones in a list.
 * Dependencies can include versions with depmod operators.
 * @param pkglist the list of local packages
 * @param reversedeps handles the backward dependencies
 * @param remove an alpm_list_t* of packages to be removed
 * @param upgrade an alpm_list_t* of packages to be upgraded (remove-then-upgrade)
 * @return an alpm_list_t* of pmpkg_t* of pmdepmissing_t pointers.
 */
alpm_list_t SYMEXPORT *alpm_checkdeps(alpm_list_t *pkglist, int reversedeps,
		alpm_list_t *remove, alpm_list_t *upgrade)
{
	alpm_list_t *i, *j;
	alpm_list_t *targets, *dblist = NULL, *modified = NULL;
	alpm_list_t *baddeps = NULL;

	ALPM_LOG_FUNC;

	targets = alpm_list_join(alpm_list_copy(remove), alpm_list_copy(upgrade));
	for(i = pkglist; i; i = i->next) {
		pmpkg_t *pkg = i->data;
		if(_alpm_pkg_find(targets, pkg->name)) {
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
			if(!_alpm_find_dep_satisfier(upgrade, depend) &&
			   !_alpm_find_dep_satisfier(dblist, depend)) {
				/* Unsatisfied dependency in the upgrade list */
				pmdepmissing_t *miss;
				char *missdepstring = alpm_dep_compute_string(depend);
				_alpm_log(PM_LOG_DEBUG, "checkdeps: missing dependency '%s' for package '%s'\n",
						missdepstring, alpm_pkg_get_name(tp));
				free(missdepstring);
				miss = _alpm_depmiss_new(alpm_pkg_get_name(tp), depend, NULL);
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
				pmpkg_t *causingpkg = _alpm_find_dep_satisfier(modified, depend);
				/* we won't break this depend, if it is already broken, we ignore it */
				/* 1. check upgrade list for satisfiers */
				/* 2. check dblist for satisfiers */
				if(causingpkg &&
				   !_alpm_find_dep_satisfier(upgrade, depend) &&
				   !_alpm_find_dep_satisfier(dblist, depend)) {
					pmdepmissing_t *miss;
					char *missdepstring = alpm_dep_compute_string(depend);
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
		int cmp = alpm_pkg_vercmp(version1, version2);
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

/* nodepversion: skip version checking */
static int _depcmp(pmpkg_t *pkg, pmdepend_t *dep, int nodepversion)
{
	alpm_list_t *i;
	int satisfy = 0;
	int depmod;

	if(nodepversion) {
		depmod = PM_DEP_MOD_ANY;
	} else {
		depmod = dep->mod;
	}

	/* check (pkg->name, pkg->version) */
	if(pkg->name_hash && dep->name_hash
			&& pkg->name_hash != dep->name_hash) {
		/* skip more expensive checks */
	} else {
		satisfy = (strcmp(pkg->name, dep->name) == 0
				&& dep_vercmp(pkg->version, depmod, dep->version));
		if(satisfy) {
			return(satisfy);
		}
	}

	/* check provisions, format : "name=version" */
	for(i = alpm_pkg_get_provides(pkg); i && !satisfy; i = i->next) {
		const char *provision = i->data;
		const char *provver = strchr(provision, '=');

		if(provver == NULL) { /* no provision version */
			satisfy = (depmod == PM_DEP_MOD_ANY
					&& strcmp(provision, dep->name) == 0);
		} else {
			/* This is a bit tricker than the old code for performance reasons. To
			 * prevent the need to copy and duplicate strings, strncmp only the name
			 * portion if they are the same length, since there is a version and
			 * operator in play here. Cast is to silence sign conversion warning;
			 * we know provver >= provision if we are here. */
			size_t namelen = (size_t)(provver - provision);
			provver += 1;
			satisfy = (strlen(dep->name) == namelen
					&& strncmp(provision, dep->name, namelen) == 0
					&& dep_vercmp(provver, depmod, dep->version));
		}
	}

	return(satisfy);
}

/* tolerant : respects NODEPVERSION flag */
int _alpm_depcmp_tolerant(pmpkg_t *pkg, pmdepend_t *dep)
{
	int nodepversion = 0;
	int flags = alpm_trans_get_flags();

	if (flags != -1) {
		nodepversion = flags & PM_TRANS_FLAG_NODEPVERSION;
	}

	return(_depcmp(pkg, dep, nodepversion));
}

/* strict : ignores NODEPVERSION flag */
int _alpm_depcmp(pmpkg_t *pkg, pmdepend_t *dep)
{
	return(_depcmp(pkg, dep, 0));
}

pmdepend_t *_alpm_splitdep(const char *depstring)
{
	pmdepend_t *depend;
	const char *ptr, *version = NULL;

	if(depstring == NULL) {
		return(NULL);
	}

	CALLOC(depend, 1, sizeof(pmdepend_t), RET_ERR(PM_ERR_MEMORY, NULL));

	/* Find a version comparator if one exists. If it does, set the type and
	 * increment the ptr accordingly so we can copy the right strings. */
	if((ptr = strstr(depstring, ">="))) {
		depend->mod = PM_DEP_MOD_GE;
		version = ptr + 2;
	} else if((ptr = strstr(depstring, "<="))) {
		depend->mod = PM_DEP_MOD_LE;
		version = ptr + 2;
	} else if((ptr = strstr(depstring, "="))) {
		/* Note: we must do =,<,> checks after <=, >= checks */
		depend->mod = PM_DEP_MOD_EQ;
		version = ptr + 1;
	} else if((ptr = strstr(depstring, "<"))) {
		depend->mod = PM_DEP_MOD_LT;
		version = ptr + 1;
	} else if((ptr = strstr(depstring, ">"))) {
		depend->mod = PM_DEP_MOD_GT;
		version = ptr + 1;
	} else {
		/* no version specified, leave version and ptr NULL */
		depend->mod = PM_DEP_MOD_ANY;
	}

	/* copy the right parts to the right places */
	STRNDUP(depend->name, depstring, ptr - depstring,
			RET_ERR(PM_ERR_MEMORY, NULL));
	depend->name_hash = _alpm_hash_sdbm(depend->name);
	if(version) {
		STRDUP(depend->version, version, RET_ERR(PM_ERR_MEMORY, NULL));
	}

	return(depend);
}

pmdepend_t *_alpm_dep_dup(const pmdepend_t *dep)
{
	pmdepend_t *newdep;
	CALLOC(newdep, 1, sizeof(pmdepend_t), RET_ERR(PM_ERR_MEMORY, NULL));

	STRDUP(newdep->name, dep->name, RET_ERR(PM_ERR_MEMORY, NULL));
	newdep->name_hash = dep->name_hash;
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
	alpm_list_t *i;

	if(_alpm_pkg_find(targets, alpm_pkg_get_name(pkg))) {
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
	for(i = _alpm_db_get_pkgcache(db); i; i = i->next) {
		pmpkg_t *lpkg = i->data;
		if(_alpm_dep_edge(lpkg, pkg) && !_alpm_pkg_find(targets, lpkg->name)) {
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
 * If the input list was topo sorted, the output list will be topo sorted too.
 *
 * @param db package database to do dependency tracing in
 * @param *targs pointer to a list of packages
 * @param include_explicit if 0, explicitly installed packages are not included
 */
void _alpm_recursedeps(pmdb_t *db, alpm_list_t *targs, int include_explicit)
{
	alpm_list_t *i, *j;

	ALPM_LOG_FUNC;

	if(db == NULL || targs == NULL) {
		return;
	}

	for(i = targs; i; i = i->next) {
		pmpkg_t *pkg = i->data;
		for(j = _alpm_db_get_pkgcache(db); j; j = j->next) {
			pmpkg_t *deppkg = j->data;
			if(_alpm_dep_edge(pkg, deppkg)
					&& can_remove_package(db, deppkg, targs, include_explicit)) {
				_alpm_log(PM_LOG_DEBUG, "adding '%s' to the targets\n",
						alpm_pkg_get_name(deppkg));
				/* add it to the target list */
				targs = alpm_list_add(targs, _alpm_pkg_dup(deppkg));
			}
		}
	}
}

/** Find a package satisfying a specified dependency.
 * First look for a literal, going through each db one by one. Then look for
 * providers. The first satisfier found is returned.
 * The dependency can include versions with depmod operators.
 * @param dbs an alpm_list_t* of pmdb_t where the satisfier will be searched
 * @param depstring package or provision name, versioned or not
 * @return a pmpkg_t* satisfying depstring
 */
pmpkg_t SYMEXPORT *alpm_find_dbs_satisfier(alpm_list_t *dbs, const char *depstring)
{
	pmdepend_t *dep;
	pmpkg_t *pkg;

	ASSERT(dbs, return(NULL));

	dep = _alpm_splitdep(depstring);
	ASSERT(dep, return(NULL));
	pkg = _alpm_resolvedep(dep, dbs, NULL, 1);
	_alpm_dep_free(dep);
	return(pkg);
}

/**
 * helper function for resolvedeps: search for dep satisfier in dbs
 *
 * @param dep is the dependency to search for
 * @param dbs are the databases to search
 * @param excluding are the packages to exclude from the search
 * @param prompt if true, will cause an unresolvable dependency to issue an
 *        interactive prompt asking whether the package should be removed from
 *        the transaction or the transaction aborted; if false, simply returns
 *        an error code without prompting
 * @return the resolved package
 **/
pmpkg_t *_alpm_resolvedep(pmdepend_t *dep, alpm_list_t *dbs,
		alpm_list_t *excluding, int prompt)
{
	alpm_list_t *i, *j;
	int ignored = 0;

	alpm_list_t *providers = NULL;
	int count;

	/* 1. literals */
	for(i = dbs; i; i = i->next) {
		pmpkg_t *pkg = _alpm_db_get_pkgfromcache(i->data, dep->name);
		if(pkg && _alpm_depcmp_tolerant(pkg, dep) && !_alpm_pkg_find(excluding, pkg->name)) {
			if(_alpm_pkg_should_ignore(pkg)) {
				int install = 0;
				if (prompt) {
					QUESTION(handle->trans, PM_TRANS_CONV_INSTALL_IGNOREPKG, pkg,
							 NULL, NULL, &install);
				} else {
					_alpm_log(PM_LOG_WARNING, _("ignoring package %s-%s\n"), pkg->name, pkg->version);
				}
				if(!install) {
					ignored = 1;
					continue;
				}
			}
			return(pkg);
		}
	}
	/* 2. satisfiers (skip literals here) */
	for(i = dbs; i; i = i->next) {
		for(j = _alpm_db_get_pkgcache(i->data); j; j = j->next) {
			pmpkg_t *pkg = j->data;
			if(_alpm_depcmp_tolerant(pkg, dep) && strcmp(pkg->name, dep->name) != 0 &&
			             !_alpm_pkg_find(excluding, pkg->name)) {
				if(_alpm_pkg_should_ignore(pkg)) {
					int install = 0;
					if (prompt) {
						QUESTION(handle->trans, PM_TRANS_CONV_INSTALL_IGNOREPKG,
									pkg, NULL, NULL, &install);
					} else {
						_alpm_log(PM_LOG_WARNING, _("ignoring package %s-%s\n"), pkg->name, pkg->version);
					}
					if(!install) {
						ignored = 1;
						continue;
					}
				}
				_alpm_log(PM_LOG_DEBUG, "provider found (%s provides %s)\n",
						pkg->name, dep->name);
				providers = alpm_list_add(providers, pkg);
				/* keep looking for other providers in the all dbs */
			}
		}
	}

	/* first check if one provider is already installed locally */
	for(i = providers; i; i = i->next) {
		pmpkg_t *pkg = i->data;
		if (_alpm_pkghash_find(_alpm_db_get_pkgcache_hash(handle->db_local), pkg->name)) {
			alpm_list_free(providers);
			return(pkg);
		}
	}
	count = alpm_list_count(providers);
	if (count >= 1) {
		/* default to first provider if there is no QUESTION callback */
		int index = 0;
		if(count > 1) {
			/* if there is more than one provider, we ask the user */
			QUESTION(handle->trans, PM_TRANS_CONV_SELECT_PROVIDER,
					providers, dep, NULL, &index);
		}
		if(index >= 0 && index < count) {
			pmpkg_t *pkg = alpm_list_getdata(alpm_list_nth(providers, index));
			alpm_list_free(providers);
			return(pkg);
		}
		alpm_list_free(providers);
		providers = NULL;
	}

	if(ignored) { /* resolvedeps will override these */
		pm_errno = PM_ERR_PKG_IGNORED;
	} else {
		pm_errno = PM_ERR_PKG_NOT_FOUND;
	}
	return(NULL);
}

/* Computes resolvable dependencies for a given package and adds that package
 * and those resolvable dependencies to a list.
 *
 * @param localpkgs is the list of local packages
 * @param dbs_sync are the sync databases
 * @param pkg is the package to resolve
 * @param packages is a pointer to a list of packages which will be
 *        searched first for any dependency packages needed to complete the
 *        resolve, and to which will be added any [pkg] and all of its
 *        dependencies not already on the list
 * @param remove is the set of packages which will be removed in this
 *        transaction
 * @param data returns the dependency which could not be satisfied in the
 *        event of an error
 * @return 0 on success, with [pkg] and all of its dependencies not already on
 *         the [*packages] list added to that list, or -1 on failure due to an
 *         unresolvable dependency, in which case the [*packages] list will be
 *         unmodified by this function
 */
int _alpm_resolvedeps(alpm_list_t *localpkgs, alpm_list_t *dbs_sync, pmpkg_t *pkg,
                      alpm_list_t *preferred, alpm_list_t **packages,
                      alpm_list_t *remove, alpm_list_t **data)
{
	int ret = 0;
	alpm_list_t *i, *j;
	alpm_list_t *targ;
	alpm_list_t *deps = NULL;
	alpm_list_t *packages_copy;

	ALPM_LOG_FUNC;

	if(_alpm_pkg_find(*packages, pkg->name) != NULL) {
		return(0);
	}

	/* Create a copy of the packages list, so that it can be restored
	   on error */
	packages_copy = alpm_list_copy(*packages);
	/* [pkg] has not already been resolved into the packages list, so put it
	   on that list */
	*packages = alpm_list_add(*packages, pkg);

	_alpm_log(PM_LOG_DEBUG, "started resolving dependencies\n");
	for(i = alpm_list_last(*packages); i; i = i->next) {
		pmpkg_t *tpkg = i->data;
		targ = alpm_list_add(NULL, tpkg);
		deps = alpm_checkdeps(localpkgs, 0, remove, targ);
		alpm_list_free(targ);

		for(j = deps; j; j = j->next) {
			pmdepmissing_t *miss = j->data;
			pmdepend_t *missdep = alpm_miss_get_dep(miss);
			/* check if one of the packages in the [*packages] list already satisfies
			 * this dependency */
			if(_alpm_find_dep_satisfier(*packages, missdep)) {
				_alpm_depmiss_free(miss);
				continue;
			}
			/* check if one of the packages in the [preferred] list already satisfies
			 * this dependency */
			pmpkg_t *spkg = _alpm_find_dep_satisfier(preferred, missdep);
			if(!spkg) {
				/* find a satisfier package in the given repositories */
				spkg = _alpm_resolvedep(missdep, dbs_sync, *packages, 0);
			}
			if(!spkg) {
				pm_errno = PM_ERR_UNSATISFIED_DEPS;
				char *missdepstring = alpm_dep_compute_string(missdep);
				_alpm_log(PM_LOG_WARNING,
						_("cannot resolve \"%s\", a dependency of \"%s\"\n"),
						missdepstring, tpkg->name);
				free(missdepstring);
				if(data) {
					*data = alpm_list_add(*data, miss);
				}
				ret = -1;
			} else {
				_alpm_log(PM_LOG_DEBUG, "pulling dependency %s (needed by %s)\n",
						alpm_pkg_get_name(spkg), alpm_pkg_get_name(tpkg));
				*packages = alpm_list_add(*packages, spkg);
				_alpm_depmiss_free(miss);
			}
		}
		alpm_list_free(deps);
	}

	if(ret != 0) {
		alpm_list_free(*packages);
		*packages = packages_copy;
	} else {
		alpm_list_free(packages_copy);
	}
	_alpm_log(PM_LOG_DEBUG, "finished resolving dependencies\n");
	return(ret);
}

/* Does pkg1 depend on pkg2, ie. does pkg2 satisfy a dependency of pkg1? */
int _alpm_dep_edge(pmpkg_t *pkg1, pmpkg_t *pkg2)
{
	alpm_list_t *i;
	for(i = alpm_pkg_get_depends(pkg1); i; i = i->next) {
		if(_alpm_depcmp_tolerant(pkg2, i->data)) {
			return(1);
		}
	}
	return(0);
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
char SYMEXPORT *alpm_dep_compute_string(const pmdepend_t *dep)
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
