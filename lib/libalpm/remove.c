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

int remove_loadtarget(pmdb_t *db, pmtrans_t *trans, char *name)
{
	pmpkg_t *info;

	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(name != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	if((info = db_scan(db, name, INFRQ_ALL)) == NULL) {
		_alpm_log(PM_LOG_ERROR, "could not find %s in database", name);
		RET_ERR(PM_ERR_PKG_NOT_FOUND, -1);
	}
	trans->packages = pm_list_add(trans->packages, info);

	return(0);
}

int remove_prepare(pmdb_t *db, pmtrans_t *trans, PMList **data)
{
	pmpkg_t *info;
	PMList *lp;

	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(data != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	if(!(trans->flags & (PM_TRANS_FLAG_NODEPS)) && (trans->type != PM_TRANS_TYPE_UPGRADE)) {
		TRANS_CB(trans, PM_TRANS_EVT_DEPS_START, NULL, NULL);

		if((lp = checkdeps(db, trans->type, trans->packages)) != NULL) {
			if(trans->flags & PM_TRANS_FLAG_CASCADE) {
				while(lp) {
					PMList *j;
					for(j = lp; j; j = j->next) {
						pmdepmissing_t* miss = (pmdepmissing_t*)j->data;
						info = db_scan(db, miss->depend.name, INFRQ_ALL);
						if(!pkg_isin(info, trans->packages)) {
							trans->packages = pm_list_add(trans->packages, info);
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
			_alpm_log(PM_LOG_FLOW1, "finding removable dependencies...");
			trans->packages = removedeps(db, trans->packages);
		}

		TRANS_CB(trans, PM_TRANS_EVT_DEPS_DONE, NULL, NULL);
	}

	return(0);
}

int remove_commit(pmdb_t *db, pmtrans_t *trans)
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

			/* run the pre-remove scriptlet if it exists  */
			snprintf(pm_install, PATH_MAX, "%s%s/%s/%s-%s/install", handle->root, handle->dbpath, db->treename, info->name, info->version);
			_alpm_runscriptlet(handle->root, pm_install, "pre_remove", info->version, NULL);
		}

		if(!(trans->flags & PM_TRANS_FLAG_DBONLY)) {
			/* iterate through the list backwards, unlinking files */
			for(lp = pm_list_last(info->files); lp; lp = lp->prev) {
				char *newpath = NULL;
				int nb = 0;
				if(_alpm_needbackup(lp->data, info->backup)) {
					nb = 1;
				}
				if(!nb && trans->type == PM_TRANS_TYPE_UPGRADE) {
					/* check noupgrade */
					if(pm_list_is_strin(lp->data, handle->noupgrade)) {
						nb = 1;
					}
				}
				snprintf(line, PATH_MAX, "%s%s", handle->root, (char*)lp->data);
				if(lstat(line, &buf)) {
					_alpm_log(PM_LOG_ERROR, "file %s does not exist", line);
					continue;
				}
				if(S_ISDIR(buf.st_mode)) {
					_alpm_log(PM_LOG_DEBUG, "removing directory %s", line);
					if(rmdir(line)) {
						/* this is okay, other packages are probably using it. */
					}
				} else {
					/* if the file is flagged, back it up to .pacsave */
					if(nb) {
						if(trans->type == PM_TRANS_TYPE_UPGRADE) {
							/* we're upgrading so just leave the file as is.  pacman_add() will handle it */
						} else {
							if(!(trans->flags & PM_TRANS_FLAG_NOSAVE)) {
								newpath = (char*)realloc(newpath, strlen(line)+strlen(".pacsave")+1);
								sprintf(newpath, "%s.pacsave", line);
								rename(line, newpath);
								_alpm_log(PM_LOG_WARNING, "%s saved as %s", line, newpath);
								alpm_logaction("%s saved as %s", line, newpath);
							} else {
								_alpm_log(PM_LOG_DEBUG, "unlinking %s", line);
								if(unlink(line)) {
									_alpm_log(PM_LOG_ERROR, "cannot remove file %s", line);
								}
							}
						}
					} else {
						_alpm_log(PM_LOG_DEBUG, "unlinking %s", line);
						if(unlink(line)) {
							_alpm_log(PM_LOG_ERROR, "cannot remove file %s", line);
						}
					}
				}
			}
		}

		if(trans->type != PM_TRANS_TYPE_UPGRADE) {
			char pm_install[PATH_MAX];

			/* run the post-remove script if it exists  */
			snprintf(pm_install, PATH_MAX, "%s%s/%s/%s-%s/install", handle->root, handle->dbpath, db->treename, info->name, info->version);
			_alpm_runscriptlet(handle->root, pm_install, "post_remove", info->version, NULL);
		}

		/* remove the package from the database */
		if(db_remove(db, info) == -1) {
			_alpm_log(PM_LOG_ERROR, "failed to remove database entry %s/%s-%s", db->path, info->name, info->version);
		}

		/* update dependency packages' REQUIREDBY fields */
		for(lp = info->depends; lp; lp = lp->next) {
			PMList *last, *j;
			pmpkg_t *depinfo = NULL;
			pmdepend_t depend;

			splitdep((char*)lp->data, &depend);

			depinfo = db_scan(db, depend.name, INFRQ_DESC|INFRQ_DEPENDS);
			if(depinfo == NULL) {
				/* look for a provides package */
				PMList *provides = _alpm_db_whatprovides(db, depend.name);
				if(provides) {
					/* TODO: should check _all_ packages listed in provides, not just
					 *       the first one.
					 */
					/* use the first one */
					depinfo = db_scan(db, provides->data, INFRQ_DEPENDS);
					FREELIST(provides);
					if(depinfo == NULL) {
						/* wtf */
						continue;
					}
				} else {
					continue;
				}
			}
			/* splice out this entry from requiredby */
			last = pm_list_last(depinfo->requiredby);
			/* ORE - use list_remove here? */
			for(j = depinfo->requiredby; j; j = j->next) {
				if(!strcmp((char*)j->data, info->name)) {
					if(j == depinfo->requiredby) {
						depinfo->requiredby = j->next;
					}
					if(j->prev)	j->prev->next = j->next;
					if(j->next)	j->next->prev = j->prev;
					/* free the spliced node */
					j->prev = j->next = NULL;
					FREELIST(j);
					break;
				}
			}
			db_write(db, depinfo, INFRQ_DEPENDS);
			FREEPKG(depinfo);
		}

		if(trans->type != PM_TRANS_TYPE_UPGRADE) {
			TRANS_CB(trans, PM_TRANS_EVT_REMOVE_DONE, info, NULL);
			alpm_logaction("removed %s (%s)", info->name, info->version);
		}
	}

	/* run ldconfig if it exists */
	_alpm_log(PM_LOG_FLOW2, "running \"%ssbin/ldconfig -r %s\"", handle->root, handle->root);
	_alpm_ldconfig(handle->root);

	/* cache needs to be rebuilt */
	db_free_pkgcache(db);

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
