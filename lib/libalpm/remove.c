/*
 *  remove.c
 * 
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2005 by Christian Hamar <krics@linuxforum.hu>
 *  Copyright (c) 2006 by David Kimpe <dnaku@frugalware.org>
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

#if defined(__APPLE__) || defined(__OpenBSD__)
#include <sys/syslimits.h>
#endif
#if defined(__APPLE__) || defined(__OpenBSD__) || defined(__sun__)
#include <sys/stat.h>
#endif

#include "config.h"
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <libintl.h>
/* pacman */
#include "alpm_list.h"
#include "trans.h"
#include "util.h"
#include "error.h"
#include "versioncmp.h"
#include "md5.h"
#include "sha1.h"
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

int _alpm_remove_loadtarget(pmtrans_t *trans, pmdb_t *db, char *name)
{
	pmpkg_t *info;

	ALPM_LOG_FUNC;

	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(name != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	if(_alpm_pkg_isin(name, trans->packages)) {
		RET_ERR(PM_ERR_TRANS_DUP_TARGET, -1);
	}

	if((info = _alpm_db_scan(db, name, INFRQ_ALL)) == NULL) {
		/* Unimportant - just ignore it if we can't find it */
		_alpm_log(PM_LOG_DEBUG, _("could not find %s in database"), name);
		RET_ERR(PM_ERR_PKG_NOT_FOUND, -1);
	}

	/* ignore holdpkgs on upgrade */
	if((trans == handle->trans)
	    && alpm_list_find_str(handle->holdpkg, info->name)) {
		int resp = 0;
		QUESTION(trans, PM_TRANS_CONV_REMOVE_HOLDPKG, info, NULL, NULL, &resp);
		if(!resp) {
			RET_ERR(PM_ERR_PKG_HOLD, -1);
		}
	}

	_alpm_log(PM_LOG_DEBUG, _("adding %s in the targets list"), info->name);
	trans->packages = alpm_list_add(trans->packages, info);

	return(0);
}

int _alpm_remove_prepare(pmtrans_t *trans, pmdb_t *db, alpm_list_t **data)
{
	alpm_list_t *lp;

	ALPM_LOG_FUNC;

	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));

	if(!(trans->flags & (PM_TRANS_FLAG_NODEPS)) && (trans->type != PM_TRANS_TYPE_UPGRADE)) {
		EVENT(trans, PM_TRANS_EVT_CHECKDEPS_START, NULL, NULL);

		_alpm_log(PM_LOG_DEBUG, _("looking for unsatisfied dependencies"));
		lp = _alpm_checkdeps(trans, db, trans->type, trans->packages);
		if(lp != NULL) {
			if(trans->flags & PM_TRANS_FLAG_CASCADE) {
				while(lp) {
					alpm_list_t *i;
					for(i = lp; i; i = i->next) {
						pmdepmissing_t *miss = (pmdepmissing_t *)i->data;
						pmpkg_t *info = _alpm_db_scan(db, miss->depend.name, INFRQ_ALL);
						if(info) {
							_alpm_log(PM_LOG_DEBUG, _("pulling %s in the targets list"), info->name);
							trans->packages = alpm_list_add(trans->packages, info);
						} else {
							_alpm_log(PM_LOG_ERROR, _("could not find %s in database -- skipping"),
							          miss->depend.name);
						}
					}
					FREELIST(lp);
					lp = _alpm_checkdeps(trans, db, trans->type, trans->packages);
				}
			} else {
				if(data) {
					*data = lp;
				} else {
					FREELIST(lp);
				}
				RET_ERR(PM_ERR_UNSATISFIED_DEPS, -1);
			}
		}

		if(trans->flags & PM_TRANS_FLAG_RECURSE) {
			_alpm_log(PM_LOG_DEBUG, _("finding removable dependencies"));
			trans->packages = _alpm_removedeps(db, trans->packages);
		}

		/* re-order w.r.t. dependencies */ 
		_alpm_log(PM_LOG_DEBUG, _("sorting by dependencies"));
		lp = _alpm_sortbydeps(trans->packages, PM_TRANS_TYPE_REMOVE);
		/* free the old alltargs */
		FREELISTPTR(trans->packages);
		trans->packages = lp;

		EVENT(trans, PM_TRANS_EVT_CHECKDEPS_DONE, NULL, NULL);
	}

	return(0);
}

static int can_remove_file(const char *path)
{
	alpm_list_t *i;
	char file[PATH_MAX+1];

	snprintf(file, PATH_MAX, "%s%s", handle->root, path);

	for(i = handle->trans->skiplist; i; i = i->next) {
		if(strcmp(file, i->data) == 0) {
			/* skipping this file, return success because "removing" this
			 * file does nothing */
			return(1);
		}
	}
	/* If we fail write permissions due to a read-only filesystem, abort.
	 * Assume all other possible failures are covered somewhere else */
	if(access(file, W_OK) == -1) {
		if(access(file, F_OK) == 0) {
			/* only return failure if the file ACTUALLY exists and we don't have
			 * permissions */
			_alpm_log(PM_LOG_ERROR, _("cannot remove file '%s': %s"), file, strerror(errno));
			return(0);
		}
	}

	return(1);
}

/* Helper function for iterating through a package's file and deleting them
 * Used by _alpm_remove_commit
 *
 * TODO the parameters are a bit out of control here.  This function doesn't
 * need to report PROGRESS, do it in the parent function.
*/
static void unlink_file(pmpkg_t *info, alpm_list_t *lp, alpm_list_t *targ,
												pmtrans_t *trans, int filenum, int *position)
{
	struct stat buf;
	int needbackup = 0;
	double percent = 0.0;
	char file[PATH_MAX+1];

	ALPM_LOG_FUNC;

	if(*position != 0) {
		percent = (double)*position / filenum;
	}

	char *hash = _alpm_needbackup(lp->data, info->backup);
	if(hash) {
		needbackup = 1;
		FREE(hash);
	}
	
	if(!needbackup && trans->type == PM_TRANS_TYPE_UPGRADE) {
		/* check noupgrade */
		if(alpm_list_find_str(handle->noupgrade, lp->data)) {
			needbackup = 1;
		}
	}

	snprintf(file, PATH_MAX, "%s%s", handle->root, (char *)lp->data);
	if(lstat(file, &buf)) {
		_alpm_log(PM_LOG_DEBUG, _("file %s does not exist"), file);
		return;
	}
	
	if(S_ISDIR(buf.st_mode)) {
		if(rmdir(file)) {
			/* this is okay, other pakcages are probably using it (like /usr) */
			_alpm_log(PM_LOG_DEBUG, _("keeping directory %s"), file);
		} else {
			_alpm_log(PM_LOG_DEBUG, _("removing directory %s"), file);
		}
	} else {
		/* check the "skip list" before removing the file.
		 * see the big comment block in db_find_conflicts() for an
		 * explanation. */
		int skipit = 0;
		alpm_list_t *j;
		for(j = trans->skiplist; j; j = j->next) {
			if(!strcmp(lp->data, (char*)j->data)) {
				skipit = 1;
			}
		}
		if(skipit) {
			_alpm_log(PM_LOG_WARNING, _("%s has changed ownership, skipping removal"),
								file);
		} else if(needbackup) {
			/* if the file is flagged, back it up to .pacsave */
			if(!(trans->type == PM_TRANS_TYPE_UPGRADE)) {
				/* if it was an upgrade, the file would be left alone because
				 * pacman_add() would handle it */
				if(!(trans->type & PM_TRANS_FLAG_NOSAVE)) {
					char newpath[PATH_MAX];
					snprintf(newpath, PATH_MAX, "%s.pacsave", file);
					rename(file, newpath);
					_alpm_log(PM_LOG_WARNING, _("%s saved as %s"), file, newpath);
				}
			}
		} else {
			_alpm_log(PM_LOG_DEBUG, _("unlinking %s"), file);
			int list_count = alpm_list_count(trans->packages); /* this way we don't have to call alpm_list_count twice during PROGRESS */

			PROGRESS(trans, PM_TRANS_PROGRESS_REMOVE_START, info->name, (double)(percent * 100), list_count, (list_count - alpm_list_count(targ) + 1));
			++(*position);

			if(unlink(file) == -1) {
				_alpm_log(PM_LOG_ERROR, _("cannot remove file %s: %s"), lp->data, strerror(errno));
			}
		}
	}
}

int _alpm_remove_commit(pmtrans_t *trans, pmdb_t *db)
{
	pmpkg_t *info;
	alpm_list_t *targ, *lp;

	ALPM_LOG_FUNC;

	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));

	for(targ = trans->packages; targ; targ = targ->next) {
		int position = 0;
		char pm_install[PATH_MAX];
		info = (pmpkg_t*)targ->data;

		if(handle->trans->state == STATE_INTERRUPTED) {
			break;
		}

		if(trans->type != PM_TRANS_TYPE_UPGRADE) {
			EVENT(trans, PM_TRANS_EVT_REMOVE_START, info, NULL);
			_alpm_log(PM_LOG_DEBUG, _("removing package %s-%s"), info->name, info->version);

			/* run the pre-remove scriptlet if it exists  */
			if(info->scriptlet && !(trans->flags & PM_TRANS_FLAG_NOSCRIPTLET)) {
				snprintf(pm_install, PATH_MAX, "%s/%s-%s/install", db->path, info->name, info->version);
				_alpm_runscriptlet(handle->root, pm_install, "pre_remove", info->version, NULL, trans);
			}
		}

		if(!(trans->flags & PM_TRANS_FLAG_DBONLY)) {
			for(lp = info->files; lp; lp = lp->next) {
				if(!can_remove_file(lp->data)) {
					_alpm_log(PM_LOG_DEBUG, _("not removing package '%s', can't remove all files"), info->name);
					RET_ERR(PM_ERR_PKG_CANT_REMOVE, -1);
				}
			}

			int filenum = alpm_list_count(info->files);
			_alpm_log(PM_LOG_DEBUG, _("removing files"));

			/* iterate through the list backwards, unlinking files */
			for(lp = alpm_list_last(info->files); lp; lp = lp->prev) {
				unlink_file(info, lp, targ, trans, filenum, &position);
			}
		}

		if(trans->type != PM_TRANS_TYPE_UPGRADE) {
			/* run the post-remove script if it exists  */
			if(info->scriptlet && !(trans->flags & PM_TRANS_FLAG_NOSCRIPTLET)) {
				snprintf(pm_install, PATH_MAX, "%s/%s-%s/install", db->path, info->name, info->version);
				_alpm_runscriptlet(handle->root, pm_install, "post_remove", info->version, NULL, trans);
			}
		}

		/* remove the package from the database */
		_alpm_log(PM_LOG_DEBUG, _("updating database"));
		_alpm_log(PM_LOG_DEBUG, _("removing database entry '%s'"), info->name);
		if(_alpm_db_remove(db, info) == -1) {
			_alpm_log(PM_LOG_ERROR, _("could not remove database entry %s-%s"), info->name, info->version);
		}
		if(_alpm_db_remove_pkgfromcache(db, info) == -1) {
			_alpm_log(PM_LOG_ERROR, _("could not remove entry '%s' from cache"), info->name);
		}

		/* update dependency packages' REQUIREDBY fields */
		_alpm_trans_update_depends(trans, info);

		PROGRESS(trans, PM_TRANS_PROGRESS_REMOVE_START, info->name, 100, alpm_list_count(trans->packages), (alpm_list_count(trans->packages) - alpm_list_count(targ) +1));
		if(trans->type != PM_TRANS_TYPE_UPGRADE) {
			EVENT(trans, PM_TRANS_EVT_REMOVE_DONE, info, NULL);
		}
	}

	/* run ldconfig if it exists */
	if((trans->type != PM_TRANS_TYPE_UPGRADE) && (handle->trans->state != STATE_INTERRUPTED)) {
		_alpm_log(PM_LOG_DEBUG, _("running \"ldconfig -r %s\""), handle->root);
		_alpm_ldconfig(handle->root);
	}

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
