/*
 *  sync.c
 *
 *  Copyright (c) 2006-2010 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2005 by Christian Hamar <krics@linuxforum.hu>
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

#include <sys/types.h> /* off_t */
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h> /* intmax_t */
#include <unistd.h>
#include <time.h>
#include <dirent.h>

/* libalpm */
#include "sync.h"
#include "alpm_list.h"
#include "log.h"
#include "package.h"
#include "db.h"
#include "deps.h"
#include "conflict.h"
#include "trans.h"
#include "add.h"
#include "util.h"
#include "handle.h"
#include "alpm.h"
#include "dload.h"
#include "delta.h"
#include "remove.h"

/** Check for new version of pkg in sync repos
 * (only the first occurrence is considered in sync)
 */
pmpkg_t SYMEXPORT *alpm_sync_newversion(pmpkg_t *pkg, alpm_list_t *dbs_sync)
{
	ASSERT(pkg != NULL, return(NULL));

	alpm_list_t *i;
	pmpkg_t *spkg = NULL;

	for(i = dbs_sync; !spkg && i; i = i->next) {
		spkg = _alpm_db_get_pkgfromcache(i->data, alpm_pkg_get_name(pkg));
	}

	if(spkg == NULL) {
		_alpm_log(PM_LOG_DEBUG, "'%s' not found in sync db => no upgrade\n",
				alpm_pkg_get_name(pkg));
		return(NULL);
	}

	/* compare versions and see if spkg is an upgrade */
	if(_alpm_pkg_compare_versions(spkg, pkg) > 0) {
		_alpm_log(PM_LOG_DEBUG, "new version of '%s' found (%s => %s)\n",
					alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg),
					alpm_pkg_get_version(spkg));
		return(spkg);
	}
	/* spkg is not an upgrade */
	return(NULL);
}

/** Search for packages to upgrade and add them to the transaction.
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_sync_sysupgrade(int enable_downgrade)
{
	alpm_list_t *i, *j, *k;
	pmtrans_t *trans;
	pmdb_t *db_local;
	alpm_list_t *dbs_sync;

	ALPM_LOG_FUNC;

	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	trans = handle->trans;
	db_local = handle->db_local;
	dbs_sync = handle->dbs_sync;
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_INITIALIZED, RET_ERR(PM_ERR_TRANS_NOT_INITIALIZED, -1));

	_alpm_log(PM_LOG_DEBUG, "checking for package upgrades\n");
	for(i = _alpm_db_get_pkgcache(db_local); i; i = i->next) {
		pmpkg_t *lpkg = i->data;

		if(_alpm_pkg_find(trans->add, lpkg->name)) {
			_alpm_log(PM_LOG_DEBUG, "%s is already in the target list -- skipping\n", lpkg->name);
			continue;
		}

		/* Search for literal then replacers in each sync database.
		 * If found, don't check other databases */
		for(j = dbs_sync; j; j = j->next) {
			pmdb_t *sdb = j->data;
			/* Check sdb */
			pmpkg_t *spkg = _alpm_db_get_pkgfromcache(sdb, lpkg->name);
			if(spkg) { /* 1. literal was found in sdb */
				int cmp = _alpm_pkg_compare_versions(spkg, lpkg);
				if(cmp > 0) {
					_alpm_log(PM_LOG_DEBUG, "new version of '%s' found (%s => %s)\n",
								lpkg->name, lpkg->version, spkg->version);
					/* check IgnorePkg/IgnoreGroup */
					if(_alpm_pkg_should_ignore(spkg) || _alpm_pkg_should_ignore(lpkg)) {
						_alpm_log(PM_LOG_WARNING, _("%s: ignoring package upgrade (%s => %s)\n"),
										lpkg->name, lpkg->version, spkg->version);
					} else {
						_alpm_log(PM_LOG_DEBUG, "adding package %s-%s to the transaction targets\n",
												spkg->name, spkg->version);
						trans->add = alpm_list_add(trans->add, spkg);
					}
				} else if(cmp < 0) {
					if(enable_downgrade) {
						/* check IgnorePkg/IgnoreGroup */
						if(_alpm_pkg_should_ignore(spkg) || _alpm_pkg_should_ignore(lpkg)) {
							_alpm_log(PM_LOG_WARNING, _("%s: ignoring package downgrade (%s => %s)\n"),
											lpkg->name, lpkg->version, spkg->version);
						} else {
							_alpm_log(PM_LOG_WARNING, _("%s: downgrading from version %s to version %s\n"),
											lpkg->name, lpkg->version, spkg->version);
							trans->add = alpm_list_add(trans->add, spkg);
						}
					} else {
						_alpm_log(PM_LOG_WARNING, _("%s: local (%s) is newer than %s (%s)\n"),
								lpkg->name, lpkg->version, sdb->treename, spkg->version);
					}
				}
				break; /* jump to next local package */
			} else { /* 2. search for replacers in sdb */
				int found = 0;
				for(k = _alpm_db_get_pkgcache(sdb); k; k = k->next) {
					spkg = k->data;
					if(alpm_list_find_str(alpm_pkg_get_replaces(spkg), lpkg->name)) {
						found = 1;
						/* check IgnorePkg/IgnoreGroup */
						if(_alpm_pkg_should_ignore(spkg) || _alpm_pkg_should_ignore(lpkg)) {
							_alpm_log(PM_LOG_WARNING, _("ignoring package replacement (%s-%s => %s-%s)\n"),
										lpkg->name, lpkg->version, spkg->name, spkg->version);
							continue;
						}

						int doreplace = 0;
						QUESTION(trans, PM_TRANS_CONV_REPLACE_PKG, lpkg, spkg, sdb->treename, &doreplace);
						if(!doreplace) {
							continue;
						}

						/* If spkg is already in the target list, we append lpkg to spkg's removes list */
						pmpkg_t *tpkg = _alpm_pkg_find(trans->add, spkg->name);
						if(tpkg) {
							/* sanity check, multiple repos can contain spkg->name */
							if(tpkg->origin_data.db != sdb) {
								_alpm_log(PM_LOG_WARNING, _("cannot replace %s by %s\n"),
													lpkg->name, spkg->name);
								continue;
							}
							_alpm_log(PM_LOG_DEBUG, "appending %s to the removes list of %s\n",
												lpkg->name, tpkg->name);
							tpkg->removes = alpm_list_add(tpkg->removes, lpkg);
							/* check the to-be-replaced package's reason field */
							if(alpm_pkg_get_reason(lpkg) == PM_PKG_REASON_EXPLICIT) {
								tpkg->reason = PM_PKG_REASON_EXPLICIT;
							}
						} else { /* add spkg to the target list */
							/* copy over reason */
							spkg->reason = alpm_pkg_get_reason(lpkg);
							spkg->removes = alpm_list_add(NULL, lpkg);
							_alpm_log(PM_LOG_DEBUG, "adding package %s-%s to the transaction targets\n",
													spkg->name, spkg->version);
							trans->add = alpm_list_add(trans->add, spkg);
						}
					}
				}
				if(found) {
					break; /* jump to next local package */
				}
			}
		}
	}

	return(0);
}

static int sync_pkg(pmpkg_t *spkg, alpm_list_t *pkg_list)
{
	pmtrans_t *trans;
	pmdb_t *db_local;
	pmpkg_t *local;

	ALPM_LOG_FUNC;

	trans = handle->trans;
	db_local = handle->db_local;

	if(_alpm_pkg_find(pkg_list, alpm_pkg_get_name(spkg))) {
		RET_ERR(PM_ERR_TRANS_DUP_TARGET, -1);
	}

	local = _alpm_db_get_pkgfromcache(db_local, alpm_pkg_get_name(spkg));
	if(local) {
		int cmp = _alpm_pkg_compare_versions(spkg, local);
		if(cmp == 0) {
			if(trans->flags & PM_TRANS_FLAG_NEEDED) {
				/* with the NEEDED flag, packages up to date are not reinstalled */
				_alpm_log(PM_LOG_WARNING, _("%s-%s is up to date -- skipping\n"),
						alpm_pkg_get_name(local), alpm_pkg_get_version(local));
				return(0);
			} else {
				_alpm_log(PM_LOG_WARNING, _("%s-%s is up to date -- reinstalling\n"),
						alpm_pkg_get_name(local), alpm_pkg_get_version(local));

			}
		} else if(cmp < 0) {
			/* local version is newer */
			_alpm_log(PM_LOG_WARNING, _("downgrading package %s (%s => %s)\n"),
					alpm_pkg_get_name(local), alpm_pkg_get_version(local),
					alpm_pkg_get_version(spkg));
		}
	}

	/* add the package to the transaction */
	spkg->reason = PM_PKG_REASON_EXPLICIT;
	_alpm_log(PM_LOG_DEBUG, "adding package %s-%s to the transaction targets\n",
						alpm_pkg_get_name(spkg), alpm_pkg_get_version(spkg));
	trans->add = alpm_list_add(trans->add, spkg);

	return(0);
}

static int sync_target(alpm_list_t *dbs_sync, char *target)
{
	alpm_list_t *i, *j;
	alpm_list_t *known_pkgs = NULL;
	pmpkg_t *spkg;
	pmdepend_t *dep; /* provisions and dependencies are also allowed */
	pmgrp_t *grp;
	int found = 0;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(target != NULL && strlen(target) != 0, RET_ERR(PM_ERR_WRONG_ARGS, -1));
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	dep = _alpm_splitdep(target);
	spkg = _alpm_resolvedep(dep, dbs_sync, NULL, 1);
	_alpm_dep_free(dep);

	if(spkg != NULL) {
		return(sync_pkg(spkg, handle->trans->add));
	}

	_alpm_log(PM_LOG_DEBUG, "%s package not found, searching for group...\n", target);
	for(i = dbs_sync; i; i = i->next) {
		pmdb_t *db = i->data;
		grp = alpm_db_readgrp(db, target);
		if(grp) {
			found = 1;
			for(j = alpm_grp_get_pkgs(grp); j; j = j->next) {
				pmpkg_t *pkg = j->data;
				if(sync_pkg(pkg, known_pkgs) == -1) {
					if(pm_errno == PM_ERR_TRANS_DUP_TARGET || pm_errno == PM_ERR_PKG_IGNORED) {
						/* just skip duplicate or ignored targets */
						continue;
					} else {
						alpm_list_free(known_pkgs);
						return(-1);
					}
				}
				known_pkgs = alpm_list_add(known_pkgs, pkg);
			}
		}
	}
	alpm_list_free(known_pkgs);

	if(!found) {
		/* pass through any 'found but ignored' errors */
		if(pm_errno != PM_ERR_PKG_IGNORED) {
			pm_errno = PM_ERR_PKG_NOT_FOUND;
		}
		return(-1);
	}

	return(0);
}

/** Add a sync target to the transaction.
 * @param target the name of the sync target to add
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_sync_dbtarget(char *dbname, char *target)
{
	alpm_list_t *i;
	alpm_list_t *dbs_sync;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	dbs_sync = handle->dbs_sync;

	/* we are looking for a package in a specific database */
	alpm_list_t *dbs = NULL;
	_alpm_log(PM_LOG_DEBUG, "searching for target '%s' in repo '%s'\n", target, dbname);
	for(i = dbs_sync; i; i = i->next) {
		pmdb_t *db = i->data;
		if(strcmp(db->treename, dbname) == 0) {
			dbs = alpm_list_add(NULL, db);
			break;
		}
	}
	if(dbs == NULL) {
		RET_ERR(PM_ERR_PKG_REPO_NOT_FOUND, -1);
	}
	int ret = sync_target(dbs, target);
	alpm_list_free(dbs);
	return(ret);
}

/** Add a sync target to the transaction.
 * @param target the name of the sync target to add
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_sync_target(char *target)
{
	alpm_list_t *dbs_sync;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	dbs_sync = handle->dbs_sync;

	return(sync_target(dbs_sync,target));
}

/** Compute the size of the files that will be downloaded to install a
 * package.
 * @param newpkg the new package to upgrade to
 */
static int compute_download_size(pmpkg_t *newpkg)
{
	const char *fname;
	char *fpath;
	off_t size = 0;

	if(newpkg->origin == PKG_FROM_FILE) {
		newpkg->infolevel |= INFRQ_DSIZE;
		newpkg->download_size = 0;
		return(0);
	}

	fname = alpm_pkg_get_filename(newpkg);
	ASSERT(fname != NULL, RET_ERR(PM_ERR_PKG_INVALID_NAME, -1));
	fpath = _alpm_filecache_find(fname);

	if(fpath) {
		FREE(fpath);
		size = 0;
	} else if(handle->usedelta) {
		off_t dltsize;
		off_t pkgsize = alpm_pkg_get_size(newpkg);

		dltsize = _alpm_shortest_delta_path(
			alpm_pkg_get_deltas(newpkg),
			alpm_pkg_get_filename(newpkg),
			&newpkg->delta_path);

		if(newpkg->delta_path && (dltsize < pkgsize * MAX_DELTA_RATIO)) {
			_alpm_log(PM_LOG_DEBUG, "using delta size\n");
			size = dltsize;
		} else {
			_alpm_log(PM_LOG_DEBUG, "using package size\n");
			size = alpm_pkg_get_size(newpkg);
			alpm_list_free(newpkg->delta_path);
			newpkg->delta_path = NULL;
		}
	} else {
		size = alpm_pkg_get_size(newpkg);
	}

	_alpm_log(PM_LOG_DEBUG, "setting download size %jd for pkg %s\n",
			(intmax_t)size, alpm_pkg_get_name(newpkg));

	newpkg->infolevel |= INFRQ_DSIZE;
	newpkg->download_size = size;
	return(0);
}

int _alpm_sync_prepare(pmtrans_t *trans, pmdb_t *db_local, alpm_list_t *dbs_sync, alpm_list_t **data)
{
	alpm_list_t *deps = NULL;
	alpm_list_t *unresolvable = NULL;
	alpm_list_t *i, *j;
	alpm_list_t *remove = NULL;
	int ret = 0;

	ALPM_LOG_FUNC;

	ASSERT(db_local != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));

	if(data) {
		*data = NULL;
	}

	if(!(trans->flags & PM_TRANS_FLAG_NODEPS)) {
		alpm_list_t *resolved = NULL; /* target list after resolvedeps */

		/* Build up list by repeatedly resolving each transaction package */
		/* Resolve targets dependencies */
		EVENT(trans, PM_TRANS_EVT_RESOLVEDEPS_START, NULL, NULL);
		_alpm_log(PM_LOG_DEBUG, "resolving target's dependencies\n");

		/* build remove list for resolvedeps */
		for(i = trans->add; i; i = i->next) {
			pmpkg_t *spkg = i->data;
			for(j = spkg->removes; j; j = j->next) {
				remove = alpm_list_add(remove, j->data);
			}
		}

		/* Compute the fake local database for resolvedeps (partial fix for the phonon/qt issue) */
		alpm_list_t *localpkgs = alpm_list_diff(_alpm_db_get_pkgcache(db_local), trans->add, _alpm_pkg_cmp);

		/* Resolve packages in the transaction one at a time, in addtion
		   building up a list of packages which could not be resolved. */
		for(i = trans->add; i; i = i->next) {
			pmpkg_t *pkg = i->data;
			if(_alpm_resolvedeps(localpkgs, dbs_sync, pkg, trans->add,
						&resolved, remove, data) == -1) {
				unresolvable = alpm_list_add(unresolvable, pkg);
			}
			/* Else, [resolved] now additionally contains [pkg] and all of its
			   dependencies not already on the list */
		}
		alpm_list_free(localpkgs);

		/* If there were unresolvable top-level packages, prompt the user to
		   see if they'd like to ignore them rather than failing the sync */
		if(unresolvable != NULL) {
			int remove_unresolvable = 0;
			QUESTION(handle->trans, PM_TRANS_CONV_REMOVE_PKGS, unresolvable,
					NULL, NULL, &remove_unresolvable);
			if (remove_unresolvable) {
				/* User wants to remove the unresolvable packages from the
				   transaction. The packages will be removed from the actual
				   transaction when the transaction packages are replaced with a
				   dependency-reordered list below */
				pm_errno = 0; /* pm_errno was set by resolvedeps */
				if(data) {
					alpm_list_free_inner(*data, (alpm_list_fn_free)_alpm_depmiss_free);
					alpm_list_free(*data);
					*data = NULL;
				}
			} else {
				/* pm_errno is set by resolvedeps */
				alpm_list_free(resolved);
				ret = -1;
				goto cleanup;
			}
		}

		/* Set DEPEND reason for pulled packages */
		for(i = resolved; i; i = i->next) {
			pmpkg_t *pkg = i->data;
			if(!_alpm_pkg_find(trans->add, pkg->name)) {
				pkg->reason = PM_PKG_REASON_DEPEND;
			}
		}

		/* Unresolvable packages will be removed from the target list, so
		   we free the transaction specific fields */
		alpm_list_free_inner(unresolvable, (alpm_list_fn_free)_alpm_pkg_free_trans);

		/* re-order w.r.t. dependencies */
		alpm_list_free(trans->add);
		trans->add = _alpm_sortbydeps(resolved, 0);
		alpm_list_free(resolved);

		EVENT(trans, PM_TRANS_EVT_RESOLVEDEPS_DONE, NULL, NULL);
	}

	if(!(trans->flags & PM_TRANS_FLAG_NOCONFLICTS)) {
		/* check for inter-conflicts and whatnot */
		EVENT(trans, PM_TRANS_EVT_INTERCONFLICTS_START, NULL, NULL);

		_alpm_log(PM_LOG_DEBUG, "looking for conflicts\n");

		/* 1. check for conflicts in the target list */
		_alpm_log(PM_LOG_DEBUG, "check targets vs targets\n");
		deps = _alpm_innerconflicts(trans->add);

		for(i = deps; i; i = i->next) {
			pmconflict_t *conflict = i->data;
			pmpkg_t *rsync, *sync, *sync1, *sync2;

			/* have we already removed one of the conflicting targets? */
			sync1 = _alpm_pkg_find(trans->add, conflict->package1);
			sync2 = _alpm_pkg_find(trans->add, conflict->package2);
			if(!sync1 || !sync2) {
				continue;
			}

			_alpm_log(PM_LOG_DEBUG, "conflicting packages in the sync list: '%s' <-> '%s'\n",
					conflict->package1, conflict->package2);

			/* if sync1 provides sync2, we remove sync2 from the targets, and vice versa */
			pmdepend_t *dep1 = _alpm_splitdep(conflict->package1);
			pmdepend_t *dep2 = _alpm_splitdep(conflict->package2);
			if(alpm_depcmp(sync1, dep2)) {
				rsync = sync2;
				sync = sync1;
			} else if(alpm_depcmp(sync2, dep1)) {
				rsync = sync1;
				sync = sync2;
			} else {
				_alpm_log(PM_LOG_ERROR, _("unresolvable package conflicts detected\n"));
				pm_errno = PM_ERR_CONFLICTING_DEPS;
				ret = -1;
				if(data) {
					pmconflict_t *newconflict = _alpm_conflict_dup(conflict);
					if(newconflict) {
						*data = alpm_list_add(*data, newconflict);
					}
				}
				alpm_list_free_inner(deps, (alpm_list_fn_free)_alpm_conflict_free);
				alpm_list_free(deps);
				_alpm_dep_free(dep1);
				_alpm_dep_free(dep2);
				goto cleanup;
			}
			_alpm_dep_free(dep1);
			_alpm_dep_free(dep2);

			/* Prints warning */
			_alpm_log(PM_LOG_WARNING,
					_("removing '%s' from target list because it conflicts with '%s'\n"),
					rsync->name, sync->name);
			trans->add = alpm_list_remove(trans->add, rsync, _alpm_pkg_cmp, NULL);
			_alpm_pkg_free_trans(rsync); /* rsync is not transaction target anymore */
			continue;
		}

		alpm_list_free_inner(deps, (alpm_list_fn_free)_alpm_conflict_free);
		alpm_list_free(deps);
		deps = NULL;

		/* 2. we check for target vs db conflicts (and resolve)*/
		_alpm_log(PM_LOG_DEBUG, "check targets vs db and db vs targets\n");
		deps = _alpm_outerconflicts(db_local, trans->add);

		for(i = deps; i; i = i->next) {
			pmconflict_t *conflict = i->data;

			/* if conflict->package2 (the local package) is not elected for removal,
			   we ask the user */
			int found = 0;
			for(j = trans->add; j && !found; j = j->next) {
				pmpkg_t *spkg = j->data;
				if(_alpm_pkg_find(spkg->removes, conflict->package2)) {
					found = 1;
				}
			}
			if(found) {
				continue;
			}

			_alpm_log(PM_LOG_DEBUG, "package '%s' conflicts with '%s'\n",
					conflict->package1, conflict->package2);

			pmpkg_t *sync = _alpm_pkg_find(trans->add, conflict->package1);
			pmpkg_t *local = _alpm_db_get_pkgfromcache(db_local, conflict->package2);
			int doremove = 0;
			QUESTION(trans, PM_TRANS_CONV_CONFLICT_PKG, conflict->package1,
							conflict->package2, conflict->reason, &doremove);
			if(doremove) {
				/* append to the removes list */
				_alpm_log(PM_LOG_DEBUG, "electing '%s' for removal\n", conflict->package2);
				sync->removes = alpm_list_add(sync->removes, local);
			} else { /* abort */
				_alpm_log(PM_LOG_ERROR, _("unresolvable package conflicts detected\n"));
				pm_errno = PM_ERR_CONFLICTING_DEPS;
				ret = -1;
				if(data) {
					pmconflict_t *newconflict = _alpm_conflict_dup(conflict);
					if(newconflict) {
						*data = alpm_list_add(*data, newconflict);
					}
				}
				alpm_list_free_inner(deps, (alpm_list_fn_free)_alpm_conflict_free);
				alpm_list_free(deps);
				goto cleanup;
			}
		}
		EVENT(trans, PM_TRANS_EVT_INTERCONFLICTS_DONE, NULL, NULL);
		alpm_list_free_inner(deps, (alpm_list_fn_free)_alpm_conflict_free);
		alpm_list_free(deps);
	}

	/* Build trans->remove list */
	for(i = trans->add; i; i = i->next) {
		pmpkg_t *spkg = i->data;
		for(j = spkg->removes; j; j = j->next) {
			trans->remove = alpm_list_add(trans->remove, _alpm_pkg_dup(j->data));
		}
	}

	if(!(trans->flags & PM_TRANS_FLAG_NODEPS)) {
		_alpm_log(PM_LOG_DEBUG, "checking dependencies\n");
		deps = alpm_checkdeps(_alpm_db_get_pkgcache(db_local), 1, trans->remove, trans->add);
		if(deps) {
			pm_errno = PM_ERR_UNSATISFIED_DEPS;
			ret = -1;
			if(data) {
				*data = deps;
			} else {
				alpm_list_free_inner(deps, (alpm_list_fn_free)_alpm_depmiss_free);
				alpm_list_free(deps);
			}
			goto cleanup;
		}
	}
	for(i = trans->add; i; i = i->next) {
		/* update download size field */
		pmpkg_t *spkg = i->data;
		if(compute_download_size(spkg) != 0) {
			ret = -1;
			goto cleanup;
		}
	}

cleanup:
	alpm_list_free(unresolvable);
	alpm_list_free(remove);

	return(ret);
}

/** Returns the size of the files that will be downloaded to install a
 * package.
 * @param newpkg the new package to upgrade to
 * @return the size of the download
 */
off_t SYMEXPORT alpm_pkg_download_size(pmpkg_t *newpkg)
{
	if(!(newpkg->infolevel & INFRQ_DSIZE)) {
		compute_download_size(newpkg);
	}
	return(newpkg->download_size);
}

static int endswith(const char *filename, const char *extension)
{
	const char *s = filename + strlen(filename) - strlen(extension);
	return(strcmp(s, extension) == 0);
}

/** Applies delta files to create an upgraded package file.
 *
 * All intermediate files are deleted, leaving only the starting and
 * ending package files.
 *
 * @param trans the transaction
 *
 * @return 0 if all delta files were able to be applied, 1 otherwise.
 */
static int apply_deltas(pmtrans_t *trans)
{
	alpm_list_t *i;
	int ret = 0;
	const char *cachedir = _alpm_filecache_setup();

	for(i = trans->add; i; i = i->next) {
		pmpkg_t *spkg = i->data;
		alpm_list_t *delta_path = spkg->delta_path;
		alpm_list_t *dlts = NULL;

		if(!delta_path) {
			continue;
		}

		for(dlts = delta_path; dlts; dlts = dlts->next) {
			pmdelta_t *d = dlts->data;
			char *delta, *from, *to;
			char command[PATH_MAX];
			size_t len = 0;

			delta = _alpm_filecache_find(d->delta);
			/* the initial package might be in a different cachedir */
			if(dlts == delta_path) {
				from = _alpm_filecache_find(d->from);
			} else {
				/* len = cachedir len + from len + '/' + null */
				len = strlen(cachedir) + strlen(d->from) + 2;
				CALLOC(from, len, sizeof(char), RET_ERR(PM_ERR_MEMORY, 1));
				snprintf(from, len, "%s/%s", cachedir, d->from);
			}
			len = strlen(cachedir) + strlen(d->to) + 2;
			CALLOC(to, len, sizeof(char), RET_ERR(PM_ERR_MEMORY, 1));
			snprintf(to, len, "%s/%s", cachedir, d->to);

			/* build the patch command */
			if(endswith(to, ".gz")) {
				/* special handling for gzip : we disable timestamp with -n option */
				snprintf(command, PATH_MAX, "xdelta3 -d -q -R -c -s %s %s | gzip -n > %s", from, delta, to);
			} else {
				snprintf(command, PATH_MAX, "xdelta3 -d -q -s %s %s %s", from, delta, to);
			}

			_alpm_log(PM_LOG_DEBUG, "command: %s\n", command);

			EVENT(trans, PM_TRANS_EVT_DELTA_PATCH_START, d->to, d->delta);

			int retval = system(command);
			if(retval == 0) {
				EVENT(trans, PM_TRANS_EVT_DELTA_PATCH_DONE, NULL, NULL);

				/* delete the delta file */
				unlink(delta);

				/* Delete the 'from' package but only if it is an intermediate
				 * package. The starting 'from' package should be kept, just
				 * as if deltas were not used. */
				if(dlts != delta_path) {
					unlink(from);
				}
			}
			FREE(from);
			FREE(to);
			FREE(delta);

			if(retval != 0) {
				/* one delta failed for this package, cancel the remaining ones */
				EVENT(trans, PM_TRANS_EVT_DELTA_PATCH_FAILED, NULL, NULL);
				ret = 1;
				break;
			}
		}
	}

	return(ret);
}

/** Compares the md5sum of a file to the expected value.
 *
 * If the md5sum does not match, the user is asked whether the file
 * should be deleted.
 *
 * @param trans the transaction
 * @param filename the filename of the file to test
 * @param md5sum the expected md5sum of the file
 *
 * @return 0 if the md5sum matched, 1 if not, -1 in case of errors
 */
static int test_md5sum(pmtrans_t *trans, const char *filename,
		const char *md5sum)
{
	char *filepath;
	int ret;

	filepath = _alpm_filecache_find(filename);

	ret = _alpm_test_md5sum(filepath, md5sum);

	if(ret == 1) {
		int doremove = 0;
		QUESTION(trans, PM_TRANS_CONV_CORRUPTED_PKG, (char *)filename,
				NULL, NULL, &doremove);
		if(doremove) {
			unlink(filepath);
		}
	}

	FREE(filepath);

	return(ret);
}

int _alpm_sync_commit(pmtrans_t *trans, pmdb_t *db_local, alpm_list_t **data)
{
	alpm_list_t *i, *j, *files = NULL;
	alpm_list_t *deltas = NULL;
	int replaces = 0;
	int errors = 0;
	const char *cachedir = NULL;
	int ret = -1;

	ALPM_LOG_FUNC;

	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));

	cachedir = _alpm_filecache_setup();
	trans->state = STATE_DOWNLOADING;

	/* Total progress - figure out the total download size if required to
	 * pass to the callback. This function is called once, and it is up to the
	 * frontend to compute incremental progress. */
	if(handle->totaldlcb) {
		off_t total_size = (off_t)0;
		/* sum up the download size for each package and store total */
		for(i = trans->add; i; i = i->next) {
			pmpkg_t *spkg = i->data;
			total_size += spkg->download_size;
		}
		handle->totaldlcb(total_size);
	}

	/* group sync records by repository and download */
	for(i = handle->dbs_sync; i; i = i->next) {
		pmdb_t *current = i->data;

		for(j = trans->add; j; j = j->next) {
			pmpkg_t *spkg = j->data;

			if(spkg->origin != PKG_FROM_FILE && current == spkg->origin_data.db) {
				const char *fname = NULL;

				fname = alpm_pkg_get_filename(spkg);
				ASSERT(fname != NULL, RET_ERR(PM_ERR_PKG_INVALID_NAME, -1));
				alpm_list_t *delta_path = spkg->delta_path;
				if(delta_path) {
					/* using deltas */
					alpm_list_t *dlts = NULL;

					for(dlts = delta_path; dlts; dlts = dlts->next) {
						pmdelta_t *d = dlts->data;

						if(d->download_size != 0) {
							/* add the delta filename to the download list if needed */
							files = alpm_list_add(files, strdup(d->delta));
						}

						/* keep a list of all the delta files for md5sums */
						deltas = alpm_list_add(deltas, d);
					}

				} else {
					/* not using deltas */
					if(spkg->download_size != 0) {
						/* add the filename to the download list if needed */
						files = alpm_list_add(files, strdup(fname));
					}
				}

			}
		}

		if(files) {
			EVENT(trans, PM_TRANS_EVT_RETRIEVE_START, current->treename, NULL);
			errors = _alpm_download_files(files, current->servers, cachedir);

			if (errors) {
				_alpm_log(PM_LOG_WARNING, _("failed to retrieve some files from %s\n"),
						current->treename);
				if(pm_errno == 0) {
					pm_errno = PM_ERR_RETRIEVE;
				}
				goto error;
			}
			FREELIST(files);
		}
	}

	for(j = trans->add; j; j = j->next) {
		pmpkg_t *pkg = j->data;
		pkg->infolevel &= ~INFRQ_DSIZE;
		pkg->download_size = 0;
	}

	/* clear out value to let callback know we are done */
	if(handle->totaldlcb) {
		handle->totaldlcb(0);
	}

	/* if we have deltas to work with */
	if(handle->usedelta && deltas) {
		int ret = 0;
		errors = 0;
		/* Check integrity of deltas */
		EVENT(trans, PM_TRANS_EVT_DELTA_INTEGRITY_START, NULL, NULL);

		for(i = deltas; i; i = i->next) {
			pmdelta_t *d = alpm_list_getdata(i);
			const char *filename = alpm_delta_get_filename(d);
			const char *md5sum = alpm_delta_get_md5sum(d);

			if(test_md5sum(trans, filename, md5sum) != 0) {
				errors++;
				*data = alpm_list_add(*data, strdup(filename));
			}
		}
		if(errors) {
			pm_errno = PM_ERR_DLT_INVALID;
			goto error;
		}
		EVENT(trans, PM_TRANS_EVT_DELTA_INTEGRITY_DONE, NULL, NULL);

		/* Use the deltas to generate the packages */
		EVENT(trans, PM_TRANS_EVT_DELTA_PATCHES_START, NULL, NULL);
		ret = apply_deltas(trans);
		EVENT(trans, PM_TRANS_EVT_DELTA_PATCHES_DONE, NULL, NULL);

		if(ret) {
			pm_errno = PM_ERR_DLT_PATCHFAILED;
			goto error;
		}
	}

	/* Check integrity of packages */
	EVENT(trans, PM_TRANS_EVT_INTEGRITY_START, NULL, NULL);

	errors = 0;
	for(i = trans->add; i; i = i->next) {
		pmpkg_t *spkg = i->data;
		if(spkg->origin == PKG_FROM_FILE) {
			continue; /* pkg_load() has been already called, this package is valid */
		}

		const char *filename = alpm_pkg_get_filename(spkg);
		const char *md5sum = alpm_pkg_get_md5sum(spkg);

		if(test_md5sum(trans, filename, md5sum) != 0) {
			errors++;
			*data = alpm_list_add(*data, strdup(filename));
			continue;
		}
		/* load the package file and replace pkgcache entry with it in the target list */
		/* TODO: alpm_pkg_get_db() will not work on this target anymore */
		_alpm_log(PM_LOG_DEBUG, "replacing pkgcache entry with package file for target %s\n", spkg->name);
		char *filepath = _alpm_filecache_find(filename);
		pmpkg_t *pkgfile;
		if(alpm_pkg_load(filepath, 1, &pkgfile) != 0) {
			_alpm_pkg_free(pkgfile);
			errors++;
			*data = alpm_list_add(*data, strdup(filename));
			FREE(filepath);
			continue;
		}
		FREE(filepath);
		pkgfile->reason = spkg->reason; /* copy over install reason */
		i->data = pkgfile;
		_alpm_pkg_free_trans(spkg); /* spkg has been removed from the target list */
	}
	if(errors) {
		pm_errno = PM_ERR_PKG_INVALID;
		goto error;
	}
	EVENT(trans, PM_TRANS_EVT_INTEGRITY_DONE, NULL, NULL);
	if(trans->flags & PM_TRANS_FLAG_DOWNLOADONLY) {
		ret = 0;
		goto error;
	}

	trans->state = STATE_COMMITING;

	replaces = alpm_list_count(trans->remove);

	/* fileconflict check */
	if(!(trans->flags & PM_TRANS_FLAG_FORCE)) {
		EVENT(trans, PM_TRANS_EVT_FILECONFLICTS_START, NULL, NULL);

		_alpm_log(PM_LOG_DEBUG, "looking for file conflicts\n");
		alpm_list_t *conflict = _alpm_db_find_fileconflicts(db_local, trans,
								    trans->add, trans->remove);
		if(conflict) {
			pm_errno = PM_ERR_FILE_CONFLICTS;
			if(data) {
				*data = conflict;
			} else {
				alpm_list_free_inner(conflict, (alpm_list_fn_free)_alpm_fileconflict_free);
				alpm_list_free(conflict);
			}
			goto error;
		}

		EVENT(trans, PM_TRANS_EVT_FILECONFLICTS_DONE, NULL, NULL);
	}

	/* remove conflicting and to-be-replaced packages */
	if(replaces) {
		_alpm_log(PM_LOG_DEBUG, "removing conflicting and to-be-replaced packages\n");
		/* we want the frontend to be aware of commit details */
		if(_alpm_remove_packages(trans, handle->db_local) == -1) {
			_alpm_log(PM_LOG_ERROR, _("could not commit removal transaction\n"));
			goto error;
		}
	}

	/* install targets */
	_alpm_log(PM_LOG_DEBUG, "installing packages\n");
	if(_alpm_upgrade_packages(trans, handle->db_local) == -1) {
		_alpm_log(PM_LOG_ERROR, _("could not commit transaction\n"));
		goto error;
	}
	ret = 0;

error:
	FREELIST(files);
	alpm_list_free(deltas);
	return(ret);
}

/* vim: set ts=2 sw=2 noet: */
