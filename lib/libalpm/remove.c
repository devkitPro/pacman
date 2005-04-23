/*
 *  remove.c
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
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <zlib.h>
#include <libtar.h>
/* pacman */
#include "util.h"
#include "error.h"
#include "rpmvercmp.h"
#include "md5.h"
#include "log.h"
#include "backup.h"
#include "package.h"
#include "db.h"
#include "cache.h"
#include "deps.h"
#include "provide.h"
#include "remove.h"
#include "handle.h"
#include "alpm.h"

extern pmhandle_t *handle;

int remove_loadtarget(pmtrans_t *trans, pmdb_t *db, char *name)
{
	pmpkg_t *info;

	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(name != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	/* ORE
	we should better find the package in the cache, and then perform a
	db_read(INFRQ_FILES) to add files information to it. */
	if((info = db_scan(db, name, INFRQ_ALL)) == NULL) {
		_alpm_log(PM_LOG_ERROR, "could not find %s in database", name);
		RET_ERR(PM_ERR_PKG_NOT_FOUND, -1);
	}
	trans->packages = pm_list_add(trans->packages, info);

	return(0);
}

int remove_prepare(pmtrans_t *trans, pmdb_t *db, PMList **data)
{
	pmpkg_t *info;
	PMList *lp;

	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(data != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	if(!(trans->flags & (PM_TRANS_FLAG_NODEPS)) && (trans->type != PM_TRANS_TYPE_UPGRADE)) {
		TRANS_CB(trans, PM_TRANS_EVT_CHECKDEPS_START, NULL, NULL);

		_alpm_log(PM_LOG_FLOW1, "looking for unsatisfied dependencies");
		if((lp = checkdeps(db, trans->type, trans->packages)) != NULL) {
			if(trans->flags & PM_TRANS_FLAG_CASCADE) {
				while(lp) {
					PMList *j;
					for(j = lp; j; j = j->next) {
						pmdepmissing_t* miss = (pmdepmissing_t*)j->data;
						info = db_scan(db, miss->depend.name, INFRQ_ALL);
						if(!pkg_isin(info, trans->packages)) {
							trans->packages = pm_list_add(trans->packages, info);
						} else {
							FREEPKG(info);
						}
					}
					FREELIST(lp);
					lp = checkdeps(db, trans->type, trans->packages);
				}
			} else {
				*data = lp;
				RET_ERR(PM_ERR_UNSATISFIED_DEPS, -1);
			}
		}

		if(trans->flags & PM_TRANS_FLAG_RECURSE) {
			_alpm_log(PM_LOG_FLOW1, "finding removable dependencies");
			trans->packages = removedeps(db, trans->packages);
		}

		/* re-order w.r.t. dependencies */ 
		_alpm_log(PM_LOG_FLOW1, "sorting by dependencies");
		lp = sortbydeps(trans->packages, PM_TRANS_TYPE_REMOVE);
		/* free the old alltargs */
		FREELISTPTR(trans->packages);
		trans->packages = lp;

		TRANS_CB(trans, PM_TRANS_EVT_CHECKDEPS_DONE, NULL, NULL);
	}

	return(0);
}

int remove_commit(pmtrans_t *trans, pmdb_t *db)
{
	pmpkg_t *info;
	struct stat buf;
	PMList *targ, *lp;
	char line[PATH_MAX+1];

	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));

	for(targ = trans->packages; targ; targ = targ->next) {
		char pm_install[PATH_MAX];
		info = (pmpkg_t*)targ->data;

		if(trans->type != PM_TRANS_TYPE_UPGRADE) {
			TRANS_CB(trans, PM_TRANS_EVT_REMOVE_START, info, NULL);
			_alpm_log(PM_LOG_FLOW1, "removing package %s-%s", info->name, info->version);

			/* run the pre-remove scriptlet if it exists  */
			if(info->scriptlet) {
				snprintf(pm_install, PATH_MAX, "%s/%s-%s/install", db->path, info->name, info->version);
				_alpm_runscriptlet(handle->root, pm_install, "pre_remove", info->version, NULL);
			}
		}

		if(!(trans->flags & PM_TRANS_FLAG_DBONLY)) {
			_alpm_log(PM_LOG_FLOW1, "removing files");

			/* iterate through the list backwards, unlinking files */
			for(lp = pm_list_last(info->files); lp; lp = lp->prev) {
				int nb = 0;
				char *file = lp->data;
				if(_alpm_needbackup(lp->data, info->backup)) {
					nb = 1;
				}
				if(!nb && trans->type == PM_TRANS_TYPE_UPGRADE) {
					/* check noupgrade */
					if(pm_list_is_strin(lp->data, handle->noupgrade)) {
						nb = 1;
					}
				}
				snprintf(line, PATH_MAX, "%s%s", handle->root, file);
				if(lstat(line, &buf)) {
					_alpm_log(PM_LOG_ERROR, "file %s does not exist", file);
					continue;
				}
				if(S_ISDIR(buf.st_mode)) {
					if(rmdir(line)) {
						/* this is okay, other packages are probably using it. */
						_alpm_log(PM_LOG_DEBUG, "keeping directory %s", file);
					} else {
						_alpm_log(PM_LOG_FLOW2, "removing directory %s", file);
					}
				} else {
					/* if the file is flagged, back it up to .pacsave */
					if(nb) {
						if(trans->type == PM_TRANS_TYPE_UPGRADE) {
							/* we're upgrading so just leave the file as is.  pacman_add() will handle it */
						} else {
							if(!(trans->flags & PM_TRANS_FLAG_NOSAVE)) {
								char newpath[PATH_MAX];
								snprintf(newpath, PATH_MAX, "%s.pacsave", line);
								rename(line, newpath);
								_alpm_log(PM_LOG_WARNING, "%s saved as %s", file, newpath);
								alpm_logaction("%s saved as %s", line, newpath);
							} else {
								_alpm_log(PM_LOG_FLOW2, "unlinking %s", file);
								if(unlink(line)) {
									_alpm_log(PM_LOG_ERROR, "cannot remove file %s", file);
								}
							}
						}
					} else {
						_alpm_log(PM_LOG_FLOW2, "unlinking %s", file);
						if(unlink(line)) {
							_alpm_log(PM_LOG_ERROR, "cannot remove file %s", file);
						}
					}
				}
			}
		}

		if(trans->type != PM_TRANS_TYPE_UPGRADE) {
			/* run the post-remove script if it exists  */
			if(info->scriptlet) {
				char pm_install[PATH_MAX];
				snprintf(pm_install, PATH_MAX, "%s/%s-%s/install", db->path, info->name, info->version);
				_alpm_runscriptlet(handle->root, pm_install, "post_remove", info->version, NULL);
			}
		}

		/* remove the package from the database */
		_alpm_log(PM_LOG_FLOW1, "updating database");
		_alpm_log(PM_LOG_FLOW2, "removing database entry %s", info->name);
		if(db_remove(db, info) == -1) {
			_alpm_log(PM_LOG_ERROR, "could not remove database entry %s/%s-%s", db->treename, info->name, info->version);
		}
		if(db_remove_pkgfromcache(db, info->name) == -1) {
			_alpm_log(PM_LOG_ERROR, "could not remove entry %s from cache", info->name);
		}

		/* update dependency packages' REQUIREDBY fields */
		_alpm_log(PM_LOG_FLOW2, "updating dependency packages 'requiredby' fields");
		for(lp = info->depends; lp; lp = lp->next) {
			PMList *j;
			pmpkg_t *depinfo = NULL;
			pmdepend_t depend;

			if(splitdep((char*)lp->data, &depend)) {
				continue;
			}

			depinfo = db_get_pkgfromcache(db, depend.name);
			if(depinfo == NULL) {
				/* look for a provides package */
				PMList *provides = _alpm_db_whatprovides(db, depend.name);
				if(provides) {
					/* TODO: should check _all_ packages listed in provides, not just
					 *       the first one.
					 */
					/* use the first one */
					depinfo = db_get_pkgfromcache(db, ((pmpkg_t *)provides->data)->name);
					FREELISTPTR(provides);
					if(depinfo == NULL) {
						/* wtf */
						continue;
					}
				} else {
					continue;
				}
			}
			/* splice out this entry from requiredby */
			for(j = depinfo->requiredby; j; j = j->next) {
				if(!strcmp((char*)j->data, info->name)) {
					depinfo->requiredby = _alpm_list_remove(depinfo->requiredby, j);
					break;
				}
			}
			_alpm_log(PM_LOG_DEBUG, "updating 'requiredby' field for package %s", depinfo->name);
			if(db_write(db, depinfo, INFRQ_DEPENDS)) {
				_alpm_log(PM_LOG_ERROR, "could not update 'requiredby' database entry %s/%s-%s", db->treename, depinfo->name, depinfo->version);
			}
		}

		if(trans->type != PM_TRANS_TYPE_UPGRADE) {
			TRANS_CB(trans, PM_TRANS_EVT_REMOVE_DONE, info, NULL);
			alpm_logaction("removed %s (%s)", info->name, info->version);
		}
	}

	/* run ldconfig if it exists */
	if(trans->type != PM_TRANS_TYPE_UPGRADE) {
		_alpm_log(PM_LOG_FLOW1, "running \"ldconfig -r %s\"", handle->root);
		_alpm_ldconfig(handle->root);
	}

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
