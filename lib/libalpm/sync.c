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
#include <time.h>
#ifdef CYGWIN
#include <limits.h> /* PATH_MAX */
#endif
#include <dirent.h>
#include <libintl.h>
/* pacman */
#include "log.h"
#include "error.h"
#include "alpm_list.h"
#include "package.h"
#include "db.h"
#include "cache.h"
#include "deps.h"
#include "conflict.h"
#include "provide.h"
#include "trans.h"
#include "util.h"
#include "sync.h"
#include "versioncmp.h"
#include "handle.h"
#include "util.h"
#include "alpm.h"
#include "md5.h"
#include "sha1.h"
#include "handle.h"
#include "server.h"

pmsyncpkg_t *_alpm_sync_new(int type, pmpkg_t *spkg, void *data)
{
	pmsyncpkg_t *sync;

	ALPM_LOG_FUNC;

	if((sync = (pmsyncpkg_t *)malloc(sizeof(pmsyncpkg_t))) == NULL) {
		_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(pmsyncpkg_t));
		return(NULL);
	}

	sync->type = type;
	sync->pkg = spkg;
	sync->data = data;
	
	return(sync);
}

void _alpm_sync_free(void *data)
{
	pmsyncpkg_t *sync = data;

	ALPM_LOG_FUNC;

	if(sync == NULL) {
		return;
	}

	if(sync->type == PM_SYNC_TYPE_REPLACE) {
		FREELISTPKGS(sync->data);
	} else {
		FREEPKG(sync->data);
	}
	FREE(sync);
}

/* Test for existence of a package in a alpm_list_t* of pmsyncpkg_t*
 * If found, return a pointer to the respective pmsyncpkg_t*
 */
static pmsyncpkg_t *find_pkginsync(char *needle, alpm_list_t *haystack)
{
	alpm_list_t *i;
	pmsyncpkg_t *sync = NULL;
	int found = 0;

	ALPM_LOG_FUNC;

	for(i = haystack; i && !found; i = i->next) {
		sync = i->data;
		if(sync && !strcmp(sync->pkg->name, needle)) {
			found = 1;
		}
	}
	if(!found) {
		sync = NULL;
	}

	return(sync);
}

static int istoonew(pmpkg_t *pkg)
{
	time_t t;

	ALPM_LOG_FUNC;

	if (!handle->upgradedelay)
		return 0;
	time(&t);
	return((pkg->date + handle->upgradedelay) > t);
}

/* Find recommended replacements for packages during a sync.
 * (refactored from _alpm_sync_prepare)
 */
static int find_replacements(pmtrans_t *trans, pmdb_t *db_local,
														 alpm_list_t *dbs_sync)
{
	alpm_list_t *i, *j, *k;

	ALPM_LOG_FUNC;

	/* check for "recommended" package replacements */
	_alpm_log(PM_LOG_DEBUG, _("checking for package replacements"));
	for(i = dbs_sync; i; i = i->next) {
		for(j = _alpm_db_get_pkgcache(i->data, INFRQ_DESC); j; j = j->next) {
			pmpkg_t *spkg = j->data;
			for(k = spkg->replaces; k; k = k->next) {
				alpm_list_t *m;
				for(m = _alpm_db_get_pkgcache(db_local, INFRQ_NONE); m; m = m->next) {
					pmpkg_t *lpkg = m->data;
					if(strcmp(k->data, lpkg->name) == 0) {
						_alpm_log(PM_LOG_DEBUG, _("checking replacement '%s' for package '%s'"), k->data, spkg->name);
						if(alpm_list_find_str(handle->ignorepkg, lpkg->name)) {
							_alpm_log(PM_LOG_WARNING, _("%s-%s: ignoring package upgrade (to be replaced by %s-%s)"),
								lpkg->name, lpkg->version, spkg->name, spkg->version);
						} else {
							/* get confirmation for the replacement */
							int doreplace = 0;
							QUESTION(trans, PM_TRANS_CONV_REPLACE_PKG, lpkg, spkg, ((pmdb_t *)i->data)->treename, &doreplace);

							if(doreplace) {
								/* if confirmed, add this to the 'final' list, designating 'lpkg' as
								 * the package to replace.
								 */
								pmsyncpkg_t *sync;
								pmpkg_t *dummy = _alpm_pkg_new(lpkg->name, NULL);
								if(dummy == NULL) {
									pm_errno = PM_ERR_MEMORY;
									goto error;
								}
								dummy->requiredby = alpm_list_strdup(lpkg->requiredby);
								/* check if spkg->name is already in the packages list. */
								sync = find_pkginsync(spkg->name, trans->packages);
								if(sync) {
									/* found it -- just append to the replaces list */
									sync->data = alpm_list_add(sync->data, dummy);
								} else {
									/* none found -- enter pkg into the final sync list */
									sync = _alpm_sync_new(PM_SYNC_TYPE_REPLACE, spkg, NULL);
									if(sync == NULL) {
										FREEPKG(dummy);
										pm_errno = PM_ERR_MEMORY;
										goto error;
									}
									sync->data = alpm_list_add(NULL, dummy);
									trans->packages = alpm_list_add(trans->packages, sync);
								}
								_alpm_log(PM_LOG_DEBUG, _("%s-%s elected for upgrade (to be replaced by %s-%s)"),
								          lpkg->name, lpkg->version, spkg->name, spkg->version);
							}
						}
						break;
					}
				}
			}
		}
	}
	return(0);
error:
	return(-1);
}

int _alpm_sync_sysupgrade(pmtrans_t *trans, pmdb_t *db_local, alpm_list_t *dbs_sync)
{
	alpm_list_t *i, *j;

	ALPM_LOG_FUNC;

	/* check for "recommended" package replacements */
	_alpm_log(PM_LOG_DEBUG, _("checking for package replacements"));
	if( find_replacements(trans, db_local, dbs_sync) == 0 ) {
		/* match installed packages with the sync dbs and compare versions */
		_alpm_log(PM_LOG_DEBUG, _("checking for package upgrades"));
		for(i = _alpm_db_get_pkgcache(db_local, INFRQ_NONE); i; i = i->next) {
			int cmp;
			int replace=0;
			pmpkg_t *local = i->data;
			pmpkg_t *spkg = NULL;
			pmsyncpkg_t *sync;

			for(j = dbs_sync; !spkg && j; j = j->next) {
				spkg = _alpm_db_get_pkgfromcache(j->data, local->name);
			}
			if(spkg == NULL) {
				_alpm_log(PM_LOG_DEBUG, _("'%s' not found in sync db -- skipping"), local->name);
				continue;
			}

			/* we don't care about a to-be-replaced package's newer version */
			for(j = trans->packages; j && !replace; j=j->next) {
				sync = j->data;
				if(sync->type == PM_SYNC_TYPE_REPLACE) {
					if(_alpm_pkg_isin(spkg->name, sync->data)) {
						replace=1;
					}
				}
			}
			if(replace) {
				_alpm_log(PM_LOG_DEBUG, _("'%s' is already elected for removal -- skipping"),
									local->name);
				continue;
			}

			/* compare versions and see if we need to upgrade */
			cmp = alpm_versioncmp(local->version, spkg->version);
			if(cmp > 0 && !spkg->force) {
				/* local version is newer */
				pmdb_t *db = spkg->data;
				_alpm_log(PM_LOG_WARNING, _("%s: local (%s) is newer than %s (%s)"),
									local->name, local->version, db->treename, spkg->version);
			} else if(cmp == 0) {
				/* versions are identical */
			} else if(alpm_list_find_str(handle->ignorepkg, spkg->name)) {
				/* package should be ignored (IgnorePkg) */
				_alpm_log(PM_LOG_WARNING, _("%s-%s: ignoring package upgrade (%s)"),
									local->name, local->version, spkg->version);
			} else if(istoonew(spkg)) {
				/* package too new (UpgradeDelay) */
				_alpm_log(PM_LOG_DEBUG, _("%s-%s: delaying upgrade of package (%s)"),
									local->name, local->version, spkg->version);
				/* check if spkg->name is already in the packages list. */
			} else {
				_alpm_log(PM_LOG_DEBUG, _("%s-%s elected for upgrade (%s => %s)"),
									local->name, local->version, local->version, spkg->version);
				if(!find_pkginsync(spkg->name, trans->packages)) {
					pmpkg_t *dummy = _alpm_pkg_new(local->name, local->version);
					if(dummy == NULL) {
						goto error;
					}
					sync = _alpm_sync_new(PM_SYNC_TYPE_UPGRADE, spkg, dummy);
					if(sync == NULL) {
						FREEPKG(dummy);
						goto error;
					}
					trans->packages = alpm_list_add(trans->packages, sync);
				} else {
					/* spkg->name is already in the packages list -- just ignore it */
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
	int cmp, repo_found = 0;

	ALPM_LOG_FUNC;

	ASSERT(db_local != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(name != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	STRNCPY(targline, name, PKG_FULLNAME_LEN);
	targ = strchr(targline, '/');
	if(targ) {
		*targ = '\0';
		targ++;
		_alpm_log(PM_LOG_DEBUG, _("searching for target in repo '%s'"), targline);
		for(j = dbs_sync; j && !spkg; j = j->next) {
			pmdb_t *dbs = j->data;
			if(strcmp(dbs->treename, targline) == 0) {
				repo_found = 1;
				spkg = _alpm_db_get_pkgfromcache(dbs, targ);
				if(spkg == NULL) {
					/* Search provides */
					alpm_list_t *p;
					_alpm_log(PM_LOG_DEBUG, _("target '%s' not found -- looking for provisions"), targ);
					p = _alpm_db_whatprovides(dbs, targ);
					if(p == NULL) {
						RET_ERR(PM_ERR_PKG_NOT_FOUND, -1);
					}
					_alpm_log(PM_LOG_DEBUG, _("found '%s' as a provision for '%s'"), p->data, targ);
					spkg = _alpm_db_get_pkgfromcache(dbs, p->data);
					FREELISTPTR(p);
				}
			}
		}
		if(!repo_found) {
			_alpm_log(PM_LOG_ERROR, _("repository '%s' not found"), targline);
			RET_ERR(PM_ERR_PKG_REPO_NOT_FOUND, -1);
		}
	} else {
		targ = targline;
		for(j = dbs_sync; j && !spkg; j = j->next) {
			pmdb_t *dbs = j->data;
			spkg = _alpm_db_get_pkgfromcache(dbs, targ);
		}
		if(spkg == NULL) {
			/* Search provides */
			_alpm_log(PM_LOG_DEBUG, _("target '%s' not found -- looking for provisions"), targ);
			for(j = dbs_sync; j && !spkg; j = j->next) {
				pmdb_t *dbs = j->data;
				alpm_list_t *p = _alpm_db_whatprovides(dbs, targ);
				if(p) {
					_alpm_log(PM_LOG_DEBUG, _("found '%s' as a provision for '%s'"), p->data, targ);
					spkg = _alpm_db_get_pkgfromcache(dbs, p->data);
					FREELISTPTR(p);
				}
			}
		}
	}
	if(spkg == NULL) {
		RET_ERR(PM_ERR_PKG_NOT_FOUND, -1);
	}

	local = _alpm_db_get_pkgfromcache(db_local, spkg->name);
	if(local) {
		cmp = alpm_versioncmp(local->version, spkg->version);
		if(cmp > 0 && !spkg->force) {
			/* local version is newer -- get confirmation before adding */
			int resp = 0;
			QUESTION(trans, PM_TRANS_CONV_LOCAL_NEWER, local, NULL, NULL, &resp);
			if(!resp) {
				_alpm_log(PM_LOG_WARNING, _("%s-%s: local version is newer -- skipping"), local->name, local->version);
				return(0);
			}
		} else if(cmp == 0) {
			/* versions are identical -- get confirmation before adding */
			int resp = 0;
			QUESTION(trans, PM_TRANS_CONV_LOCAL_UPTODATE, local, NULL, NULL, &resp);
			if(!resp) {
				_alpm_log(PM_LOG_WARNING, _("%s-%s is up to date -- skipping"), local->name, local->version);
				return(0);
			}
		}
	}

	/* add the package to the transaction */
	if(!find_pkginsync(spkg->name, trans->packages)) {
		pmpkg_t *dummy = NULL;
		if(local) {
			dummy = _alpm_pkg_new(local->name, local->version);
			if(dummy == NULL) {
				RET_ERR(PM_ERR_MEMORY, -1);
			}
		}
		sync = _alpm_sync_new(PM_SYNC_TYPE_UPGRADE, spkg, dummy);
		if(sync == NULL) {
			FREEPKG(dummy);
			RET_ERR(PM_ERR_MEMORY, -1);
		}
		_alpm_log(PM_LOG_DEBUG, _("adding target '%s' to the transaction set"), spkg->name);
		trans->packages = alpm_list_add(trans->packages, sync);
	}

	return(0);
}

/* Helper functions for alpm_list_remove
 */
/* removed - use pkg_cmp all of the time
static int ptr_cmp(const void *s1, const void *s2)
{
	return(strcmp(((pmsyncpkg_t *)s1)->pkg->name, ((pmsyncpkg_t *)s2)->pkg->name));
}*/

static int pkg_cmp(const void *p1, const void *p2)
{
	return(strcmp(((pmpkg_t *)p1)->name, ((pmsyncpkg_t *)p2)->pkg->name));
}

int _alpm_sync_prepare(pmtrans_t *trans, pmdb_t *db_local, alpm_list_t *dbs_sync, alpm_list_t **data)
{
	alpm_list_t *deps = NULL;
	alpm_list_t *list = NULL; /* list allowing checkdeps usage with data from trans->packages */
	alpm_list_t *trail = NULL; /* breadcrumb list to avoid running into circles */
	alpm_list_t *asked = NULL; 
	alpm_list_t *i, *j, *k, *l;
	int ret = 0;

	ALPM_LOG_FUNC;

	ASSERT(db_local != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));

	if(data) {
		*data = NULL;
	}

	for(i = trans->packages; i; i = i->next) {
		pmsyncpkg_t *sync = i->data;
		list = alpm_list_add(list, sync->pkg);
	}

	if(!(trans->flags & PM_TRANS_FLAG_NODEPS)) {
		trail = alpm_list_new();

		/* Resolve targets dependencies */
		EVENT(trans, PM_TRANS_EVT_RESOLVEDEPS_START, NULL, NULL);
		_alpm_log(PM_LOG_DEBUG, _("resolving target's dependencies"));
		for(i = trans->packages; i; i = i->next) {
			pmpkg_t *spkg = ((pmsyncpkg_t *)i->data)->pkg;
			if(_alpm_resolvedeps(db_local, dbs_sync, spkg, list, trail, trans, data) == -1) {
				/* pm_errno is set by resolvedeps */
				ret = -1;
				goto cleanup;
			}
		}

		for(i = list; i; i = i->next) {
			/* add the dependencies found by resolvedeps to the transaction set */
			pmpkg_t *spkg = i->data;
			if(!find_pkginsync(spkg->name, trans->packages)) {
				pmsyncpkg_t *sync = _alpm_sync_new(PM_SYNC_TYPE_DEPEND, spkg, NULL);
				if(sync == NULL) {
					ret = -1;
					goto cleanup;
				}
				trans->packages = alpm_list_add(trans->packages, sync);
				_alpm_log(PM_LOG_DEBUG, _("adding package %s-%s to the transaction targets"),
						spkg->name, spkg->version);
			} else {
				/* remove the original targets from the list if requested */
				if((trans->flags & PM_TRANS_FLAG_DEPENDSONLY)) {
					void *vp;
					pmpkg_t *p;
					trans->packages = alpm_list_remove(trans->packages, spkg, pkg_cmp, &vp);
					p = vp;
					FREEPKG(p);
				}
			}
		}

		/* re-order w.r.t. dependencies */
		k = l = NULL;
		for(i=trans->packages; i; i=i->next) {
			pmsyncpkg_t *s = (pmsyncpkg_t*)i->data;
			k = alpm_list_add(k, s->pkg);
		}
		k = _alpm_sortbydeps(k, PM_TRANS_TYPE_ADD);
		for(i=k; i; i=i->next) {
			for(j=trans->packages; j; j=j->next) {
				pmsyncpkg_t *s = (pmsyncpkg_t*)j->data;
				if(s->pkg==i->data) {
					l = alpm_list_add(l, s);
				}
			}
		}
		FREELISTPTR(k);
		FREELISTPTR(trans->packages);
		trans->packages = l;

		EVENT(trans, PM_TRANS_EVT_RESOLVEDEPS_DONE, NULL, NULL);

		_alpm_log(PM_LOG_DEBUG, _("looking for unresolvable dependencies"));
		deps = _alpm_checkdeps(trans, db_local, PM_TRANS_TYPE_UPGRADE, list);
		if(deps) {
			if(data) {
				*data = deps;
				deps = NULL;
			}
			pm_errno = PM_ERR_UNSATISFIED_DEPS;
			ret = -1;
			goto cleanup;
		}

		FREELISTPTR(trail);
	}

	/* We don't care about conflicts if we're just printing uris */
	if(!(trans->flags & (PM_TRANS_FLAG_NOCONFLICTS | PM_TRANS_FLAG_PRINTURIS))) {
		/* check for inter-conflicts and whatnot */
		EVENT(trans, PM_TRANS_EVT_INTERCONFLICTS_START, NULL, NULL);

		_alpm_log(PM_LOG_DEBUG, _("looking for conflicts"));
		deps = _alpm_checkconflicts(db_local, list);
		if(deps) {
			int errorout = 0;

			for(i = deps; i && !errorout; i = i->next) {
				pmdepmissing_t *miss = i->data;
				int found = 0;
				pmsyncpkg_t *sync;
				pmpkg_t *local;

				_alpm_log(PM_LOG_DEBUG, _("package '%s' is conflicting with '%s'"),
				          miss->target, miss->depend.name);

				/* check if the conflicting package is one that's about to be removed/replaced.
				 * if so, then just ignore it
				 */
				for(j = trans->packages; j && !found; j = j->next) {
					sync = j->data;
					if(sync->type == PM_SYNC_TYPE_REPLACE) {
						if(_alpm_pkg_isin(miss->depend.name, sync->data)) {
							found = 1;
						}
					}
				}
				if(found) {
					_alpm_log(PM_LOG_DEBUG, _("'%s' is already elected for removal -- skipping"),
							miss->depend.name);
					continue;
				}

				sync = find_pkginsync(miss->target, trans->packages);
				if(sync == NULL) {
					_alpm_log(PM_LOG_DEBUG, _("'%s' not found in transaction set -- skipping"),
					          miss->target);
					continue;
				}
				local = _alpm_db_get_pkgfromcache(db_local, miss->depend.name);
				/* check if this package also "provides" the package it's conflicting with
				 */
				if(alpm_list_find_str(sync->pkg->provides, miss->depend.name)) {
					/* so just treat it like a "replaces" item so the REQUIREDBY
					 * fields are inherited properly.
					 */
					_alpm_log(PM_LOG_DEBUG, _("package '%s' provides its own conflict"), miss->target);
					if(local) {
						/* nothing to do for now: it will be handled later
						 * (not the same behavior as in pacman 2.x) */
					} else {
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

						/* figure out which one was requested in targets.  If they both were,
						 * then it's still an unresolvable conflict. */
						target = alpm_list_find_str(trans->targets, miss->target);
						depend = alpm_list_find_str(trans->targets, miss->depend.name);
						if(depend && !target) {
							_alpm_log(PM_LOG_DEBUG, _("'%s' is in the target list -- keeping it"),
								miss->depend.name);
							/* remove miss->target */
							rmpkg = miss->target;
						} else if(target && !depend) {
							_alpm_log(PM_LOG_DEBUG, _("'%s' is in the target list -- keeping it"),
								miss->target);
							/* remove miss->depend.name */
							rmpkg = miss->depend.name;
						} else {
							/* miss->depend.name is not needed, miss->target already provides
							 * it, let's resolve the conflict */
							rmpkg = miss->depend.name;
						}
						if(rmpkg) {
							pmsyncpkg_t *rsync = find_pkginsync(rmpkg, trans->packages);
							void *vpkg;
							_alpm_log(PM_LOG_DEBUG, _("removing '%s' from target list"), rmpkg);
							trans->packages = alpm_list_remove(trans->packages, rsync, pkg_cmp, &vpkg);
							FREESYNC(vpkg);
							continue;
						}
					}
				}
				/* It's a conflict -- see if they want to remove it
				*/
				_alpm_log(PM_LOG_DEBUG, _("resolving package '%s' conflict"), miss->target);
				if(local) {
					int doremove = 0;
					if(!alpm_list_find_str(asked, miss->depend.name)) {
						QUESTION(trans, PM_TRANS_CONV_CONFLICT_PKG, miss->target, miss->depend.name, NULL, &doremove);
						asked = alpm_list_add(asked, strdup(miss->depend.name));
						if(doremove) {
							pmsyncpkg_t *rsync = find_pkginsync(miss->depend.name, trans->packages);
							pmpkg_t *q = _alpm_pkg_new(miss->depend.name, NULL);
							if(q == NULL) {
								if(data) {
									FREELIST(*data);
								}
								ret = -1;
								goto cleanup;
							}
							q->requiredby = alpm_list_strdup(local->requiredby);
							if(sync->type != PM_SYNC_TYPE_REPLACE) {
								/* switch this sync type to REPLACE */
								sync->type = PM_SYNC_TYPE_REPLACE;
								FREEPKG(sync->data);
							}
							/* append to the replaces list */
							_alpm_log(PM_LOG_DEBUG, _("electing '%s' for removal"), miss->depend.name);
							sync->data = alpm_list_add(sync->data, q);
							if(rsync) {
								/* remove it from the target list */
								void *vpkg;
								_alpm_log(PM_LOG_DEBUG, _("removing '%s' from target list"), miss->depend.name);
								trans->packages = alpm_list_remove(trans->packages, rsync, pkg_cmp, &vpkg);
								FREESYNC(vpkg);
							}
						} else {
							/* abort */
							_alpm_log(PM_LOG_ERROR, _("unresolvable package conflicts detected"));
							errorout = 1;
							if(data) {
								if((miss = (pmdepmissing_t *)malloc(sizeof(pmdepmissing_t))) == NULL) {
									_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(pmdepmissing_t));
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
					_alpm_log(PM_LOG_ERROR, _("unresolvable package conflicts detected"));
					errorout = 1;
					if(data) {
						if((miss = (pmdepmissing_t *)malloc(sizeof(pmdepmissing_t))) == NULL) {
							_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(pmdepmissing_t));
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

	FREELISTPTR(list);

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
			_alpm_log(PM_LOG_DEBUG, _("checking dependencies of packages designated for removal"));
			deps = _alpm_checkdeps(trans, db_local, PM_TRANS_TYPE_REMOVE, list);
			if(deps) {
				int errorout = 0;
				for(i = deps; i; i = i->next) {
					pmdepmissing_t *miss = i->data;
					if(!find_pkginsync(miss->depend.name, trans->packages)) {
						int pfound = 0;
						alpm_list_t *k;
						/* If miss->depend.name depends on something that miss->target and a
						 * package in final both provide, then it's okay...  */
						pmpkg_t *leavingp  = _alpm_db_get_pkgfromcache(db_local, miss->target);
						pmpkg_t *conflictp = _alpm_db_get_pkgfromcache(db_local, miss->depend.name);
						if(!leavingp || !conflictp) {
							_alpm_log(PM_LOG_ERROR, _("something has gone horribly wrong"));
							ret = -1;
							goto cleanup;
						}
						/* Look through the upset package's dependencies and try to match one up
						 * to a provisio from the package we want to remove */
						for(k = conflictp->depends; k && !pfound; k = k->next) {
							alpm_list_t *m;
							for(m = leavingp->provides; m && !pfound; m = m->next) {
								if(!strcmp(k->data, m->data)) {
									/* Found a match -- now look through final for a package that
									 * provides the same thing.  If none are found, then it truly
									 * is an unresolvable conflict. */
									alpm_list_t *n, *o;
									for(n = trans->packages; n && !pfound; n = n->next) {
										pmsyncpkg_t *sp = n->data;
										for(o = sp->pkg->provides; o && !pfound; o = o->next) {
											if(!strcmp(m->data, o->data)) {
												/* found matching provisio -- we're good to go */
												_alpm_log(PM_LOG_DEBUG, _("found '%s' as a provision for '%s' -- conflict aborted"),
														sp->pkg->name, (char *)o->data);
												pfound = 1;
											}
										}
									}
								}
							}
						}
						if(!pfound) {
							if(!errorout) {
								errorout = 1;
							}
							if(data) {
								if((miss = (pmdepmissing_t *)malloc(sizeof(pmdepmissing_t))) == NULL) {
									_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(pmdepmissing_t));
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

#ifndef __sun__
	/* check for free space only in case the packages will be extracted */
	if(!(trans->flags & PM_TRANS_FLAG_NOCONFLICTS)) {
		if(_alpm_check_freespace(trans, data) == -1) {
				/* pm_errno is set by check_freespace */
				ret = -1;
				goto cleanup;
		}
	}
#endif

cleanup:
	FREELISTPTR(list);
	FREELISTPTR(trail);
	FREELIST(asked);

	return(ret);
}

int _alpm_sync_commit(pmtrans_t *trans, pmdb_t *db_local, alpm_list_t **data)
{
	alpm_list_t *i, *j, *files = NULL;
	pmtrans_t *tr = NULL;
	int replaces = 0, retval = 0;
	char ldir[PATH_MAX];
	int varcache = 1;

	ALPM_LOG_FUNC;

	ASSERT(db_local != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));

	trans->state = STATE_DOWNLOADING;
	/* group sync records by repository and download */
	snprintf(ldir, PATH_MAX, "%s%s", handle->root, handle->cachedir);

	for(i = handle->dbs_sync; i; i = i->next) {
		pmdb_t *current = i->data;

		for(j = trans->packages; j; j = j->next) {
			pmsyncpkg_t *sync = j->data;
			pmpkg_t *spkg = sync->pkg;
			pmdb_t *dbs = spkg->data;

			if(current == dbs) {
				const char *fname = NULL;
				char path[PATH_MAX];

				fname = alpm_pkg_get_filename(spkg);
				if(trans->flags & PM_TRANS_FLAG_PRINTURIS) {
					EVENT(trans, PM_TRANS_EVT_PRINTURI, (char *)alpm_db_get_url(current), (char *)fname);
				} else {
					struct stat buf;
					snprintf(path, PATH_MAX, "%s/%s", ldir, fname);
					if(stat(path, &buf)) {
						/* file is not in the cache dir, so add it to the list */
						files = alpm_list_add(files, strdup(fname));
					} else {
						_alpm_log(PM_LOG_DEBUG, _("%s is already in the cache\n"), fname);
					}
				}
			}
		}

		if(files) {
			struct stat buf;
			EVENT(trans, PM_TRANS_EVT_RETRIEVE_START, current->treename, NULL);
			if(stat(ldir, &buf)) {
				/* no cache directory.... try creating it */
				_alpm_log(PM_LOG_WARNING, _("no %s cache exists.  creating...\n"), ldir);
				alpm_logaction(_("warning: no %s cache exists.  creating..."), ldir);
				if(_alpm_makepath(ldir)) {
					/* couldn't mkdir the cache directory, so fall back to /tmp and unlink
					 * the package afterwards.
					 */
					_alpm_log(PM_LOG_WARNING, _("couldn't create package cache, using /tmp instead\n"));
					alpm_logaction(_("warning: couldn't create package cache, using /tmp instead"));
					snprintf(ldir, PATH_MAX, "%stmp", alpm_option_get_root());
					alpm_option_set_cachedir(ldir);
					varcache = 0;
				}
			}
			if(_alpm_downloadfiles(current->servers, ldir, files)) {
				_alpm_log(PM_LOG_WARNING, _("failed to retrieve some files from %s\n"), current->treename);
				RET_ERR(PM_ERR_RETRIEVE, -1);
			}
			FREELIST(files);
		}
	}
	if(trans->flags & PM_TRANS_FLAG_PRINTURIS) {
		return(0);
	}

	/* Check integrity of files */
	EVENT(trans, PM_TRANS_EVT_INTEGRITY_START, NULL, NULL);

	for(i = trans->packages; i; i = i->next) {
		pmsyncpkg_t *sync = i->data;
		pmpkg_t *spkg = sync->pkg;
		char str[PATH_MAX];
		const char *pkgname;
		char *md5sum1, *md5sum2, *sha1sum1, *sha1sum2;
		char *ptr=NULL;

		pkgname = alpm_pkg_get_filename(spkg);
		md5sum1 = spkg->md5sum;
		sha1sum1 = spkg->sha1sum;

		if((md5sum1 == NULL) && (sha1sum1 == NULL)) {
			if((ptr = (char *)malloc(512)) == NULL) {
				RET_ERR(PM_ERR_MEMORY, -1);
			}
			snprintf(ptr, 512, _("can't get md5 or sha1 checksum for package %s\n"), pkgname);
			*data = alpm_list_add(*data, ptr);
			retval = 1;
			continue;
		}
		snprintf(str, PATH_MAX, "%s/%s/%s", handle->root, handle->cachedir, pkgname);
		md5sum2 = _alpm_MDFile(str);
		sha1sum2 = _alpm_SHAFile(str);
		if(md5sum2 == NULL && sha1sum2 == NULL) {
			if((ptr = (char *)malloc(512)) == NULL) {
				RET_ERR(PM_ERR_MEMORY, -1);
			}
			snprintf(ptr, 512, _("can't get md5 or sha1 checksum for package %s\n"), pkgname);
			*data = alpm_list_add(*data, ptr);
			retval = 1;
			continue;
		}
		if((strcmp(md5sum1, md5sum2) != 0) && (strcmp(sha1sum1, sha1sum2) != 0)) {
			int doremove=0;
			if((ptr = (char *)malloc(512)) == NULL) {
				RET_ERR(PM_ERR_MEMORY, -1);
			}
			if(trans->flags & PM_TRANS_FLAG_ALLDEPS) {
				doremove=1;
			} else {
				QUESTION(trans, PM_TRANS_CONV_CORRUPTED_PKG, (char *)pkgname, NULL, NULL, &doremove);
			}
			if(doremove) {
				char str[PATH_MAX];
				snprintf(str, PATH_MAX, "%s%s/%s", handle->root, handle->cachedir, pkgname);
				unlink(str);
				snprintf(ptr, 512, _("archive %s was corrupted (bad MD5 or SHA1 checksum)\n"), pkgname);
			} else {
				snprintf(ptr, 512, _("archive %s is corrupted (bad MD5 or SHA1 checksum)\n"), pkgname);
			}
			*data = alpm_list_add(*data, ptr);
			retval = 1;
		}
		FREE(md5sum2);
		FREE(sha1sum2);
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
		_alpm_log(PM_LOG_ERROR, _("could not create removal transaction"));
		pm_errno = PM_ERR_MEMORY;
		goto error;
	}

	if(_alpm_trans_init(tr, PM_TRANS_TYPE_REMOVE, PM_TRANS_FLAG_NODEPS, NULL, NULL, NULL) == -1) {
		_alpm_log(PM_LOG_ERROR, _("could not initialize the removal transaction"));
		goto error;
	}

	for(i = trans->packages; i; i = i->next) {
		pmsyncpkg_t *sync = i->data;
		if(sync->type == PM_SYNC_TYPE_REPLACE) {
			alpm_list_t *j;
			for(j = sync->data; j; j = j->next) {
				pmpkg_t *pkg = j->data;
				if(!_alpm_pkg_isin(pkg->name, tr->packages)) {
					if(_alpm_trans_addtarget(tr, pkg->name) == -1) {
						goto error;
					}
					replaces++;
				}
			}
		}
	}
	if(replaces) {
		_alpm_log(PM_LOG_DEBUG, _("removing conflicting and to-be-replaced packages"));
		if(_alpm_trans_prepare(tr, data) == -1) {
			_alpm_log(PM_LOG_ERROR, _("could not prepare removal transaction"));
			goto error;
		}
		/* we want the frontend to be aware of commit details */
		tr->cb_event = trans->cb_event;
		if(_alpm_trans_commit(tr, NULL) == -1) {
			_alpm_log(PM_LOG_ERROR, _("could not commit removal transaction"));
			goto error;
		}
	}
	FREETRANS(tr);

	/* install targets */
	_alpm_log(PM_LOG_DEBUG, _("installing packages"));
	tr = _alpm_trans_new();
	if(tr == NULL) {
		_alpm_log(PM_LOG_ERROR, _("could not create transaction"));
		pm_errno = PM_ERR_MEMORY;
		goto error;
	}
	if(_alpm_trans_init(tr, PM_TRANS_TYPE_UPGRADE, trans->flags | PM_TRANS_FLAG_NODEPS, trans->cb_event, trans->cb_conv, trans->cb_progress) == -1) {
		_alpm_log(PM_LOG_ERROR, _("could not initialize transaction"));
		goto error;
	}
	for(i = trans->packages; i; i = i->next) {
		pmsyncpkg_t *sync = i->data;
		pmpkg_t *spkg = sync->pkg;

		const char *fname = NULL;
		char str[PATH_MAX];

		fname = alpm_pkg_get_filename(spkg);
		snprintf(str, PATH_MAX, "%s%s/%s", handle->root, handle->cachedir, fname);
		if(_alpm_trans_addtarget(tr, str) == -1) {
			goto error;
		}
		/* using alpm_list_last() is ok because addtarget() adds the new target at the
		 * end of the tr->packages list */
		spkg = alpm_list_last(tr->packages)->data;
		if(sync->type == PM_SYNC_TYPE_DEPEND) {
			spkg->reason = PM_PKG_REASON_DEPEND;
		}
	}
	if(_alpm_trans_prepare(tr, data) == -1) {
		_alpm_log(PM_LOG_ERROR, _("could not prepare transaction"));
		/* pm_errno is set by trans_prepare */
		goto error;
	}
	if(_alpm_trans_commit(tr, NULL) == -1) {
		_alpm_log(PM_LOG_ERROR, _("could not commit transaction"));
		goto error;
	}
	FREETRANS(tr);

	/* propagate replaced packages' requiredby fields to their new owners */
	if(replaces) {
		_alpm_log(PM_LOG_DEBUG, _("updating database for replaced packages' dependencies"));
		for(i = trans->packages; i; i = i->next) {
			pmsyncpkg_t *sync = i->data;
			if(sync->type == PM_SYNC_TYPE_REPLACE) {
				alpm_list_t *j;
				pmpkg_t *new = _alpm_db_get_pkgfromcache(db_local, sync->pkg->name);
				for(j = sync->data; j; j = j->next) {
					alpm_list_t *k;
					pmpkg_t *old = j->data;
					/* merge lists */
					for(k = old->requiredby; k; k = k->next) {
						if(!alpm_list_find_str(new->requiredby, k->data)) {
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
							for(m = depender->depends; m; m = m->next) {
								if(!strcmp(m->data, old->name)) {
									FREE(m->data);
									m->data = strdup(new->name);
								}
							}
							if(_alpm_db_write(db_local, depender, INFRQ_DEPENDS) == -1) {
								_alpm_log(PM_LOG_ERROR, _("could not update requiredby for database entry %s-%s"),
								          new->name, new->version);
							}
							/* add the new requiredby */
							new->requiredby = alpm_list_add(new->requiredby, strdup(k->data));
						}
					}
				}
				if(_alpm_db_write(db_local, new, INFRQ_DEPENDS) == -1) {
					_alpm_log(PM_LOG_ERROR, _("could not update new database entry %s-%s"),
					          new->name, new->version);
				}
			}
		}
	}

	if(!varcache && !(trans->flags & PM_TRANS_FLAG_DOWNLOADONLY)) {
		/* delete packages */
		for(i = files; i; i = i->next) {
			unlink(i->data);
		}
	}

	/* run ldconfig if it exists */
	if(handle->trans->state != STATE_INTERRUPTED) {
		_alpm_log(PM_LOG_DEBUG, _("running \"ldconfig -r %s\""), handle->root);
		_alpm_ldconfig(handle->root);
	}

	return(0);

error:
	FREETRANS(tr);
	/* commiting failed, so this is still just a prepared transaction */
	trans->state = STATE_PREPARED;
	return(-1);
}

pmsynctype_t SYMEXPORT alpm_sync_get_type(pmsyncpkg_t *sync)
{
	/* Sanity checks */
	ASSERT(sync != NULL, return(-1));

	return sync->type;
}

pmpkg_t SYMEXPORT *alpm_sync_get_package(pmsyncpkg_t *sync)
{
	/* Sanity checks */
	ASSERT(sync != NULL, return(NULL));

	return sync->pkg;
}

void SYMEXPORT *alpm_sync_get_data(pmsyncpkg_t *sync)
{
	/* Sanity checks */
	ASSERT(sync != NULL, return(NULL));

	return sync->data;
}

/* vim: set ts=2 sw=2 noet: */
