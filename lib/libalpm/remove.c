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
#include <zlib.h>
#include <libintl.h>
/* pacman */
#include "list.h"
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

extern pmhandle_t *handle;

int _alpm_remove_loadtarget(pmtrans_t *trans, pmdb_t *db, char *name)
{
	pmpkg_t *info;

	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(name != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	if(_alpm_pkg_isin(name, trans->packages)) {
		RET_ERR(PM_ERR_TRANS_DUP_TARGET, -1);
	}

	if((info = _alpm_db_scan(db, name, INFRQ_ALL)) == NULL) {
		_alpm_log(PM_LOG_ERROR, _("could not find %s in database"), name);
		RET_ERR(PM_ERR_PKG_NOT_FOUND, -1);
	}

	/* ignore holdpkgs on upgrade */
	if((trans == handle->trans) && _alpm_list_is_strin(info->name, handle->holdpkg)) {
		int resp = 0;
		QUESTION(trans, PM_TRANS_CONV_REMOVE_HOLDPKG, info, NULL, NULL, &resp);
		if(!resp) {
			RET_ERR(PM_ERR_PKG_HOLD, -1);
		}
	}

	_alpm_log(PM_LOG_FLOW2, _("adding %s in the targets list"), info->name);
	trans->packages = _alpm_list_add(trans->packages, info);

	return(0);
}

int _alpm_remove_prepare(pmtrans_t *trans, pmdb_t *db, PMList **data)
{
	PMList *lp;

	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));

	if(!(trans->flags & (PM_TRANS_FLAG_NODEPS)) && (trans->type != PM_TRANS_TYPE_UPGRADE)) {
		EVENT(trans, PM_TRANS_EVT_CHECKDEPS_START, NULL, NULL);

		_alpm_log(PM_LOG_FLOW1, _("looking for unsatisfied dependencies"));
		lp = _alpm_checkdeps(trans, db, trans->type, trans->packages);
		if(lp != NULL) {
			if(trans->flags & PM_TRANS_FLAG_CASCADE) {
				while(lp) {
					PMList *i;
					for(i = lp; i; i = i->next) {
						pmdepmissing_t *miss = (pmdepmissing_t *)i->data;
						pmpkg_t *info = _alpm_db_scan(db, miss->depend.name, INFRQ_ALL);
						if(info) {
							_alpm_log(PM_LOG_FLOW2, _("pulling %s in the targets list"), info->name);
							trans->packages = _alpm_list_add(trans->packages, info);
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
			_alpm_log(PM_LOG_FLOW1, _("finding removable dependencies"));
			trans->packages = _alpm_removedeps(db, trans->packages);
		}

		/* re-order w.r.t. dependencies */ 
		_alpm_log(PM_LOG_FLOW1, _("sorting by dependencies"));
		lp = _alpm_sortbydeps(trans->packages, PM_TRANS_TYPE_REMOVE);
		/* free the old alltargs */
		FREELISTPTR(trans->packages);
		trans->packages = lp;

		EVENT(trans, PM_TRANS_EVT_CHECKDEPS_DONE, NULL, NULL);
	}

	return(0);
}

/* Helper function for comparing strings
 */
static int str_cmp(const void *s1, const void *s2)
{
	return(strcmp(s1, s2));
}

int _alpm_remove_commit(pmtrans_t *trans, pmdb_t *db)
{
	pmpkg_t *info;
	struct stat buf;
	PMList *targ, *lp;
	char line[PATH_MAX+1];

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
			_alpm_log(PM_LOG_FLOW1, _("removing package %s-%s"), info->name, info->version);

			/* run the pre-remove scriptlet if it exists  */
			if(info->scriptlet && !(trans->flags & PM_TRANS_FLAG_NOSCRIPTLET)) {
				snprintf(pm_install, PATH_MAX, "%s/%s-%s/install", db->path, info->name, info->version);
				_alpm_runscriptlet(handle->root, pm_install, "pre_remove", info->version, NULL, trans);
			}
		}

		if(!(trans->flags & PM_TRANS_FLAG_DBONLY)) {
			int filenum = _alpm_list_count(info->files);
			_alpm_log(PM_LOG_FLOW1, _("removing files"));

			/* iterate through the list backwards, unlinking files */
			for(lp = _alpm_list_last(info->files); lp; lp = lp->prev) {
				int nb = 0;
				double percent;
				char *file = lp->data;
				char *md5 =_alpm_needbackup(file, info->backup);
				char *sha1 =_alpm_needbackup(file, info->backup);

				if (position != 0) {
				percent = (double)position / filenum;
				}
				if(md5 && sha1) {
					nb = 1;
					FREE(md5);
					FREE(sha1);
				}
				if(!nb && trans->type == PM_TRANS_TYPE_UPGRADE) {
					/* check noupgrade */
					if(_alpm_list_is_strin(file, handle->noupgrade)) {
						nb = 1;
					}
				}
				snprintf(line, PATH_MAX, "%s%s", handle->root, file);
				if(lstat(line, &buf)) {
					_alpm_log(PM_LOG_DEBUG, _("file %s does not exist"), file);
					continue;
				}
				if(S_ISDIR(buf.st_mode)) {
					if(rmdir(line)) {
						/* this is okay, other packages are probably using it. */
						_alpm_log(PM_LOG_DEBUG, _("keeping directory %s"), file);
					} else {
						_alpm_log(PM_LOG_FLOW2, _("removing directory %s"), file);
					}
				} else {
					/* check the "skip list" before removing the file.
					 * see the big comment block in db_find_conflicts() for an
					 * explanation. */
					int skipit = 0;
					PMList *j;
					for(j = trans->skiplist; j; j = j->next) {
						if(!strcmp(file, (char*)j->data)) {
							skipit = 1;
						}
					}
					if(skipit) {
						_alpm_log(PM_LOG_FLOW2, _("skipping removal of %s as it has moved to another package"),
						          file);
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
									_alpm_log(PM_LOG_WARNING, _("%s saved as %s"), file, newpath);
									alpm_logaction(_("%s saved as %s"), line, newpath);
								} else {
									_alpm_log(PM_LOG_FLOW2, _("unlinking %s"), file);
									if(unlink(line)) {
										_alpm_log(PM_LOG_ERROR, _("cannot remove file %s"), file);
									}
								}
							}
						} else {
							_alpm_log(PM_LOG_FLOW2, _("unlinking %s"), file);
							/* Need at here because we count only real unlinked files ? */
				PROGRESS(trans, PM_TRANS_PROGRESS_REMOVE_START, info->name, (double)(percent * 100), _alpm_list_count(trans->packages), (_alpm_list_count(trans->packages) - _alpm_list_count(targ) +1));
							position++;
							if(unlink(line)) {
								_alpm_log(PM_LOG_ERROR, _("cannot remove file %s"), file);
							}
						}
					}
				}
			}
		}

		if(trans->type != PM_TRANS_TYPE_UPGRADE) {
			/* run the post-remove script if it exists  */
			if(info->scriptlet && !(trans->flags & PM_TRANS_FLAG_NOSCRIPTLET)) {
				char pm_install[PATH_MAX];
				snprintf(pm_install, PATH_MAX, "%s/%s-%s/install", db->path, info->name, info->version);
				_alpm_runscriptlet(handle->root, pm_install, "post_remove", info->version, NULL, trans);
			}
		}

		/* remove the package from the database */
		_alpm_log(PM_LOG_FLOW1, _("updating database"));
		_alpm_log(PM_LOG_FLOW2, _("removing database entry '%s'"), info->name);
		if(_alpm_db_remove(db, info) == -1) {
			_alpm_log(PM_LOG_ERROR, _("could not remove database entry %s-%s"), info->name, info->version);
		}
		if(_alpm_db_remove_pkgfromcache(db, info) == -1) {
			_alpm_log(PM_LOG_ERROR, _("could not remove entry '%s' from cache"), info->name);
		}

		/* update dependency packages' REQUIREDBY fields */
		_alpm_log(PM_LOG_FLOW2, _("updating dependency packages 'requiredby' fields"));
		for(lp = info->depends; lp; lp = lp->next) {
			pmpkg_t *depinfo = NULL;
			pmdepend_t depend;
			char *data;
			if(_alpm_splitdep((char*)lp->data, &depend)) {
				continue;
			}
			/* if this dependency is in the transaction targets, no need to update 
			 * its requiredby info: it is in the process of being removed (if not 
			 * already done!)
			 */
			if(_alpm_pkg_isin(depend.name, trans->packages)) {
				continue;
			}
			depinfo = _alpm_db_get_pkgfromcache(db, depend.name);
			if(depinfo == NULL) {
				/* look for a provides package */
				PMList *provides = _alpm_db_whatprovides(db, depend.name);
				if(provides) {
					/* TODO: should check _all_ packages listed in provides, not just
					 *       the first one.
					 */
					/* use the first one */
					depinfo = _alpm_db_get_pkgfromcache(db, ((pmpkg_t *)provides->data)->name);
					FREELISTPTR(provides);
				}
				if(depinfo == NULL) {
					_alpm_log(PM_LOG_ERROR, _("could not find dependency '%s'"), depend.name);
					/* wtf */
					continue;
				}
			}
			/* splice out this entry from requiredby */
			depinfo->requiredby = _alpm_list_remove(depinfo->requiredby, info->name, str_cmp, (void **)&data);
			FREE(data);
			_alpm_log(PM_LOG_DEBUG, _("updating 'requiredby' field for package '%s'"), depinfo->name);
			if(_alpm_db_write(db, depinfo, INFRQ_DEPENDS)) {
				_alpm_log(PM_LOG_ERROR, _("could not update 'requiredby' database entry %s-%s"),
				          depinfo->name, depinfo->version);
			}
		}

	    PROGRESS(trans, PM_TRANS_PROGRESS_REMOVE_START, info->name, 100, _alpm_list_count(trans->packages), (_alpm_list_count(trans->packages) - _alpm_list_count(targ) +1));
		if(trans->type != PM_TRANS_TYPE_UPGRADE) {
			EVENT(trans, PM_TRANS_EVT_REMOVE_DONE, info, NULL);
		}
	}

	/* run ldconfig if it exists */
	if((trans->type != PM_TRANS_TYPE_UPGRADE) && (handle->trans->state != STATE_INTERRUPTED)) {
		_alpm_log(PM_LOG_FLOW1, _("running \"ldconfig -r %s\""), handle->root);
		_alpm_ldconfig(handle->root);
	}

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
