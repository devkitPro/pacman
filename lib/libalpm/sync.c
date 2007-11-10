/*
 *  sync.c
 * 
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
 *  USA.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>

/* libalpm */
#include "sync.h"
#include "alpm_list.h"
#include "log.h"
#include "error.h"
#include "package.h"
#include "db.h"
#include "cache.h"
#include "deps.h"
#include "conflict.h"
#include "provide.h"
#include "trans.h"
#include "util.h"
#include "handle.h"
#include "alpm.h"
#include "server.h"
#include "delta.h"

pmsyncpkg_t *_alpm_sync_new(int type, pmpkg_t *spkg, void *data)
{
	pmsyncpkg_t *sync;

	ALPM_LOG_FUNC;

	CALLOC(sync, 1, sizeof(pmsyncpkg_t), RET_ERR(PM_ERR_MEMORY, NULL));

	sync->type = type;
	sync->pkg = spkg;
	sync->data = data;
	
	return(sync);
}

void _alpm_sync_free(pmsyncpkg_t *sync)
{
	ALPM_LOG_FUNC;

	if(sync == NULL) {
		return;
	}

	/* TODO wow this is ugly */
	if(sync->type == PM_SYNC_TYPE_REPLACE) {
		alpm_list_t *tmp;
		for(tmp = sync->data; tmp; tmp = alpm_list_next(tmp)) {
			_alpm_pkg_free(tmp->data);
			tmp->data = NULL;
		}
		sync->data = NULL;
	} else {
		_alpm_pkg_free(sync->data);
		sync->data = NULL;
	}
	FREE(sync);
}

/* Find recommended replacements for packages during a sync.
 * (refactored from _alpm_sync_prepare)
 */
static int find_replacements(pmtrans_t *trans, pmdb_t *db_local,
														 alpm_list_t *dbs_sync)
{
	alpm_list_t *i, *j, *k; /* wow */

	ALPM_LOG_FUNC;

	/* check for "recommended" package replacements */
	_alpm_log(PM_LOG_DEBUG, "checking for package replacements\n");
	for(i = dbs_sync; i; i = i->next) {
		pmdb_t *db = i->data;

		/* for each db, check each package's REPLACES list */
		for(j = _alpm_db_get_pkgcache(db); j; j = j->next) {
			pmpkg_t *spkg = j->data;

			for(k = alpm_pkg_get_replaces(spkg); k; k = k->next) {
				const char *replacement = k->data;
				
				pmpkg_t *lpkg = _alpm_db_get_pkgfromcache(db_local, replacement);
				if(!lpkg) {
					continue;
				}

				_alpm_log(PM_LOG_DEBUG, "checking replacement '%s' for package '%s'\n",
						replacement, spkg->name);
				/* ignore if EITHER the local or replacement package are to be ignored */
				if(_alpm_pkg_should_ignore(spkg) || _alpm_pkg_should_ignore(lpkg)) {
					_alpm_log(PM_LOG_WARNING, _("%s-%s: ignoring package upgrade (to be replaced by %s-%s)\n"),
										alpm_pkg_get_name(lpkg), alpm_pkg_get_version(lpkg),
										alpm_pkg_get_name(spkg), alpm_pkg_get_version(spkg));
				} else {
					/* get confirmation for the replacement */
					int doreplace = 0;
					QUESTION(trans, PM_TRANS_CONV_REPLACE_PKG, lpkg, spkg, db->treename, &doreplace);

					if(doreplace) {
						/* if confirmed, add this to the 'final' list, designating 'lpkg' as
						 * the package to replace.
						 */
						pmsyncpkg_t *sync;
						pmpkg_t *dummy = _alpm_pkg_new(alpm_pkg_get_name(lpkg), NULL);
						if(dummy == NULL) {
							pm_errno = PM_ERR_MEMORY;
							goto error;
						}
						dummy->requiredby = alpm_list_strdup(alpm_pkg_get_requiredby(lpkg));
						/* check if spkg->name is already in the packages list. */
						sync = _alpm_sync_find(trans->packages, alpm_pkg_get_name(spkg));
						if(sync) {
							/* found it -- just append to the replaces list */
							sync->data = alpm_list_add(sync->data, dummy);
						} else {
							/* none found -- enter pkg into the final sync list */
							sync = _alpm_sync_new(PM_SYNC_TYPE_REPLACE, spkg, NULL);
							if(sync == NULL) {
								_alpm_pkg_free(dummy);
								pm_errno = PM_ERR_MEMORY;
								goto error;
							}
							sync->data = alpm_list_add(NULL, dummy);
							trans->packages = alpm_list_add(trans->packages, sync);
						}
						_alpm_log(PM_LOG_DEBUG, "%s-%s elected for upgrade (to be replaced by %s-%s)\n",
											alpm_pkg_get_name(lpkg), alpm_pkg_get_version(lpkg),
											alpm_pkg_get_name(spkg), alpm_pkg_get_version(spkg));
					}
				}
			}
		}
	}
	return(0);
error:
	return(-1);
}

/* TODO reimplement this in terms of alpm_get_upgrades */
int _alpm_sync_sysupgrade(pmtrans_t *trans, pmdb_t *db_local, alpm_list_t *dbs_sync)
{
	alpm_list_t *i, *j;

	ALPM_LOG_FUNC;

	/* check for "recommended" package replacements */
	if(find_replacements(trans, db_local, dbs_sync) == 0) {
		/* match installed packages with the sync dbs and compare versions */
		_alpm_log(PM_LOG_DEBUG, "checking for package upgrades\n");
		for(i = _alpm_db_get_pkgcache(db_local); i; i = i->next) {
			int replace=0;
			pmpkg_t *local = i->data;
			pmpkg_t *spkg = NULL;
			pmsyncpkg_t *sync;

			for(j = dbs_sync; !spkg && j; j = j->next) {
				spkg = _alpm_db_get_pkgfromcache(j->data, alpm_pkg_get_name(local));
			}
			if(spkg == NULL) {
				_alpm_log(PM_LOG_DEBUG, "'%s' not found in sync db -- skipping\n",
						alpm_pkg_get_name(local));
				continue;
			}

			/* we don't care about a to-be-replaced package's newer version */
			for(j = trans->packages; j && !replace; j=j->next) {
				sync = j->data;
				if(sync->type == PM_SYNC_TYPE_REPLACE) {
					if(_alpm_pkg_find(alpm_pkg_get_name(spkg), sync->data)) {
						replace=1;
					}
				}
			}
			if(replace) {
				_alpm_log(PM_LOG_DEBUG, "'%s' is already elected for removal -- skipping\n",
									alpm_pkg_get_name(local));
				continue;
			}

			/* compare versions and see if we need to upgrade */
			if(alpm_pkg_compare_versions(local, spkg)) {
				_alpm_log(PM_LOG_DEBUG, "%s-%s elected for upgrade (%s => %s)\n",
									alpm_pkg_get_name(local), alpm_pkg_get_version(local),
									alpm_pkg_get_name(spkg), alpm_pkg_get_version(spkg));
				if(!_alpm_sync_find(trans->packages, alpm_pkg_get_name(spkg))) {
					/* If package is in the ignorepkg list, ask before we add it to
					 * the transaction */
					if(_alpm_pkg_should_ignore(local)) {
						int resp = 0;
						QUESTION(trans, PM_TRANS_CONV_INSTALL_IGNOREPKG, local, NULL, NULL, &resp);
						if(!resp) {
							continue;
						}
					}
					pmpkg_t *tmp = _alpm_pkg_dup(local);
					if(tmp == NULL) {
						goto error;
					}
					sync = _alpm_sync_new(PM_SYNC_TYPE_UPGRADE, spkg, tmp);
					if(sync == NULL) {
						_alpm_pkg_free(tmp);
						goto error;
					}
					trans->packages = alpm_list_add(trans->packages, sync);
				}
			}
		}

		return(0);
	}
error:
	/* if we're here, it's an error */
	return(-1);
}

int _alpm_sync_addtarget(pmtrans_t *trans, pmdb_t *db_local, alpm_list_t *dbs_sync, char *name)
{
	char targline[PKG_FULLNAME_LEN];
	char *targ;
	alpm_list_t *j;
	pmpkg_t *local;
	pmpkg_t *spkg = NULL;
	pmsyncpkg_t *sync;
	int repo_found = 0;

	ALPM_LOG_FUNC;

	ASSERT(db_local != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(name != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	strncpy(targline, name, PKG_FULLNAME_LEN);
	targ = strchr(targline, '/');
	if(targ) {
		/* we are looking for a package in a specific database */
		*targ = '\0';
		targ++;
		_alpm_log(PM_LOG_DEBUG, "searching for target '%s' in repo\n", targ);
		for(j = dbs_sync; j && !spkg; j = j->next) {
			pmdb_t *db = j->data;
			if(strcmp(db->treename, targline) == 0) {
				repo_found = 1;
				spkg = _alpm_db_get_pkgfromcache(db, targ);
				if(spkg == NULL) {
					/* Search provides */
					_alpm_log(PM_LOG_DEBUG, "target '%s' not found in db '%s' -- looking for provisions\n", targ, db->treename);
					alpm_list_t *p = _alpm_db_whatprovides(db, targ);
					if(!p) {
						RET_ERR(PM_ERR_PKG_NOT_FOUND, -1);
					}
					_alpm_log(PM_LOG_DEBUG, "found '%s' as a provision for '%s'\n",
							(char *)p->data, targ);
					spkg = _alpm_db_get_pkgfromcache(db, p->data);
					alpm_list_free(p);
				}
			}
		}
		if(!repo_found) {
			_alpm_log(PM_LOG_ERROR, _("repository '%s' not found\n"), targline);
			RET_ERR(PM_ERR_PKG_REPO_NOT_FOUND, -1);
		}
	} else {
		targ = targline;
		for(j = dbs_sync; j && !spkg; j = j->next) {
			pmdb_t *db = j->data;
			spkg = _alpm_db_get_pkgfromcache(db, targ);
		}
		if(spkg == NULL) {
			/* Search provides */
			_alpm_log(PM_LOG_DEBUG, "target '%s' not found -- looking for provisions\n", targ);
			for(j = dbs_sync; j && !spkg; j = j->next) {
				pmdb_t *db = j->data;
				alpm_list_t *p = _alpm_db_whatprovides(db, targ);
				if(p) {
					_alpm_log(PM_LOG_DEBUG, "found '%s' as a provision for '%s' in db '%s'\n",
							(char *)p->data, targ, db->treename);
					spkg = _alpm_db_get_pkgfromcache(db, p->data);
					alpm_list_free(p);
				}
			}
		}
	}

	if(spkg == NULL) {
		RET_ERR(PM_ERR_PKG_NOT_FOUND, -1);
	}

	local = _alpm_db_get_pkgfromcache(db_local, alpm_pkg_get_name(spkg));
	if(local) {
		if(alpm_pkg_compare_versions(local, spkg) == 0) {
			/* spkg is NOT an upgrade, get confirmation before adding */
			int resp = 0;
			if(_alpm_pkg_should_ignore(local)) {
				QUESTION(trans, PM_TRANS_CONV_INSTALL_IGNOREPKG, local, NULL, NULL, &resp);
				if(!resp) {
					return(0);
				}
			} else if(!(trans->flags & PM_TRANS_FLAG_PRINTURIS)) {
				QUESTION(trans, PM_TRANS_CONV_LOCAL_UPTODATE, local, NULL, NULL, &resp);
				if(!resp) {
					_alpm_log(PM_LOG_WARNING, _("%s-%s is up to date -- skipping\n"),
										alpm_pkg_get_name(local), alpm_pkg_get_version(local));
					return(0);
				}
			}
		}
	}

	/* add the package to the transaction */
	if(!_alpm_sync_find(trans->packages, alpm_pkg_get_name(spkg))) {
		pmpkg_t *dummy = NULL;
		if(local) {
			dummy = _alpm_pkg_new(alpm_pkg_get_name(local),
														alpm_pkg_get_version(local));
			if(dummy == NULL) {
				RET_ERR(PM_ERR_MEMORY, -1);
			}
		}
		sync = _alpm_sync_new(PM_SYNC_TYPE_UPGRADE, spkg, dummy);
		if(sync == NULL) {
			_alpm_pkg_free(dummy);
			RET_ERR(PM_ERR_MEMORY, -1);
		}
		_alpm_log(PM_LOG_DEBUG, "adding target '%s' to the transaction set\n",
							alpm_pkg_get_name(spkg));
		trans->packages = alpm_list_add(trans->packages, sync);
	}

	return(0);
}

/* Helper functions for alpm_list_remove
 */
static int syncpkg_cmp(const void *s1, const void *s2)
{
	const pmsyncpkg_t *sp1 = s1;
	const pmsyncpkg_t *sp2 = s2;
	pmpkg_t *p1, *p2;

  p1 = alpm_sync_get_pkg(sp1);
  p2 = alpm_sync_get_pkg(sp2);

	return(strcmp(alpm_pkg_get_name(p1), alpm_pkg_get_name(p2)));
}

int _alpm_sync_prepare(pmtrans_t *trans, pmdb_t *db_local, alpm_list_t *dbs_sync, alpm_list_t **data)
{
	alpm_list_t *deps = NULL;
	alpm_list_t *list = NULL; /* allow checkdeps usage with trans->packages */
	alpm_list_t *i, *j;
	int ret = 0;

	ALPM_LOG_FUNC;

	ASSERT(db_local != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));

	if(data) {
		*data = NULL;
	}

	if(!(trans->flags & PM_TRANS_FLAG_DEPENDSONLY)) {
		for(i = trans->packages; i; i = i->next) {
			pmsyncpkg_t *sync = i->data;
			list = alpm_list_add(list, sync->pkg);
		}
	}

	if(!(trans->flags & PM_TRANS_FLAG_NODEPS)) {
		/* Resolve targets dependencies */
		EVENT(trans, PM_TRANS_EVT_RESOLVEDEPS_START, NULL, NULL);
		_alpm_log(PM_LOG_DEBUG, "resolving target's dependencies\n");
		for(i = trans->packages; i; i = i->next) {
			pmpkg_t *spkg = ((pmsyncpkg_t *)i->data)->pkg;
			if(_alpm_resolvedeps(db_local, dbs_sync, spkg, &list,
						trans, data) == -1) {
				/* pm_errno is set by resolvedeps */
				ret = -1;
				goto cleanup;
			}
		}

		if((trans->flags & PM_TRANS_FLAG_DEPENDSONLY)) {
			FREELIST(trans->packages);
		}

		for(i = list; i; i = i->next) {
			/* add the dependencies found by resolvedeps to the transaction set */
			pmpkg_t *spkg = i->data;
			if(!_alpm_sync_find(trans->packages, alpm_pkg_get_name(spkg))) {
				pmsyncpkg_t *sync = _alpm_sync_new(PM_SYNC_TYPE_DEPEND, spkg, NULL);
				if(sync == NULL) {
					ret = -1;
					goto cleanup;
				}
				trans->packages = alpm_list_add(trans->packages, sync);
				_alpm_log(PM_LOG_DEBUG, "adding package %s-%s to the transaction targets\n",
									alpm_pkg_get_name(spkg), alpm_pkg_get_version(spkg));
			}
		}

		/* re-order w.r.t. dependencies */
		alpm_list_t *sortlist = _alpm_sortbydeps(list, PM_TRANS_TYPE_ADD);
		alpm_list_t *newpkgs = NULL;
		for(i = sortlist; i; i = i->next) {
			for(j = trans->packages; j; j = j->next) {
				pmsyncpkg_t *s = j->data;
				if(s->pkg == i->data) {
					newpkgs = alpm_list_add(newpkgs, s);
				}
			}
		}
		alpm_list_free(sortlist);
		alpm_list_free(trans->packages);
		trans->packages = newpkgs;

		EVENT(trans, PM_TRANS_EVT_RESOLVEDEPS_DONE, NULL, NULL);

		_alpm_log(PM_LOG_DEBUG, "looking for unresolvable dependencies\n");
		deps = _alpm_checkdeps(db_local, PM_TRANS_TYPE_UPGRADE, list);
		if(deps) {
			if(data) {
				*data = deps;
			} else {
				FREELIST(deps);
			}
			pm_errno = PM_ERR_UNSATISFIED_DEPS;
			ret = -1;
			goto cleanup;
		}
	}

	/* We don't care about conflicts if we're just printing uris */
	if(!(trans->flags & (PM_TRANS_FLAG_NOCONFLICTS | PM_TRANS_FLAG_PRINTURIS))) {
		/* check for inter-conflicts and whatnot */
		EVENT(trans, PM_TRANS_EVT_INTERCONFLICTS_START, NULL, NULL);

		_alpm_log(PM_LOG_DEBUG, "looking for conflicts\n");
		deps = _alpm_checkconflicts(db_local, list);
		if(deps) {
			int errorout = 0;
			alpm_list_t *asked = NULL;

			for(i = deps; i && !errorout; i = i->next) {
				pmdepmissing_t *miss = i->data;
				pmsyncpkg_t *sync;
				pmpkg_t *found = NULL;

				_alpm_log(PM_LOG_DEBUG, "package '%s' conflicts with '%s'\n",
				          miss->target, miss->depend.name);
				/* check if the conflicting package is about to be removed/replaced.
				 * if so, then just ignore it. */
				for(j = trans->packages; j && !found; j = j->next) {
					sync = j->data;
					if(sync->type == PM_SYNC_TYPE_REPLACE) {
						found = _alpm_pkg_find(miss->depend.name, sync->data);
					}
				}
				if(found) {
					_alpm_log(PM_LOG_DEBUG, "'%s' is already elected for removal -- skipping\n",
							alpm_pkg_get_name(found));
					continue;
				}

				sync = _alpm_sync_find(trans->packages, miss->target);
				if(sync == NULL) {
					_alpm_log(PM_LOG_DEBUG, "'%s' not found in transaction set -- skipping\n",
					          miss->target);
					continue;
				}
				pmpkg_t *local = _alpm_db_get_pkgfromcache(db_local, miss->depend.name);
				/* check if this package provides the package it's conflicting with */
				if(alpm_list_find_str(alpm_pkg_get_provides(sync->pkg),
							miss->depend.name)) {
					/* treat like a replaces item so requiredby fields are
					 * inherited properly. */
					_alpm_log(PM_LOG_DEBUG, "package '%s' provides its own conflict\n",
							miss->target);
					if(!local) {
						char *rmpkg = NULL;
						int target, depend;
						/* hmmm, depend.name isn't installed, so it must be conflicting
						 * with another package in our final list.  For example:
						 *
						 *     pacman -S blackbox xfree86
						 *
						 * If no x-servers are installed and blackbox pulls in xorg, then
						 * xorg and xfree86 will conflict with each other.  In this case,
						 * we should follow the user's preference and rip xorg out of final,
						 * opting for xfree86 instead.
						 */

						/* figure out which one was requested in targets. If they both
						 * were, then it's still an unresolvable conflict. */
						target = alpm_list_find_str(trans->targets, miss->target);
						depend = alpm_list_find_str(trans->targets, miss->depend.name);
						if(depend && !target) {
							_alpm_log(PM_LOG_DEBUG, "'%s' is in the target list -- keeping it\n",
								miss->depend.name);
							/* remove miss->target */
							rmpkg = miss->target;
						} else if(target && !depend) {
							_alpm_log(PM_LOG_DEBUG, "'%s' is in the target list -- keeping it\n",
								miss->target);
							/* remove miss->depend.name */
							rmpkg = miss->depend.name;
						} else {
							/* miss->depend.name is not needed, miss->target already provides
							 * it, let's resolve the conflict */
							rmpkg = miss->depend.name;
						}
						if(rmpkg) {
							pmsyncpkg_t *rsync = _alpm_sync_find(trans->packages, rmpkg);
							void *vpkg;
							_alpm_log(PM_LOG_DEBUG, "removing '%s' from target list\n",
									rsync->pkg->name);
							trans->packages = alpm_list_remove(trans->packages, rsync,
									syncpkg_cmp, &vpkg);
							_alpm_sync_free(vpkg);
							continue;
						}
					}
				}
				/* It's a conflict -- see if they want to remove it */
				_alpm_log(PM_LOG_DEBUG, "resolving package '%s' conflict\n",
						miss->target);
				if(local) {
					int doremove = 0;
					if(!alpm_list_find_str(asked, miss->depend.name)) {
						QUESTION(trans, PM_TRANS_CONV_CONFLICT_PKG, miss->target,
								miss->depend.name, NULL, &doremove);
						asked = alpm_list_add(asked, strdup(miss->depend.name));
						if(doremove) {
							pmpkg_t *q = _alpm_pkg_dup(local);
							q->requiredby = alpm_list_strdup(alpm_pkg_get_requiredby(local));
							if(sync->type != PM_SYNC_TYPE_REPLACE) {
								/* switch this sync type to REPLACE */
								sync->type = PM_SYNC_TYPE_REPLACE;
								_alpm_pkg_free(sync->data);
								sync->data = NULL;
							}
							/* append to the replaces list */
							_alpm_log(PM_LOG_DEBUG, "electing '%s' for removal\n",
									miss->depend.name);
							sync->data = alpm_list_add(sync->data, q);
							/* see if the package is in the current target list */
							pmsyncpkg_t *rsync = _alpm_sync_find(trans->packages,
									miss->depend.name);
							if(rsync) {
								/* remove it from the target list */
								void *vpkg;
								_alpm_log(PM_LOG_DEBUG, "removing '%s' from target list\n",
										miss->depend.name);
								trans->packages = alpm_list_remove(trans->packages, rsync,
										syncpkg_cmp, &vpkg);
								_alpm_sync_free(vpkg);
							}
						} else {
							/* abort */
							_alpm_log(PM_LOG_ERROR, _("unresolvable package conflicts detected\n"));
							errorout = 1;
							if(data) {
								if((miss = malloc(sizeof(pmdepmissing_t))) == NULL) {
									_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %zd bytes\n"), sizeof(pmdepmissing_t));
									FREELIST(*data);
									pm_errno = PM_ERR_MEMORY;
									ret = -1;
									goto cleanup;
								}
								*miss = *(pmdepmissing_t *)i->data;
								*data = alpm_list_add(*data, miss);
							}
						}
					}
				} else {
					_alpm_log(PM_LOG_ERROR, _("unresolvable package conflicts detected\n"));
					errorout = 1;
					if(data) {
						if((miss = malloc(sizeof(pmdepmissing_t))) == NULL) {
							_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %zd bytes\n"), sizeof(pmdepmissing_t));
							FREELIST(*data);
							pm_errno = PM_ERR_MEMORY;
							ret = -1;
							goto cleanup;
						}
						*miss = *(pmdepmissing_t *)i->data;
						*data = alpm_list_add(*data, miss);
					}
				}
			}
			if(errorout) {
				pm_errno = PM_ERR_CONFLICTING_DEPS;
				ret = -1;
				goto cleanup;
			}
			FREELIST(deps);
			FREELIST(asked);
		}
		EVENT(trans, PM_TRANS_EVT_INTERCONFLICTS_DONE, NULL, NULL);
	}

	alpm_list_free(list);
	list = NULL;

	/* XXX: this fails for cases where a requested package wants
	 *      a dependency that conflicts with an older version of
	 *      the package.  It will be removed from final, and the user
	 *      has to re-request it to get it installed properly.
	 *
	 *      Not gonna happen very often, but should be dealt with...
	 */

	if(!(trans->flags & PM_TRANS_FLAG_NODEPS)) {
		/* Check dependencies of packages in rmtargs and make sure
		 * we won't be breaking anything by removing them.
		 * If a broken dep is detected, make sure it's not from a
		 * package that's in our final (upgrade) list.
		 */
		/*EVENT(trans, PM_TRANS_EVT_CHECKDEPS_DONE, NULL, NULL);*/
		for(i = trans->packages; i; i = i->next) {
			pmsyncpkg_t *sync = i->data;
			if(sync->type == PM_SYNC_TYPE_REPLACE) {
				for(j = sync->data; j; j = j->next) {
					list = alpm_list_add(list, j->data);
				}
			}
		}
		if(list) {
			_alpm_log(PM_LOG_DEBUG, "checking dependencies of packages designated for removal\n");
			deps = _alpm_checkdeps(db_local, PM_TRANS_TYPE_REMOVE, list);
			if(deps) {
				/* Check if broken dependencies are fixed by packages we are installing */
				int errorout = 0;
				for(i = deps; i; i = i->next) {
					pmdepmissing_t *miss = i->data;

					alpm_list_t *l;
					int satisfied = 0;
					for(l = trans->packages; l && !satisfied; l = l->next) {
						pmsyncpkg_t *sp = l->data;
						pmpkg_t *sppkg = sp->pkg;
						if(alpm_depcmp(sppkg, &(miss->depend))) {
							char *missdepstring = alpm_dep_get_string(&(miss->depend));
							_alpm_log(PM_LOG_DEBUG, "sync: dependency '%s' satisfied by package '%s'\n",
									missdepstring, alpm_pkg_get_name(sppkg));
							free(missdepstring);
							satisfied = 1;
						}
					}
					if(!satisfied) {
						errorout++;
						*data = alpm_list_add(*data, miss);
					}
				}
				if(errorout) {
					pm_errno = PM_ERR_UNSATISFIED_DEPS;
					ret = -1;
					goto cleanup;
				}
				FREELIST(deps);
			}
		}
		/*EVENT(trans, PM_TRANS_EVT_CHECKDEPS_DONE, NULL, NULL);*/
	}

cleanup:
	alpm_list_free(list);

	return(ret);
}

/** Returns a list of deltas that should be downloaded instead of the
 * package.
 *
 * It first tests if a delta path exists between the currently installed
 * version (if any) and the version to upgrade to. If so, the delta path
 * is used if its size is below a set percentage (MAX_DELTA_RATIO) of
 * the package size, Otherwise, an empty list is returned.
 *
 * @param newpkg the new package to upgrade to
 * @param db_local the local database
 *
 * @return the list of pmdelta_t * objects. NULL (the empty list) is
 * returned if the package should be downloaded instead of deltas.
 */
static alpm_list_t *pkg_upgrade_delta_path(pmpkg_t *newpkg, pmdb_t *db_local)
{
	pmpkg_t *oldpkg = alpm_db_get_pkg(db_local, newpkg->name);
	alpm_list_t *ret = NULL;

	if(oldpkg) {
		const char *oldname = alpm_pkg_get_filename(oldpkg);
		char *oldpath = _alpm_filecache_find(oldname);

		if(oldpath) {
			alpm_list_t *deltas = _alpm_shortest_delta_path(
					alpm_pkg_get_deltas(newpkg),
					alpm_pkg_get_version(oldpkg),
					alpm_pkg_get_version(newpkg));

			if(deltas) {
				unsigned long dltsize = _alpm_delta_path_size(deltas);
				unsigned long pkgsize = alpm_pkg_get_size(newpkg);

				if(dltsize < pkgsize * MAX_DELTA_RATIO) {
					ret = deltas;
				} else {
					ret = NULL;
					alpm_list_free(deltas);
				}
			}

			FREE(oldpath);
		}
	}

	return(ret);
}

/** Returns the size of the files that will be downloaded to install a
 * package.
 *
 * @param newpkg the new package to upgrade to
 * @param db_local the local database
 *
 * @return the size of the download
 */
unsigned long SYMEXPORT alpm_pkg_download_size(pmpkg_t *newpkg, pmdb_t *db_local)
{
	char *fpath = _alpm_filecache_find(alpm_pkg_get_filename(newpkg));
	unsigned long size = 0;

	if(fpath) {
		size = 0;
	} else if(handle->usedelta) {
		alpm_list_t *deltas = pkg_upgrade_delta_path(newpkg, db_local);

		if(deltas) {
			size = _alpm_delta_path_size_uncached(deltas);
		} else {
			size = alpm_pkg_get_size(newpkg);
		}

		alpm_list_free(deltas);
	} else {
		size = alpm_pkg_get_size(newpkg);
	}

	FREE(fpath);

	return(size);
}

/** Applies delta files to create an upgraded package file.
 *
 * All intermediate files are deleted, leaving only the starting and
 * ending package files.
 *
 * @param trans the transaction
 * @param patches A list of alternating pmpkg_t * and pmdelta_t *
 * objects. The patch command will be built using the pmpkg_t, pmdelta_t
 * pair.
 *
 * @return 0 if all delta files were able to be applied, 1 otherwise.
 */
static int apply_deltas(pmtrans_t *trans, alpm_list_t *patches)
{
	/* keep track of the previous package in the loop to decide if a
	 * package file should be deleted */
	pmpkg_t *lastpkg = NULL;
	int lastpkg_failed = 0;
	int ret = 0;
	const char *cachedir = _alpm_filecache_setup();

	alpm_list_t *p = patches;
	while(p) {
		pmpkg_t *pkg;
		pmdelta_t *d;
		char command[PATH_MAX], fname[PATH_MAX];
		char pkgfilename[PKG_FILENAME_LEN];

		pkg = alpm_list_getdata(p);
		p = alpm_list_next(p);

		d = alpm_list_getdata(p);
		p = alpm_list_next(p);

		/* if patching fails, ignore the rest of that package's deltas */
		if(lastpkg_failed) {
			if(pkg == lastpkg) {
				continue;
			} else {
				lastpkg_failed = 0;
			}
		}

		/* an example of the patch command: (using /cache for cachedir)
		 * xdelta patch /cache/pacman_3.0.0-1_to_3.0.1-1-i686.delta \
		 *              /cache/pacman-3.0.0-1-i686.pkg.tar.gz       \
		 *              /cache/pacman-3.0.1-1-i686.pkg.tar.gz
		 */

		/* build the patch command */
		snprintf(command, PATH_MAX,
				"xdelta patch"         /* the command */
				" %s/%s"               /* the delta */
				" %s/%s-%s-%s" PKGEXT  /* the 'from' package */
				" %s/%s-%s-%s" PKGEXT, /* the 'to' package */
				cachedir, d->filename,
				cachedir, pkg->name, d->from, pkg->arch,
				cachedir, pkg->name, d->to, pkg->arch);

		_alpm_log(PM_LOG_DEBUG, _("command: %s\n"), command);

		snprintf(pkgfilename, PKG_FILENAME_LEN, "%s-%s-%s" PKGEXT,
				pkg->name, d->to, pkg->arch);

		EVENT(trans, PM_TRANS_EVT_DELTA_PATCH_START, pkgfilename, d->filename);

		if(system(command) == 0) {
			EVENT(trans, PM_TRANS_EVT_DELTA_PATCH_DONE, NULL, NULL);

			/* delete the delta file */
			snprintf(fname, PATH_MAX, "%s/%s", cachedir, d->filename);
			unlink(fname);

			/* Delete the 'from' package but only if it is an intermediate
			 * package. The starting 'from' package should be kept, just
			 * as if deltas were not used. Delete the package file if the
			 * previous iteration of the loop used the same package. */
			if(pkg == lastpkg) {
				snprintf(fname, PATH_MAX, "%s/%s-%s-%s" PKGEXT,
						cachedir, pkg->name, d->from, pkg->arch);
				unlink(fname);
			} else {
				lastpkg = pkg;
			}
		} else {
			EVENT(trans, PM_TRANS_EVT_DELTA_PATCH_FAILED, NULL, NULL);
			lastpkg_failed = 1;
			ret = 1;
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
 * @param data data to write the error messages to
 *
 * @return 0 if the md5sum matched, 1 otherwise
 */
static int test_md5sum(pmtrans_t *trans, const char *filename,
		const char *md5sum, alpm_list_t **data)
{
	char *filepath;
	char *md5sum2;
	char *errormsg = NULL;
	int ret = 0;

	filepath = _alpm_filecache_find(filename);
	md5sum2 = alpm_get_md5sum(filepath);

	if(md5sum == NULL) {
		/* TODO wtf is this? malloc'd strings for error messages? */
		if((errormsg = calloc(512, sizeof(char))) == NULL) {
			RET_ERR(PM_ERR_MEMORY, -1);
		}
		snprintf(errormsg, 512, _("can't get md5 checksum for file %s\n"),
				filename);
		*data = alpm_list_add(*data, errormsg);
		ret = 1;
	} else if(md5sum2 == NULL) {
		if((errormsg = calloc(512, sizeof(char))) == NULL) {
			RET_ERR(PM_ERR_MEMORY, -1);
		}
		snprintf(errormsg, 512, _("can't get md5 checksum for file %s\n"),
				filename);
		*data = alpm_list_add(*data, errormsg);
		ret = 1;
	} else if(strcmp(md5sum, md5sum2) != 0) {
		int doremove = 0;
		if((errormsg = calloc(512, sizeof(char))) == NULL) {
			RET_ERR(PM_ERR_MEMORY, -1);
		}
		QUESTION(trans, PM_TRANS_CONV_CORRUPTED_PKG, (char *)filename,
				NULL, NULL, &doremove);
		if(doremove) {
			unlink(filepath);
		}
		snprintf(errormsg, 512, _("file %s was corrupted (bad MD5 checksum)\n"),
				filename);
		*data = alpm_list_add(*data, errormsg);
		ret = 1;
	}

	FREE(filepath);
	FREE(md5sum2);

	return(ret);
}

/** Compares the md5sum of a delta to the expected value.
 *
 * @param trans the transaction
 * @param delta the delta to test
 * @param data data to write the error messages to
 *
 * @return 0 if the md5sum matched, 1 otherwise
 */
static int test_delta_md5sum(pmtrans_t *trans, pmdelta_t *delta,
		alpm_list_t **data)
{
	const char *filename;
	const char *md5sum;
	int ret = 0;

	filename = alpm_delta_get_filename(delta);
	md5sum = alpm_delta_get_md5sum(delta);

	ret = test_md5sum(trans, filename, md5sum, data);

	return(ret);
}

/** Compares the md5sum of a package to the expected value.
 *
 * @param trans the transaction
 * @param pkg the package to test
 * @param data data to write the error messages to
 *
 * @return 0 if the md5sum matched, 1 otherwise
 */
static int test_pkg_md5sum(pmtrans_t *trans, pmpkg_t *pkg, alpm_list_t **data)
{
	const char *filename;
	const char *md5sum;
	int ret = 0;

	filename = alpm_pkg_get_filename(pkg);
	md5sum = alpm_pkg_get_md5sum(pkg);

	ret = test_md5sum(trans, filename, md5sum, data);

	return(ret);
}

int _alpm_sync_commit(pmtrans_t *trans, pmdb_t *db_local, alpm_list_t **data)
{
	alpm_list_t *i, *j, *files = NULL;
	alpm_list_t *patches = NULL, *deltas = NULL;
	pmtrans_t *tr = NULL;
	int replaces = 0, retval = 0;
	const char *cachedir = NULL;
	int dltotal = 0, dl = 0;

	ALPM_LOG_FUNC;

	ASSERT(db_local != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));

	cachedir = _alpm_filecache_setup();
	trans->state = STATE_DOWNLOADING;

	/* Sum up the download sizes. This has to be in its own loop because
	 * the download loop is grouped by db. */
	for(j = trans->packages; j; j = j->next) {
		pmsyncpkg_t *sync = j->data;
		pmpkg_t *spkg = sync->pkg;
		dltotal += alpm_pkg_download_size(spkg, db_local);
	}

	/* group sync records by repository and download */
	for(i = handle->dbs_sync; i; i = i->next) {
		pmdb_t *current = i->data;

		for(j = trans->packages; j; j = j->next) {
			pmsyncpkg_t *sync = j->data;
			pmpkg_t *spkg = sync->pkg;
			pmdb_t *dbs = spkg->origin_data.db;

			if(current == dbs) {
				const char *fname = NULL;

				fname = alpm_pkg_get_filename(spkg);
				if(trans->flags & PM_TRANS_FLAG_PRINTURIS) {
					EVENT(trans, PM_TRANS_EVT_PRINTURI, (char *)alpm_db_get_url(current),
							(char *)fname);
				} else {
					char *fpath = _alpm_filecache_find(fname);
					if(!fpath) {
						if(handle->usedelta) {
							alpm_list_t *delta_path = pkg_upgrade_delta_path(spkg, db_local);

							if(delta_path) {
								alpm_list_t *dlts = NULL;

								for(dlts = delta_path; dlts; dlts = alpm_list_next(dlts)) {
									pmdelta_t *d = (pmdelta_t *)alpm_list_getdata(dlts);
									char *fpath2 = _alpm_filecache_find(d->filename);

									if(!fpath2) {
										/* add the delta filename to the download list if
										 * it's not in the cache*/
										files = alpm_list_add(files, strdup(d->filename));
									}

									/* save the package and delta so that the xdelta patch
									 * command can be run after the downloads finish */
									patches = alpm_list_add(patches, spkg);
									patches = alpm_list_add(patches, d);

									/* keep a list of the delta files for md5sums */
									deltas = alpm_list_add(deltas, d);
								}

								alpm_list_free(delta_path);
								delta_path = NULL;
							} else {
								/* no deltas to download, so add the file to the
								 * download list */
								files = alpm_list_add(files, strdup(fname));
							}
						} else {
							/* not using deltas, so add the file to the download list */
							files = alpm_list_add(files, strdup(fname));
						}
					}
					FREE(fpath);
				}
			}
		}

		if(files) {
			EVENT(trans, PM_TRANS_EVT_RETRIEVE_START, current->treename, NULL);
			if(_alpm_downloadfiles(current->servers, cachedir, files, &dl, dltotal)) {
				_alpm_log(PM_LOG_WARNING, _("failed to retrieve some files from %s\n"),
						current->treename);
				RET_ERR(PM_ERR_RETRIEVE, -1);
			}
			FREELIST(files);
		}
	}
	if(trans->flags & PM_TRANS_FLAG_PRINTURIS) {
		return(0);
	}

	if(handle->usedelta) {
		int ret = 0;

		/* only output if there are deltas to work with */
		if(deltas) {
			/* Check integrity of deltas */
			EVENT(trans, PM_TRANS_EVT_DELTA_INTEGRITY_START, NULL, NULL);

			for(i = deltas; i; i = i->next) {
				pmdelta_t *d = alpm_list_getdata(i);

				ret = test_delta_md5sum(trans, d, data);

				if(ret == 1) {
					retval = 1;
				} else if(ret == -1) { /* -1 is for serious errors */
					RET_ERR(pm_errno, -1);
				}
			}
			if(retval) {
				pm_errno = PM_ERR_DLT_CORRUPTED;
				goto error;
			}
			EVENT(trans, PM_TRANS_EVT_DELTA_INTEGRITY_DONE, NULL, NULL);

			/* Use the deltas to generate the packages */
			EVENT(trans, PM_TRANS_EVT_DELTA_PATCHES_START, NULL, NULL);
			ret = apply_deltas(trans, patches);
			EVENT(trans, PM_TRANS_EVT_DELTA_PATCHES_DONE, NULL, NULL);

			alpm_list_free(patches);
			patches = NULL;
			alpm_list_free(deltas);
			deltas = NULL;
		}
		if(ret) {
			pm_errno = PM_ERR_DLT_PATCHFAILED;
			goto error;
		}
	}

	/* Check integrity of packages */
	EVENT(trans, PM_TRANS_EVT_INTEGRITY_START, NULL, NULL);

	for(i = trans->packages; i; i = i->next) {
		pmsyncpkg_t *sync = i->data;
		pmpkg_t *spkg = sync->pkg;
		int ret = 0;

		ret = test_pkg_md5sum(trans, spkg, data);

		if(ret == 1) {
			retval = 1;
		} else if(ret == -1) { /* -1 is for serious errors */
			RET_ERR(pm_errno, -1);
		}
	}
	if(retval) {
		pm_errno = PM_ERR_PKG_CORRUPTED;
		goto error;
	}
	EVENT(trans, PM_TRANS_EVT_INTEGRITY_DONE, NULL, NULL);
	if(trans->flags & PM_TRANS_FLAG_DOWNLOADONLY) {
		return(0);
	}

	/* remove conflicting and to-be-replaced packages */
	trans->state = STATE_COMMITING;
	tr = _alpm_trans_new();
	if(tr == NULL) {
		_alpm_log(PM_LOG_ERROR, _("could not create removal transaction\n"));
		pm_errno = PM_ERR_MEMORY;
		goto error;
	}

	if(_alpm_trans_init(tr, PM_TRANS_TYPE_REMOVE, PM_TRANS_FLAG_NODEPS, NULL, NULL, NULL) == -1) {
		_alpm_log(PM_LOG_ERROR, _("could not initialize the removal transaction\n"));
		goto error;
	}

	for(i = trans->packages; i; i = i->next) {
		pmsyncpkg_t *sync = i->data;
		if(sync->type == PM_SYNC_TYPE_REPLACE) {
			alpm_list_t *j;
			for(j = sync->data; j; j = j->next) {
				pmpkg_t *pkg = j->data;
				if(!_alpm_pkg_find(pkg->name, tr->packages)) {
					if(_alpm_trans_addtarget(tr, pkg->name) == -1) {
						goto error;
					}
					replaces++;
				}
			}
		}
	}
	if(replaces) {
		_alpm_log(PM_LOG_DEBUG, "removing conflicting and to-be-replaced packages\n");
		if(_alpm_trans_prepare(tr, data) == -1) {
			_alpm_log(PM_LOG_ERROR, _("could not prepare removal transaction\n"));
			goto error;
		}
		/* we want the frontend to be aware of commit details */
		tr->cb_event = trans->cb_event;
		if(_alpm_trans_commit(tr, NULL) == -1) {
			_alpm_log(PM_LOG_ERROR, _("could not commit removal transaction\n"));
			goto error;
		}
	}
	_alpm_trans_free(tr);
	tr = NULL;

	/* install targets */
	_alpm_log(PM_LOG_DEBUG, "installing packages\n");
	tr = _alpm_trans_new();
	if(tr == NULL) {
		_alpm_log(PM_LOG_ERROR, _("could not create transaction\n"));
		pm_errno = PM_ERR_MEMORY;
		goto error;
	}
	if(_alpm_trans_init(tr, PM_TRANS_TYPE_UPGRADE, trans->flags | PM_TRANS_FLAG_NODEPS, trans->cb_event, trans->cb_conv, trans->cb_progress) == -1) {
		_alpm_log(PM_LOG_ERROR, _("could not initialize transaction\n"));
		goto error;
	}
	for(i = trans->packages; i; i = i->next) {
		pmsyncpkg_t *sync = i->data;
		pmpkg_t *spkg = sync->pkg;
		const char *fname;
		char *fpath;

		fname = alpm_pkg_get_filename(spkg);
		/* Loop through the cache dirs until we find a matching file */
		fpath = _alpm_filecache_find(fname);

		if(_alpm_trans_addtarget(tr, fpath) == -1) {
			FREE(fpath);
			goto error;
		}
		FREE(fpath);

		/* using alpm_list_last() is ok because addtarget() adds the new target at the
		 * end of the tr->packages list */
		spkg = alpm_list_last(tr->packages)->data;
		if(sync->type == PM_SYNC_TYPE_DEPEND) {
			spkg->reason = PM_PKG_REASON_DEPEND;
		}
	}
	if(_alpm_trans_prepare(tr, data) == -1) {
		_alpm_log(PM_LOG_ERROR, _("could not prepare transaction\n"));
		/* pm_errno is set by trans_prepare */
		goto error;
	}
	if(_alpm_trans_commit(tr, NULL) == -1) {
		_alpm_log(PM_LOG_ERROR, _("could not commit transaction\n"));
		goto error;
	}
	_alpm_trans_free(tr);
	tr = NULL;

	/* propagate replaced packages' requiredby fields to their new owners */
	if(replaces) {
		_alpm_log(PM_LOG_DEBUG, "updating database for replaced packages' dependencies\n");
		for(i = trans->packages; i; i = i->next) {
			pmsyncpkg_t *sync = i->data;
			if(sync->type == PM_SYNC_TYPE_REPLACE) {
				alpm_list_t *j;
				pmpkg_t *new = _alpm_db_get_pkgfromcache(db_local, alpm_pkg_get_name(sync->pkg));
				for(j = sync->data; j; j = j->next) {
					alpm_list_t *k;
					pmpkg_t *old = j->data;
					/* merge lists */
					for(k = alpm_pkg_get_requiredby(old); k; k = k->next) {
						if(!alpm_list_find_str(alpm_pkg_get_requiredby(new), k->data)) {
							/* replace old's name with new's name in the requiredby's dependency list */
							alpm_list_t *m;
							pmpkg_t *depender = _alpm_db_get_pkgfromcache(db_local, k->data);
							if(depender == NULL) {
								/* If the depending package no longer exists in the local db,
								 * then it must have ALSO conflicted with sync->pkg.  If
								 * that's the case, then we don't have anything to propagate
								 * here. */
								continue;
							}
							for(m = alpm_pkg_get_depends(depender); m; m = m->next) {
								if(!strcmp(m->data, alpm_pkg_get_name(old))) {
									FREE(m->data);
									m->data = strdup(alpm_pkg_get_name(new));
								}
							}
							if(_alpm_db_write(db_local, depender, INFRQ_DEPENDS) == -1) {
								_alpm_log(PM_LOG_ERROR, _("could not update requiredby for database entry %s-%s\n"),
													alpm_pkg_get_name(new), alpm_pkg_get_version(new));
							}
							/* add the new requiredby */
							new->requiredby = alpm_list_add(alpm_pkg_get_requiredby(new), strdup(k->data));
						}
					}
				}
				if(_alpm_db_write(db_local, new, INFRQ_DEPENDS) == -1) {
					_alpm_log(PM_LOG_ERROR, _("could not update new database entry %s-%s\n"),
										alpm_pkg_get_name(new), alpm_pkg_get_version(new));
				}
			}
		}
	}

	return(0);

error:
	_alpm_trans_free(tr);
	tr = NULL;
	return(-1);
}

pmsyncpkg_t *_alpm_sync_find(alpm_list_t *syncpkgs, const char* pkgname)
{
	alpm_list_t *i;
	for(i = syncpkgs; i; i = i->next) {
		pmsyncpkg_t *syncpkg = i->data;
		if(!syncpkg) {
			continue;
		}

		pmpkg_t *pkg = alpm_sync_get_pkg(syncpkg);
		if(strcmp(alpm_pkg_get_name(pkg), pkgname) == 0) {
			_alpm_log(PM_LOG_DEBUG, "found package '%s-%s' in sync\n",
								alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
			return(syncpkg);
		}
	}

	_alpm_log(PM_LOG_DEBUG, "package '%s' not found in sync\n", pkgname);
	return(NULL); /* not found */
}

pmsynctype_t SYMEXPORT alpm_sync_get_type(const pmsyncpkg_t *sync)
{
	/* Sanity checks */
	ASSERT(sync != NULL, return(-1));

	return sync->type;
}

pmpkg_t SYMEXPORT *alpm_sync_get_pkg(const pmsyncpkg_t *sync)
{
	/* Sanity checks */
	ASSERT(sync != NULL, return(NULL));

	return sync->pkg;
}

void SYMEXPORT *alpm_sync_get_data(const pmsyncpkg_t *sync)
{
	/* Sanity checks */
	ASSERT(sync != NULL, return(NULL));

	return sync->data;
}

/* vim: set ts=2 sw=2 noet: */
