/*
 *  add.c
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
#include "list.h"
#include "cache.h"
#include "rpmvercmp.h"
#include "md5.h"
#include "log.h"
#include "backup.h"
#include "package.h"
#include "db.h"
#include "provide.h"
#include "trans.h"
#include "deps.h"
#include "add.h"
#include "remove.h"
#include "handle.h"

extern pmhandle_t *handle;

int add_loadtarget(pmdb_t *db, pmtrans_t *trans, char *name)
{
	pmpkg_t *info, *dummy;
	PMList *j;

	ASSERT(db != NULL, PM_RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, PM_RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(name != NULL, PM_RET_ERR(PM_ERR_WRONG_ARGS, -1));

	/* ORE
	load_pkg should be done only if pkg has to be added to the transaction */

	_alpm_log(PM_LOG_FLOW2, "reading %s...", name);
	info = pkg_load(name);
	if(info == NULL) {
		/* pm_errno is already set by pkg_load() */
		return(-1);
	}

	/* no additional hyphens in version strings */
	if(strchr(info->version, '-') != strrchr(info->version, '-')) {
		pm_errno = PM_ERR_INVALID_NAME;
		goto error;
	}

	dummy = db_get_pkgfromcache(db, info->name);
	/* only freshen this package if it is already installed and at a lesser version */
	if(trans->flags & PM_TRANS_FLAG_FRESHEN) {
		if(dummy == NULL || rpmvercmp(dummy->version, info->version) >= 0) {
			pm_errno = PM_ERR_PKG_CANT_FRESH;
			goto error;
		}
	}
	/* only install this package if it is not already installed */
	if(trans->type != PM_TRANS_TYPE_UPGRADE) {
		if(dummy) {
			pm_errno = PM_ERR_PKG_INSTALLED;
			goto error;
		}
	}

	/* check if an older version of said package is already in transaction packages.
	 * if so, replace it in the list */
	/* ORE
	we'd better do it before load_pkg. */
	for(j = trans->packages; j; j = j->next) {
		pmpkg_t *pkg = j->data;

		if(strcmp(pkg->name, info->name) == 0) {
			if(rpmvercmp(pkg->version, info->version) < 0) {
				_alpm_log(PM_LOG_WARNING, "replacing older version of %s in target list", pkg->name);
				FREEPKG(j->data);
				j->data = info;
			}
		}
	}

	/* add the package to the transaction */
	trans->packages = pm_list_add(trans->packages, info);

	return(0);

error:
	FREEPKG(info);
	return(-1);
}

int add_prepare(pmdb_t *db, pmtrans_t *trans, PMList **data)
{
	PMList *lp;

	*data = NULL;

	ASSERT(db != NULL, PM_RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, PM_RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(data != NULL, PM_RET_ERR(PM_ERR_WRONG_ARGS, -1));

	/* ORE ???
	No need to check deps if pacman_add was called during a sync:
	it is already done in pacman_sync */

	/* Check dependencies
	 */

	if(!(trans->flags & PM_TRANS_FLAG_NODEPS)) {
		PMList *j;

		TRANS_CB(trans, PM_TRANS_EVT_DEPS_START, NULL, NULL);

		lp = checkdeps(db, trans->type, trans->packages);
		if(lp != NULL) {
			int errorout = 0;

			/* look for unsatisfied dependencies */
			_alpm_log(PM_LOG_FLOW2, "looking for unsatisfied dependencies...");
			for(j = lp; j; j = j->next) {
				pmdepmissing_t* miss = j->data;

				if(miss->type == PM_DEP_DEPEND || miss->type == PM_DEP_REQUIRED) {
					if(!errorout) {
						errorout = 1;
					}
					if((miss = (pmdepmissing_t *)malloc(sizeof(pmdepmissing_t))) == NULL) {
						FREELIST(lp);
						/* ORE, needed or not ?
						FREELIST(*data);*/
						PM_RET_ERR(PM_ERR_MEMORY, -1);
					}
					*miss = *(pmdepmissing_t*)j->data;
					*data = pm_list_add(*data, miss);
				}
			}
			if(errorout) {
				FREELIST(lp);
				PM_RET_ERR(PM_ERR_UNSATISFIED_DEPS, -1);
			}

			/* no unsatisfied deps, so look for conflicts */
			_alpm_log(PM_LOG_FLOW2, "looking for conflicts...");
			for(j = lp; j; j = j->next) {
				pmdepmissing_t* miss = (pmdepmissing_t *)j->data;
				if(miss->type == PM_DEP_CONFLICT) {
					if(!errorout) {
						errorout = 1;
					}
					MALLOC(miss, sizeof(pmdepmissing_t));
					*miss = *(pmdepmissing_t*)j->data;
					*data = pm_list_add(*data, miss);
				}
			}
			if(errorout) {
				FREELIST(lp);
				PM_RET_ERR(PM_ERR_CONFLICTING_DEPS, -1);
			}
			FREELIST(lp);
		}

		/* re-order w.r.t. dependencies */
		_alpm_log(PM_LOG_FLOW2, "sorting by dependencies...");
		lp = sortbydeps(trans->packages);
		/* free the old alltargs */
		for(j = trans->packages; j; j = j->next) {
			j->data = NULL;
		}
		FREELIST(trans->packages);
		trans->packages = lp;

		TRANS_CB(trans, PM_TRANS_EVT_DEPS_DONE, NULL, NULL);
	}

	/* Check for file conflicts
	 */
	if(!(trans->flags & PM_TRANS_FLAG_FORCE)) {
		TRANS_CB(trans, PM_TRANS_EVT_CONFLICTS_START, NULL, NULL);

		lp = db_find_conflicts(db, trans->packages, handle->root);
		if(lp != NULL) {
			*data = lp;
			PM_RET_ERR(PM_ERR_FILE_CONFLICTS, -1);
		}

		TRANS_CB(trans, PM_TRANS_EVT_CONFLICTS_DONE, NULL, NULL);
	}

	return(0);
}

int add_commit(pmdb_t *db, pmtrans_t *trans)
{
	int i, ret = 0, errors = 0;
	TAR *tar = NULL;
	char expath[PATH_MAX];
	time_t t;
	pmpkg_t *info = NULL;
	PMList *targ, *lp;

	ASSERT(db != NULL, PM_RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, PM_RET_ERR(PM_ERR_TRANS_NULL, -1));

	if(trans->packages == NULL) {
		return(0);
	}

	for(targ = trans->packages; targ; targ = targ->next) {
		tartype_t gztype = {
			(openfunc_t)_alpm_gzopen_frontend,
			(closefunc_t)gzclose,
			(readfunc_t)gzread,
			(writefunc_t)gzwrite
		};
		unsigned short pmo_upgrade = (trans->type == PM_TRANS_TYPE_UPGRADE) ? 1 : 0;
		char pm_install[PATH_MAX];
		pmpkg_t *oldpkg = NULL;
		info = (pmpkg_t *)targ->data;
		errors = 0;

		/* see if this is an upgrade.  if so, remove the old package first */
		if(pmo_upgrade) {
			if(pkg_isin(info, db_get_pkgcache(db))) {
				TRANS_CB(trans, PM_TRANS_EVT_UPGRADE_START, info, NULL);

				/* we'll need the full record for backup checks later */
				if((oldpkg = db_scan(db, info->name, INFRQ_ALL)) != NULL) {
					pmtrans_t *tr;

					_alpm_log(PM_LOG_FLOW2, "removing old package first...\n");
					/* ORE
					set flags to something, but what (nodeps?) ??? */
					tr = trans_new();
					if(tr == NULL) {
						PM_RET_ERR(PM_ERR_TRANS_ABORT, -1);
					}
					if(trans_init(tr, PM_TRANS_TYPE_UPGRADE, 0, NULL) == -1) {
						FREETRANS(tr);
						PM_RET_ERR(PM_ERR_TRANS_ABORT, -1);
					}
					if(remove_loadtarget(db, tr, info->name) == -1) {
						FREETRANS(tr);
						PM_RET_ERR(PM_ERR_TRANS_ABORT, -1);
					}
					if(remove_commit(db, tr) == -1) {
						FREETRANS(tr);
						PM_RET_ERR(PM_ERR_TRANS_ABORT, -1);
					}
					FREETRANS(tr);
				}
			} else {
				/* no previous package version is installed, so this is actually just an
				 * install
				 */
				pmo_upgrade = 0;
			}
		}
		if(!pmo_upgrade) {
			TRANS_CB(trans, PM_TRANS_EVT_ADD_START, info, NULL);
		}

		/* Add the package to the database */
		t = time(NULL);

		/* Update the requiredby field by scaning the whole database 
		 * looking for packages depending on the package to add */
		for(lp = db_get_pkgcache(db); lp; lp = lp->next) {
			pmpkg_t *tmpp = NULL;
			PMList *tmppm = NULL;

			tmpp = db_scan(db, ((pmpkg_t *)lp->data)->name, INFRQ_DEPENDS);
			if(tmpp == NULL) {
				continue;
			}
			for(tmppm = tmpp->depends; tmppm; tmppm = tmppm->next) {
				pmdepend_t depend;
				splitdep(tmppm->data, &depend);
				if(tmppm->data && !strcmp(depend.name, info->name)) {
					info->requiredby = pm_list_add(info->requiredby, strdup(tmpp->name));
					continue;
				}
			}
		}

		_alpm_log(PM_LOG_FLOW2, "updating database...");
		/* Figure out whether this package was installed explicitly by the user
		 * or installed as a dependency for another package
		 */
		/* ORE
		info->reason = PM_PKG_REASON_EXPLICIT;
		if(pm_list_is_strin(dependonly, info->data)) {
			info->reason = PM_PKG_REASON_DEPEND;
		}*/
		/* make an install date (in UTC) */
		strncpy(info->installdate, asctime(gmtime(&t)), sizeof(info->installdate));
		if(db_write(db, info, INFRQ_ALL)) {
			_alpm_log(PM_LOG_ERROR, "could not update database for %s", info->name);
			alpm_logaction(NULL, "error updating database for %s!", info->name);
			PM_RET_ERR(PM_ERR_DB_WRITE, -1);
		}

		/* update dependency packages' REQUIREDBY fields */
		for(lp = info->depends; lp; lp = lp->next) {
			pmpkg_t *depinfo = NULL;
			pmdepend_t depend;

			splitdep(lp->data, &depend);
			depinfo = db_scan(db, depend.name, INFRQ_DESC|INFRQ_DEPENDS);
			if(depinfo == NULL) {
				/* look for a provides package */
				PMList *provides = _alpm_db_whatprovides(db, depend.name);
				if(provides) {
					/* use the first one */
					depinfo = db_scan(db, ((pmpkg_t *)provides->data)->name, INFRQ_DEPENDS);
					if(depinfo == NULL) {
						PMList *lp;
						/* wtf */
						for(lp = provides; lp; lp = lp->next) {
							lp->data = NULL;
						}
						pm_list_free(provides);
						continue;
					}
				} else {
					continue;
				}
			}
			depinfo->requiredby = pm_list_add(depinfo->requiredby, strdup(info->name));
			db_write(db, depinfo, INFRQ_DEPENDS);
			FREEPKG(depinfo);
		}

		/* Extract the .tar.gz package */
		if(tar_open(&tar, info->data, &gztype, O_RDONLY, 0, TAR_GNU) == -1) {
			PM_RET_ERR(PM_ERR_PKG_OPEN, -1);
		}
		_alpm_log(PM_LOG_DEBUG, "extracting files...");
		for(i = 0; !th_read(tar); i++) {
			int nb = 0;
			int notouch = 0;
			char *md5_orig = NULL;
			char pathname[PATH_MAX];
			struct stat buf;

			strncpy(pathname, th_get_pathname(tar), PATH_MAX);

			if(!strcmp(pathname, ".PKGINFO") || !strcmp(pathname, ".FILELIST")) {
				tar_skip_regfile(tar);
				continue;
			}

			if(!strcmp(pathname, "._install") || !strcmp(pathname, ".INSTALL")) {
				/* the install script goes inside the db */
				snprintf(expath, PATH_MAX, "%s/%s-%s/install", db->path, info->name, info->version);
			} else {
				/* build the new pathname relative to handle->root */
				snprintf(expath, PATH_MAX, "%s%s", handle->root, pathname);
			}

			if(!stat(expath, &buf) && !S_ISDIR(buf.st_mode)) {
				/* file already exists */
				if(pm_list_is_strin(pathname, handle->noupgrade)) {
					notouch = 1;
				} else {
					if(!pmo_upgrade || oldpkg == NULL) {
						nb = pm_list_is_strin(pathname, info->backup) ? 1 : 0;
					} else {
						/* op == PM_UPGRADE */
						if((md5_orig = _alpm_needbackup(pathname, oldpkg->backup)) != 0) {
							nb = 1;
						}
					}
				}
			}

			if(nb) {
				char *temp;
				char *md5_local, *md5_pkg;

				md5_local = MDFile(expath);
				/* extract the package's version to a temporary file and md5 it */
				temp = strdup("/tmp/pacman_XXXXXX");
				mkstemp(temp);
				if(tar_extract_file(tar, temp)) {
					alpm_logaction("could not extract %s: %s", pathname, strerror(errno));
					errors++;
					continue;
				}
				md5_pkg = MDFile(temp);
				/* append the new md5 hash to it's respective entry in info->backup
				 * (it will be the new orginal)
				 */
				for(lp = info->backup; lp; lp = lp->next) {
					char *fn;

					if(!lp->data) continue;
					if(!strcmp((char*)lp->data, pathname)) {
						/* 32 for the hash, 1 for the terminating NULL, and 1 for the tab delimiter */
						MALLOC(fn, strlen(lp->data)+34);
						sprintf(fn, "%s\t%s", (char*)lp->data, md5_pkg);
						FREE(lp->data);
						lp->data = fn;
					}
				}

				_alpm_log(PM_LOG_FLOW2, " checking md5 hashes for %s", expath);
				_alpm_log(PM_LOG_FLOW2, " current:  %s", md5_local);
				_alpm_log(PM_LOG_FLOW2, " new:      %s", md5_pkg);
				if(md5_orig) {
					_alpm_log(PM_LOG_FLOW2, " original: %s", md5_orig);
				}

				if(!pmo_upgrade) {
					/* PM_ADD */

					/* if a file already exists with a different md5 hash,
					 * then we rename it to a .pacorig extension and continue */
					if(strcmp(md5_local, md5_pkg)) {
						char newpath[PATH_MAX];
						snprintf(newpath, PATH_MAX, "%s.pacorig", expath);
						if(rename(expath, newpath)) {
							_alpm_log(PM_LOG_ERROR, "could not rename %s: %s", expath, strerror(errno));
							alpm_logaction("error: could not rename %s: %s", expath, strerror(errno));
						}
						if(_alpm_copyfile(temp, expath)) {
							_alpm_log(PM_LOG_ERROR, "could not copy %s to %s: %s", temp, expath, strerror(errno));
							alpm_logaction("error: could not copy %s to %s: %s", temp, expath, strerror(errno));
							errors++;
						} else {
							_alpm_log(PM_LOG_WARNING, "warning: %s saved as %s", expath, newpath);
							alpm_logaction("warning: %s saved as %s", expath, newpath);
						}
					}
				} else if(md5_orig) {
					/* PM_UPGRADE */
					int installnew = 0;

					/* the fun part */
					if(!strcmp(md5_orig, md5_local)) {
						if(!strcmp(md5_local, md5_pkg)) {
							_alpm_log(PM_LOG_FLOW2, " action: installing new file");
							installnew = 1;
						} else {
							_alpm_log(PM_LOG_FLOW2, " action: installing new file");
							installnew = 1;
						}
					} else if(!strcmp(md5_orig, md5_pkg)) {
						_alpm_log(PM_LOG_FLOW2, " action: leaving existing file in place");
					} else if(!strcmp(md5_local, md5_pkg)) {
						_alpm_log(PM_LOG_FLOW2, " action: installing new file");
						installnew = 1;
					} else {
						char newpath[PATH_MAX];
						_alpm_log(PM_LOG_FLOW2, " action: saving current file and installing new one");
						installnew = 1;
						snprintf(newpath, PATH_MAX, "%s.pacsave", expath);
						if(rename(expath, newpath)) {
							_alpm_log(PM_LOG_ERROR, "could not rename %s: %s", expath, strerror(errno));
							alpm_logaction("error: could not rename %s: %s", expath, strerror(errno));
						} else {
							_alpm_log(PM_LOG_WARNING, "warning: %s saved as %s", expath, newpath);
							alpm_logaction("warning: %s saved as %s", expath, newpath);
						}
					}

					if(installnew) {
						/*_alpm_log(PM_LOG_FLOW2, " %s", expath);*/
						if(_alpm_copyfile(temp, expath)) {
							_alpm_log(PM_LOG_ERROR, "could not copy %s to %s: %s", temp, expath, strerror(errno));
							errors++;
						}
					}
				}

				FREE(md5_local);
				FREE(md5_pkg);
				FREE(md5_orig);
				unlink(temp);
				FREE(temp);
			} else {
				if(!notouch) {
					_alpm_log(PM_LOG_FLOW2, "%s", pathname);
				} else {
					_alpm_log(PM_LOG_FLOW2, "%s is in NoUpgrade - skipping", pathname);
					strncat(expath, ".pacnew", PATH_MAX);
					_alpm_log(PM_LOG_WARNING, "warning: extracting %s%s as %s", handle->root, pathname, expath);
					alpm_logaction("warning: extracting %s%s as %s", handle->root, pathname, expath);
					tar_skip_regfile(tar);
				}
				if(trans->flags & PM_TRANS_FLAG_FORCE) {
					/* if FORCE was used, then unlink() each file (whether it's there
					 * or not) before extracting.  this prevents the old "Text file busy"
					 * error that crops up if one tries to --force a glibc or pacman
					 * upgrade.
					 */
					unlink(expath);
				}
				if(tar_extract_file(tar, expath)) {
					_alpm_log(PM_LOG_ERROR, "could not extract %s: %s", pathname, strerror(errno));
					alpm_logaction("could not extract %s: %s", pathname, strerror(errno));
					errors++;
				}
				/* calculate an md5 hash if this is in info->backup */
				for(lp = info->backup; lp; lp = lp->next) {
					char *fn, *md5;
					char path[PATH_MAX];

					if(!lp->data) continue;
					if(!strcmp((char*)lp->data, pathname)) {
						snprintf(path, PATH_MAX, "%s%s", handle->root, (char*)lp->data);
						md5 = MDFile(path);
						/* 32 for the hash, 1 for the terminating NULL, and 1 for the tab delimiter */
						MALLOC(fn, strlen(lp->data)+34);
						sprintf(fn, "%s\t%s", (char*)lp->data, md5);
						FREE(lp->data);
						lp->data = fn;
					}
				}
			}
		}
		tar_close(tar);

		if(errors) {
			ret = 1;
			_alpm_log(PM_LOG_ERROR, "errors occurred while %s %s",
				(pmo_upgrade ? "upgrading" : "installing"), info->name);
			alpm_logaction("errors occurred while %s %s",
				(pmo_upgrade ? "upgrading" : "installing"), info->name);
		}

		/* run the post-install script if it exists  */
		snprintf(pm_install, PATH_MAX, "%s%s/%s/%s-%s/install", handle->root, handle->dbpath, db->treename, info->name, info->version);
		if(pmo_upgrade) {
			_alpm_runscriptlet(handle->root, pm_install, "post_upgrade", info->version, oldpkg ? oldpkg->version : NULL);
		} else {
			_alpm_runscriptlet(handle->root, pm_install, "post_install", info->version, NULL);
		}

		if(pmo_upgrade && oldpkg) {
			TRANS_CB(trans, PM_TRANS_EVT_UPGRADE_DONE, info, NULL);
			alpm_logaction("upgraded %s (%s -> %s)", info->name,
				oldpkg->version, info->version);
		} else {
			TRANS_CB(trans, PM_TRANS_EVT_ADD_DONE, info, NULL);
			alpm_logaction("installed %s (%s)", info->name, info->version);
		}

		FREEPKG(oldpkg);
	}

	/* run ldconfig if it exists */
	_alpm_log(PM_LOG_FLOW2, "running \"%ssbin/ldconfig -r %s\"", handle->root, handle->root);
	_alpm_ldconfig(handle->root);

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
