/*
 *  remove.c
 *
 *  Copyright (c) 2006-2010 Pacman Development Team <pacman-dev@archlinux.org>
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
#include "deps.h"
#include "handle.h"
#include "alpm.h"

int SYMEXPORT alpm_remove_target(char *target)
{
	pmpkg_t *info;
	pmtrans_t *trans;
	pmdb_t *db_local;
	alpm_list_t *p;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(target != NULL && strlen(target) != 0, RET_ERR(PM_ERR_WRONG_ARGS, -1));
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	trans = handle->trans;
	db_local = handle->db_local;
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_INITIALIZED, RET_ERR(PM_ERR_TRANS_NOT_INITIALIZED, -1));
	ASSERT(db_local != NULL, RET_ERR(PM_ERR_DB_NULL, -1));


	if(_alpm_pkg_find(trans->remove, target)) {
		RET_ERR(PM_ERR_TRANS_DUP_TARGET, -1);
	}

	if((info = _alpm_db_get_pkgfromcache(db_local, target)) != NULL) {
		_alpm_log(PM_LOG_DEBUG, "adding %s in the target list\n", info->name);
		trans->remove = alpm_list_add(trans->remove, _alpm_pkg_dup(info));
		return(0);
	}

	_alpm_log(PM_LOG_DEBUG, "could not find %s in database\n", target);
	pmgrp_t *grp = alpm_db_readgrp(db_local, target);
	if(grp == NULL) {
		RET_ERR(PM_ERR_PKG_NOT_FOUND, -1);
	}
	for(p = alpm_grp_get_pkgs(grp); p; p = alpm_list_next(p)) {
		pmpkg_t *pkg = alpm_list_getdata(p);
		_alpm_log(PM_LOG_DEBUG, "adding %s in the target list\n", pkg->name);
		trans->remove = alpm_list_add(trans->remove, _alpm_pkg_dup(pkg));
	}

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
				if(!_alpm_pkg_find(trans->remove, alpm_pkg_get_name(info))) {
					_alpm_log(PM_LOG_DEBUG, "pulling %s in target list\n",
							alpm_pkg_get_name(info));
					trans->remove = alpm_list_add(trans->remove, _alpm_pkg_dup(info));
				}
			} else {
				_alpm_log(PM_LOG_ERROR, _("could not find %s in database -- skipping\n"),
									miss->target);
			}
		}
		alpm_list_free_inner(lp, (alpm_list_fn_free)_alpm_depmiss_free);
		alpm_list_free(lp);
		lp = alpm_checkdeps(_alpm_db_get_pkgcache(db), 1, trans->remove, NULL);
	}
}

static void remove_prepare_keep_needed(pmtrans_t *trans, pmdb_t *db,
		alpm_list_t *lp)
{
	ALPM_LOG_FUNC;

	/* Remove needed packages (which break dependencies) from target list */
	while(lp != NULL) {
		alpm_list_t *i;
		for(i = lp; i; i = i->next) {
			pmdepmissing_t *miss = (pmdepmissing_t *)i->data;
			void *vpkg;
			pmpkg_t *pkg = _alpm_pkg_find(trans->remove, miss->causingpkg);
			if(pkg == NULL) {
				continue;
			}
			trans->remove = alpm_list_remove(trans->remove, pkg, _alpm_pkg_cmp,
					&vpkg);
			pkg = vpkg;
			if(pkg) {
				_alpm_log(PM_LOG_WARNING, _("removing %s from target list\n"),
						alpm_pkg_get_name(pkg));
				_alpm_pkg_free(pkg);
			}
		}
		alpm_list_free_inner(lp, (alpm_list_fn_free)_alpm_depmiss_free);
		alpm_list_free(lp);
		lp = alpm_checkdeps(_alpm_db_get_pkgcache(db), 1, trans->remove, NULL);
	}
}

int _alpm_remove_prepare(pmtrans_t *trans, pmdb_t *db, alpm_list_t **data)
{
	alpm_list_t *lp;

	ALPM_LOG_FUNC;

	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));

	if((trans->flags & PM_TRANS_FLAG_RECURSE) && !(trans->flags & PM_TRANS_FLAG_CASCADE)) {
		_alpm_log(PM_LOG_DEBUG, "finding removable dependencies\n");
		_alpm_recursedeps(db, trans->remove, trans->flags & PM_TRANS_FLAG_RECURSEALL);
	}

	if(!(trans->flags & PM_TRANS_FLAG_NODEPS)) {
		EVENT(trans, PM_TRANS_EVT_CHECKDEPS_START, NULL, NULL);

		_alpm_log(PM_LOG_DEBUG, "looking for unsatisfied dependencies\n");
		lp = alpm_checkdeps(_alpm_db_get_pkgcache(db), 1, trans->remove, NULL);
		if(lp != NULL) {

			if(trans->flags & PM_TRANS_FLAG_CASCADE) {
				remove_prepare_cascade(trans, db, lp);
			} else if (trans->flags & PM_TRANS_FLAG_UNNEEDED) {
				/* Remove needed packages (which would break dependencies)
				 * from target list */
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
	lp = _alpm_sortbydeps(trans->remove, 1);
	/* free the old alltargs */
	alpm_list_free(trans->remove);
	trans->remove = lp;

	/* -Rcs == -Rc then -Rs */
	if((trans->flags & PM_TRANS_FLAG_CASCADE) && (trans->flags & PM_TRANS_FLAG_RECURSE)) {
		_alpm_log(PM_LOG_DEBUG, "finding removable dependencies\n");
		_alpm_recursedeps(db, trans->remove, trans->flags & PM_TRANS_FLAG_RECURSEALL);
	}

	if(!(trans->flags & PM_TRANS_FLAG_NODEPS)) {
		EVENT(trans, PM_TRANS_EVT_CHECKDEPS_DONE, NULL, NULL);
	}

	return(0);
}

static int can_remove_file(const char *path, alpm_list_t *skip)
{
	char file[PATH_MAX+1];

	snprintf(file, PATH_MAX, "%s%s", handle->root, path);

	if(alpm_list_find_str(skip, file)) {
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
static void unlink_file(pmpkg_t *info, char *filename, alpm_list_t *skip_remove, int nosave)
{
	struct stat buf;
	char file[PATH_MAX+1];

	ALPM_LOG_FUNC;

	snprintf(file, PATH_MAX, "%s%s", handle->root, filename);

	/* check the remove skip list before removing the file.
	 * see the big comment block in db_find_fileconflicts() for an
	 * explanation. */
	if(alpm_list_find_str(skip_remove, filename)) {
		_alpm_log(PM_LOG_DEBUG, "%s is in skip_remove, skipping removal\n",
				file);
		return;
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
		/* if the file needs backup and has been modified, back it up to .pacsave */
		char *pkghash = _alpm_needbackup(filename, alpm_pkg_get_backup(info));
		if(pkghash) {
			if(nosave) {
				_alpm_log(PM_LOG_DEBUG, "transaction is set to NOSAVE, not backing up '%s'\n", file);
				FREE(pkghash);
			} else {
				char *filehash = alpm_compute_md5sum(file);
				int cmp = strcmp(filehash,pkghash);
				FREE(filehash);
				FREE(pkghash);
				if(cmp != 0) {
					char newpath[PATH_MAX];
					snprintf(newpath, PATH_MAX, "%s.pacsave", file);
					rename(file, newpath);
					_alpm_log(PM_LOG_WARNING, _("%s saved as %s\n"), file, newpath);
					alpm_logaction("warning: %s saved as %s\n", file, newpath);
					return;
				}
			}
		}

		_alpm_log(PM_LOG_DEBUG, "unlinking %s\n", file);

		if(unlink(file) == -1) {
			_alpm_log(PM_LOG_ERROR, _("cannot remove file '%s': %s\n"),
								filename, strerror(errno));
		}
	}
}

int _alpm_upgraderemove_package(pmpkg_t *oldpkg, pmpkg_t *newpkg, pmtrans_t *trans)
{
	alpm_list_t *skip_remove, *b;
	alpm_list_t *newfiles, *lp;
	alpm_list_t *files = alpm_pkg_get_files(oldpkg);
	const char *pkgname = alpm_pkg_get_name(oldpkg);

	ALPM_LOG_FUNC;

	_alpm_log(PM_LOG_DEBUG, "removing old package first (%s-%s)\n",
			oldpkg->name, oldpkg->version);

	/* copy the remove skiplist over */
	skip_remove =
		alpm_list_join(alpm_list_strdup(trans->skip_remove),alpm_list_strdup(handle->noupgrade));
	/* Add files in the NEW backup array to the skip_remove array
	 * so this removal operation doesn't kill them */
	/* old package backup list */
	alpm_list_t *filelist = alpm_pkg_get_files(newpkg);
	for(b = alpm_pkg_get_backup(newpkg); b; b = b->next) {
		char *backup = _alpm_backup_file(b->data);
		/* safety check (fix the upgrade026 pactest) */
		if(!alpm_list_find_str(filelist, backup)) {
			FREE(backup);
			continue;
		}
		_alpm_log(PM_LOG_DEBUG, "adding %s to the skip_remove array\n", backup);
		skip_remove = alpm_list_add(skip_remove, backup);
	}

	for(lp = files; lp; lp = lp->next) {
		if(!can_remove_file(lp->data, skip_remove)) {
			_alpm_log(PM_LOG_DEBUG, "not removing package '%s', can't remove all files\n",
					pkgname);
			RET_ERR(PM_ERR_PKG_CANT_REMOVE, -1);
		}
	}

	/* iterate through the list backwards, unlinking files */
	newfiles = alpm_list_reverse(files);
	for(lp = newfiles; lp; lp = alpm_list_next(lp)) {
		unlink_file(oldpkg, lp->data, skip_remove, 0);
	}
	alpm_list_free(newfiles);
	FREELIST(skip_remove);

	/* remove the package from the database */
	_alpm_log(PM_LOG_DEBUG, "updating database\n");
	_alpm_log(PM_LOG_DEBUG, "removing database entry '%s'\n", pkgname);
	if(_alpm_local_db_remove(handle->db_local, oldpkg) == -1) {
		_alpm_log(PM_LOG_ERROR, _("could not remove database entry %s-%s\n"),
				pkgname, alpm_pkg_get_version(oldpkg));
	}
	/* remove the package from the cache */
	if(_alpm_db_remove_pkgfromcache(handle->db_local, oldpkg) == -1) {
		_alpm_log(PM_LOG_ERROR, _("could not remove entry '%s' from cache\n"),
				pkgname);
	}

	return(0);
}

int _alpm_remove_packages(pmtrans_t *trans, pmdb_t *db)
{
	pmpkg_t *info;
	alpm_list_t *targ, *lp;
	int pkg_count;

	ALPM_LOG_FUNC;

	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));

	pkg_count = alpm_list_count(trans->remove);

	for(targ = trans->remove; targ; targ = targ->next) {
		int position = 0;
		char scriptlet[PATH_MAX];
		info = (pmpkg_t*)targ->data;
		const char *pkgname = NULL;
		int targcount = alpm_list_count(targ);

		if(handle->trans->state == STATE_INTERRUPTED) {
			return(0);
		}

		/* get the name now so we can use it after package is removed */
		pkgname = alpm_pkg_get_name(info);
		snprintf(scriptlet, PATH_MAX, "%s%s-%s/install",
				_alpm_db_path(db), pkgname, alpm_pkg_get_version(info));

		EVENT(trans, PM_TRANS_EVT_REMOVE_START, info, NULL);
		_alpm_log(PM_LOG_DEBUG, "removing package %s-%s\n",
				pkgname, alpm_pkg_get_version(info));

		/* run the pre-remove scriptlet if it exists  */
		if(alpm_pkg_has_scriptlet(info) && !(trans->flags & PM_TRANS_FLAG_NOSCRIPTLET)) {
			_alpm_runscriptlet(handle->root, scriptlet, "pre_remove",
					alpm_pkg_get_version(info), NULL, trans);
		}

		if(!(trans->flags & PM_TRANS_FLAG_DBONLY)) {
			alpm_list_t *files = alpm_pkg_get_files(info);
			for(lp = files; lp; lp = lp->next) {
				if(!can_remove_file(lp->data, NULL)) {
					_alpm_log(PM_LOG_DEBUG, "not removing package '%s', can't remove all files\n",
					          pkgname);
					RET_ERR(PM_ERR_PKG_CANT_REMOVE, -1);
				}
			}

			int filenum = alpm_list_count(files);
			alpm_list_t *newfiles;
			_alpm_log(PM_LOG_DEBUG, "removing %d files\n", filenum);

			/* init progress bar */
			PROGRESS(trans, PM_TRANS_PROGRESS_REMOVE_START, info->name, 0,
					pkg_count, (pkg_count - targcount + 1));

			/* iterate through the list backwards, unlinking files */
			newfiles = alpm_list_reverse(files);
			for(lp = newfiles; lp; lp = alpm_list_next(lp)) {
				double percent;
				unlink_file(info, lp->data, NULL, trans->flags & PM_TRANS_FLAG_NOSAVE);

				/* update progress bar after each file */
				percent = (double)position / (double)filenum;
				PROGRESS(trans, PM_TRANS_PROGRESS_REMOVE_START, info->name,
						(double)(percent * 100), pkg_count,
						(pkg_count - targcount + 1));
				position++;
			}
			alpm_list_free(newfiles);
		}

		/* set progress to 100% after we finish unlinking files */
		PROGRESS(trans, PM_TRANS_PROGRESS_REMOVE_START, pkgname, 100,
		         pkg_count, (pkg_count - targcount + 1));

		/* run the post-remove script if it exists  */
		if(alpm_pkg_has_scriptlet(info) && !(trans->flags & PM_TRANS_FLAG_NOSCRIPTLET)) {
			_alpm_runscriptlet(handle->root, scriptlet, "post_remove",
					alpm_pkg_get_version(info), NULL, trans);
		}

		/* remove the package from the database */
		_alpm_log(PM_LOG_DEBUG, "updating database\n");
		_alpm_log(PM_LOG_DEBUG, "removing database entry '%s'\n", pkgname);
		if(_alpm_local_db_remove(db, info) == -1) {
			_alpm_log(PM_LOG_ERROR, _("could not remove database entry %s-%s\n"),
			          pkgname, alpm_pkg_get_version(info));
		}
		/* remove the package from the cache */
		if(_alpm_db_remove_pkgfromcache(db, info) == -1) {
			_alpm_log(PM_LOG_ERROR, _("could not remove entry '%s' from cache\n"),
			          pkgname);
		}

		EVENT(trans, PM_TRANS_EVT_REMOVE_DONE, info, NULL);
	}

	/* run ldconfig if it exists */
	_alpm_ldconfig(handle->root);

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
