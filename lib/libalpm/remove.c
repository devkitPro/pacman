/*
 *  remove.c
 *
 *  Copyright (c) 2002-2007 by Judd Vinet <jvinet@zeroflux.org>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

/* libalpm */
#include "remove.h"
#include "alpm_list.h"
#include "trans.h"
#include "util.h"
#include "log.h"
#include "backup.h"
#include "package.h"
#include "db.h"
#include "cache.h"
#include "deps.h"
#include "handle.h"
#include "alpm.h"

int _alpm_remove_loadtarget(pmtrans_t *trans, pmdb_t *db, char *name)
{
	pmpkg_t *info;

	ALPM_LOG_FUNC;

	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(name != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	if(_alpm_pkg_find(trans->packages, name)) {
		RET_ERR(PM_ERR_TRANS_DUP_TARGET, -1);
	}

	if((info = _alpm_db_get_pkgfromcache(db, name)) == NULL) {
		_alpm_log(PM_LOG_DEBUG, "could not find %s in database\n", name);
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

	_alpm_log(PM_LOG_DEBUG, "adding %s in the targets list\n", info->name);
	trans->packages = alpm_list_add(trans->packages, _alpm_pkg_dup(info));

	return(0);
}

static void remove_prepare_cascade(pmtrans_t *trans, pmdb_t *db,
		alpm_list_t *lp)
{
	ALPM_LOG_FUNC;

	while(lp) {
		alpm_list_t *i;
		for(i = lp; i; i = i->next) {
			pmdepmissing_t *miss = (pmdepmissing_t *)i->data;
			pmpkg_t *info = _alpm_db_get_pkgfromcache(db, miss->target);
			if(info) {
				if(!_alpm_pkg_find(trans->packages, alpm_pkg_get_name(info))) {
					_alpm_log(PM_LOG_DEBUG, "pulling %s in the targets list\n",
							alpm_pkg_get_name(info));
					trans->packages = alpm_list_add(trans->packages, _alpm_pkg_dup(info));
				}
			} else {
				_alpm_log(PM_LOG_ERROR, _("could not find %s in database -- skipping\n"),
									miss->target);
			}
		}
		alpm_list_free_inner(lp, (alpm_list_fn_free)_alpm_depmiss_free);
		alpm_list_free(lp);
		lp = alpm_checkdeps(db, 1, trans->packages, NULL);
	}
}

static void remove_prepare_keep_needed(pmtrans_t *trans, pmdb_t *db,
		alpm_list_t *lp)
{
	ALPM_LOG_FUNC;

	/* Remove needed packages (which break dependencies) from the target list */
	while(lp != NULL) {
		alpm_list_t *i;
		for(i = lp; i; i = i->next) {
			pmdepmissing_t *miss = (pmdepmissing_t *)i->data;
			void *vpkg;
			pmpkg_t *pkg = _alpm_pkg_find(trans->packages, miss->causingpkg);
			if(pkg == NULL) {
				continue;
			}
			trans->packages = alpm_list_remove(trans->packages, pkg, _alpm_pkg_cmp,
					&vpkg);
			pkg = vpkg;
			if(pkg) {
				_alpm_log(PM_LOG_WARNING, "removing %s from the target-list\n",
						alpm_pkg_get_name(pkg));
				_alpm_pkg_free(pkg);
			}
		}
		alpm_list_free_inner(lp, (alpm_list_fn_free)_alpm_depmiss_free);
		alpm_list_free(lp);
		lp = alpm_checkdeps(db, 1, trans->packages, NULL);
	}
}

int _alpm_remove_prepare(pmtrans_t *trans, pmdb_t *db, alpm_list_t **data)
{
	alpm_list_t *lp;

	ALPM_LOG_FUNC;

	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));

	/* skip all checks if we are doing this removal as part of an upgrade */
	if(trans->type == PM_TRANS_TYPE_REMOVEUPGRADE) {
		return(0);
	}

	if((trans->flags & PM_TRANS_FLAG_RECURSE) && !(trans->flags & PM_TRANS_FLAG_CASCADE)) {
		_alpm_log(PM_LOG_DEBUG, "finding removable dependencies\n");
		_alpm_recursedeps(db, trans->packages, trans->flags & PM_TRANS_FLAG_RECURSEALL);
	}

	if(!(trans->flags & PM_TRANS_FLAG_NODEPS)) {
		EVENT(trans, PM_TRANS_EVT_CHECKDEPS_START, NULL, NULL);

		_alpm_log(PM_LOG_DEBUG, "looking for unsatisfied dependencies\n");
		lp = alpm_checkdeps(db, 1, trans->packages, NULL);
		if(lp != NULL) {

			if(trans->flags & PM_TRANS_FLAG_CASCADE) {
				remove_prepare_cascade(trans, db, lp);
			} else if (trans->flags & PM_TRANS_FLAG_UNNEEDED) {
				/* Remove needed packages (which would break dependencies)
				 * from the target list */
				remove_prepare_keep_needed(trans, db, lp);
			} else {
				if(data) {
					*data = lp;
				} else {
					alpm_list_free_inner(lp, (alpm_list_fn_free)_alpm_depmiss_free);
					alpm_list_free(lp);
				}
				RET_ERR(PM_ERR_UNSATISFIED_DEPS, -1);
			}
		}
	}

	/* re-order w.r.t. dependencies */
	_alpm_log(PM_LOG_DEBUG, "sorting by dependencies\n");
	lp = _alpm_sortbydeps(trans->packages, 1);
	/* free the old alltargs */
	alpm_list_free(trans->packages);
	trans->packages = lp;

	/* -Rcs == -Rc then -Rs */
	if((trans->flags & PM_TRANS_FLAG_CASCADE) && (trans->flags & PM_TRANS_FLAG_RECURSE)) {
		_alpm_log(PM_LOG_DEBUG, "finding removable dependencies\n");
		_alpm_recursedeps(db, trans->packages, trans->flags & PM_TRANS_FLAG_RECURSEALL);
	}

	if(!(trans->flags & PM_TRANS_FLAG_NODEPS)) {
		EVENT(trans, PM_TRANS_EVT_CHECKDEPS_DONE, NULL, NULL);
	}

	return(0);
}

static int can_remove_file(pmtrans_t *trans, const char *path)
{
	char file[PATH_MAX+1];

	snprintf(file, PATH_MAX, "%s%s", handle->root, path);

	if(alpm_list_find_str(trans->skip_remove, file)) {
		/* return success because we will never actually remove this file */
		return(1);
	}
	/* If we fail write permissions due to a read-only filesystem, abort.
	 * Assume all other possible failures are covered somewhere else */
	if(access(file, W_OK) == -1) {
		if(errno != EACCES && errno != ETXTBSY && access(file, F_OK) == 0) {
			/* only return failure if the file ACTUALLY exists and we can't write to
			 * it - ignore "chmod -w" simple permission failures */
			_alpm_log(PM_LOG_ERROR, _("cannot remove file '%s': %s\n"),
			          file, strerror(errno));
			return(0);
		}
	}

	return(1);
}

/* Helper function for iterating through a package's file and deleting them
 * Used by _alpm_remove_commit. */
static void unlink_file(pmpkg_t *info, alpm_list_t *lp, pmtrans_t *trans)
{
	struct stat buf;
	int needbackup = 0;
	char file[PATH_MAX+1];

	ALPM_LOG_FUNC;

	char *hash = _alpm_needbackup(lp->data, alpm_pkg_get_backup(info));
	if(hash) {
		needbackup = 1;
		FREE(hash);
	}

	snprintf(file, PATH_MAX, "%s%s", handle->root, (char *)lp->data);

	if(trans->type == PM_TRANS_TYPE_REMOVEUPGRADE) {
		/* check noupgrade */
		if(alpm_list_find_str(handle->noupgrade, lp->data)) {
			_alpm_log(PM_LOG_DEBUG, "Skipping removal of '%s' due to NoUpgrade\n",
					file);
			return;
		}
	}

	/* we want to do a lstat here, and not a _alpm_lstat.
	 * if a directory in the package is actually a directory symlink on the
	 * filesystem, we want to work with the linked directory instead of the
	 * actual symlink */
	if(lstat(file, &buf)) {
		_alpm_log(PM_LOG_DEBUG, "file %s does not exist\n", file);
		return;
	}

	if(S_ISDIR(buf.st_mode)) {
		if(rmdir(file)) {
			/* this is okay, other packages are probably using it (like /usr) */
			_alpm_log(PM_LOG_DEBUG, "keeping directory %s\n", file);
		} else {
			_alpm_log(PM_LOG_DEBUG, "removing directory %s\n", file);
		}
	} else {
		/* check the remove skip list before removing the file.
		 * see the big comment block in db_find_fileconflicts() for an
		 * explanation. */
		if(alpm_list_find_str(trans->skip_remove, file)) {
			_alpm_log(PM_LOG_DEBUG, "%s is in trans->skip_remove, skipping removal\n",
					file);
			return;
		} else if(needbackup) {
			/* if the file is flagged, back it up to .pacsave */
			if(!(trans->flags & PM_TRANS_FLAG_NOSAVE)) {
				char newpath[PATH_MAX];
				snprintf(newpath, PATH_MAX, "%s.pacsave", file);
				rename(file, newpath);
				_alpm_log(PM_LOG_WARNING, _("%s saved as %s\n"), file, newpath);
				alpm_logaction("warning: %s saved as %s\n", file, newpath);
				return;
			} else {
				_alpm_log(PM_LOG_DEBUG, "transaction is set to NOSAVE, not backing up '%s'\n", file);
			}
		}
		_alpm_log(PM_LOG_DEBUG, "unlinking %s\n", file);

		if(unlink(file) == -1) {
			_alpm_log(PM_LOG_ERROR, _("cannot remove file '%s': %s\n"),
								(char *)lp->data, strerror(errno));
		}
	}
}

int _alpm_remove_commit(pmtrans_t *trans, pmdb_t *db)
{
	pmpkg_t *info;
	alpm_list_t *targ, *lp;
	int pkg_count;

	ALPM_LOG_FUNC;

	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));

	pkg_count = alpm_list_count(trans->packages);

	for(targ = trans->packages; targ; targ = targ->next) {
		int position = 0;
		char scriptlet[PATH_MAX];
		alpm_list_t *files;
		info = (pmpkg_t*)targ->data;
		const char *pkgname = NULL;

		if(handle->trans->state == STATE_INTERRUPTED) {
			return(0);
		}

		/* get the name now so we can use it after package is removed */
		pkgname = alpm_pkg_get_name(info);
		snprintf(scriptlet, PATH_MAX, "%s%s-%s/install", db->path,
						 pkgname, alpm_pkg_get_version(info));

		if(trans->type != PM_TRANS_TYPE_REMOVEUPGRADE) {
			EVENT(trans, PM_TRANS_EVT_REMOVE_START, info, NULL);
			_alpm_log(PM_LOG_DEBUG, "removing package %s-%s\n",
								pkgname, alpm_pkg_get_version(info));

			/* run the pre-remove scriptlet if it exists  */
			if(alpm_pkg_has_scriptlet(info) && !(trans->flags & PM_TRANS_FLAG_NOSCRIPTLET)) {
				_alpm_runscriptlet(handle->root, scriptlet, "pre_remove",
				                   alpm_pkg_get_version(info), NULL, trans);
			}
		}

		files = alpm_pkg_get_files(info);

		if(!(trans->flags & PM_TRANS_FLAG_DBONLY)) {
			for(lp = files; lp; lp = lp->next) {
				if(!can_remove_file(trans, lp->data)) {
					_alpm_log(PM_LOG_DEBUG, "not removing package '%s', can't remove all files\n",
					          pkgname);
					RET_ERR(PM_ERR_PKG_CANT_REMOVE, -1);
				}
			}

			int filenum = alpm_list_count(files);
			double percent = 0.0;
			alpm_list_t *newfiles;
			_alpm_log(PM_LOG_DEBUG, "removing %d files\n", filenum);

			/* iterate through the list backwards, unlinking files */
			newfiles = alpm_list_reverse(files);
			for(lp = newfiles; lp; lp = alpm_list_next(lp)) {
				unlink_file(info, lp, trans);

				/* update progress bar after each file */
				percent = (double)position / (double)filenum;
				PROGRESS(trans, PM_TRANS_PROGRESS_REMOVE_START, info->name,
						(double)(percent * 100), pkg_count,
						(pkg_count - alpm_list_count(targ) + 1));
				position++;
			}
			alpm_list_free(newfiles);
		}

		/* set progress to 100% after we finish unlinking files */
		PROGRESS(trans, PM_TRANS_PROGRESS_REMOVE_START, pkgname, 100,
		         pkg_count, (pkg_count - alpm_list_count(targ) + 1));

		if(trans->type != PM_TRANS_TYPE_REMOVEUPGRADE) {
			/* run the post-remove script if it exists  */
			if(alpm_pkg_has_scriptlet(info) && !(trans->flags & PM_TRANS_FLAG_NOSCRIPTLET)) {
				_alpm_runscriptlet(handle->root, scriptlet, "post_remove",
													 alpm_pkg_get_version(info), NULL, trans);
			}
		}

		/* remove the package from the database */
		_alpm_log(PM_LOG_DEBUG, "updating database\n");
		_alpm_log(PM_LOG_DEBUG, "removing database entry '%s'\n", pkgname);
		if(_alpm_db_remove(db, info) == -1) {
			_alpm_log(PM_LOG_ERROR, _("could not remove database entry %s-%s\n"),
			          pkgname, alpm_pkg_get_version(info));
		}
		/* remove the package from the cache */
		if(_alpm_db_remove_pkgfromcache(db, info) == -1) {
			_alpm_log(PM_LOG_ERROR, _("could not remove entry '%s' from cache\n"),
			          pkgname);
		}

		/* call a done event if this isn't an upgrade */
		if(trans->type != PM_TRANS_TYPE_REMOVEUPGRADE) {
			EVENT(trans, PM_TRANS_EVT_REMOVE_DONE, info, NULL);
		}
	}

	/* run ldconfig if it exists */
	if(trans->type != PM_TRANS_TYPE_REMOVEUPGRADE) {
		_alpm_log(PM_LOG_DEBUG, "running \"ldconfig -r %s\"\n", handle->root);
		_alpm_ldconfig(handle->root);
	}

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
