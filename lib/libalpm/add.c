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
#include "conflict.h"
#include "trans.h"
#include "deps.h"
#include "add.h"
#include "remove.h"
#include "handle.h"

extern pmhandle_t *handle;

int add_loadtarget(pmtrans_t *trans, pmdb_t *db, char *name)
{
	pmpkg_t *info = NULL;
	pmpkg_t *dummy;
	char pkgname[PKG_NAME_LEN], pkgver[PKG_VERSION_LEN];
	PMList *i;
	struct stat buf;

	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(name != NULL && strlen(name) != 0, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	/* Check if we need to add a fake target to the transaction.
	 * format: field1=value1|field2=value2|...
	 * Supported fields are "name", "version", "depend"
	 * A fake package is created with the given tokens (name and version are
	 * required).
	 */
	if(strchr(name, '|')) {
		char *str, *ptr, *p;
		dummy = pkg_new();
		if(dummy == NULL) {
			pm_errno = PM_ERR_MEMORY;
			goto error;
		}
		str = strdup(name);
		ptr = str;
		while((p = strsep(&ptr, "|")) != NULL) {
			char *q;
			if(p[0] == 0) {
				continue;
			}
			q = strchr(p, '=');
			if(q == NULL) { /* not a valid token */
				continue;
			}
			if(strncmp("name", p, q-p) == 0) {
				STRNCPY(dummy->name, q+1, PKG_NAME_LEN);
			} else if(strncmp("version", p, q-p) == 0) {
				STRNCPY(dummy->version, q+1, PKG_VERSION_LEN);
			} else if(strncmp("depend", p, q-p) == 0) {
				dummy->depends = pm_list_add(dummy->depends, strdup(q+1));
			} else {
				_alpm_log(PM_LOG_ERROR, "could not parse token %s", p);
			}
		}
		FREE(str);
		if(dummy->name[0] == 0 || dummy->version[0] == 0) {
			pm_errno = PM_ERR_PKG_INVALID_NAME;
			FREEPKG(dummy);
			goto error;
		}
		/* add the package to the transaction */
		trans->packages = pm_list_add(trans->packages, dummy);
		return(0);
	}

	if(stat(name, &buf)) {
		pm_errno = PM_ERR_NOT_A_FILE;
		goto error;
	}

	_alpm_log(PM_LOG_FLOW2, "loading target %s", name);

	if(pkg_splitname(name, pkgname, pkgver) == -1) {
		pm_errno = PM_ERR_PKG_INVALID_NAME;
		goto error;
	}

	/* no additional hyphens in version strings */
	if(strchr(pkgver, '-') != strrchr(pkgver, '-')) {
		pm_errno = PM_ERR_PKG_INVALID_NAME;
		goto error;
	}

	if(trans->type != PM_TRANS_TYPE_UPGRADE) {
		/* only install this package if it is not already installed */
		dummy = db_get_pkgfromcache(db, pkgname);
		if(dummy) {
			pm_errno = PM_ERR_PKG_INSTALLED;
			goto error;
		}
	} else {
		if(trans->flags & PM_TRANS_FLAG_FRESHEN) {
			/* only upgrade/install this package if it is already installed and at a lesser version */
			dummy = db_get_pkgfromcache(db, pkgname);
			if(dummy == NULL || rpmvercmp(dummy->version, pkgver) >= 0) {
				pm_errno = PM_ERR_PKG_CANT_FRESH;
				goto error;
			}
		}
	}

	/* check if an older version of said package is already in transaction packages.
	 * if so, replace it in the list */
	for(i = trans->packages; i; i = i->next) {
		pmpkg_t *pkg = i->data;
		if(strcmp(pkg->name, pkgname) == 0) {
			if(rpmvercmp(pkg->version, pkgver) < 0) {
				_alpm_log(PM_LOG_WARNING, "replacing older version of %s %s by %s in target list", pkg->name, pkg->version, pkgver);
				FREEPKG(i->data);
				i->data = pkg_load(name);
				if(i->data == NULL) {
					/* pm_errno is already set by pkg_load() */
					goto error;
				}
			}
			return(0);
		}
	}

	_alpm_log(PM_LOG_FLOW2, "reading %s", name);
	info = pkg_load(name);
	if(info == NULL) {
		/* pm_errno is already set by pkg_load() */
		goto error;
	}
	/* check to verify we're not getting fooled by a corrupted package */
	if(strcmp(pkgname, info->name) != 0 || strcmp(pkgver, info->version) != 0) {
		pm_errno = PM_ERR_PKG_INVALID;
		goto error;
	}

	/* set the reason to EXPLICIT by default
	 * it will be overwritten in the case of an upgrade or a sync operation */
	info->reason = PM_PKG_REASON_EXPLICIT;

	/* add the package to the transaction */
	trans->packages = pm_list_add(trans->packages, info);

	return(0);

error:
	FREEPKG(info);
	return(-1);
}

int add_prepare(pmtrans_t *trans, pmdb_t *db, PMList **data)
{
	PMList *lp;

	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(data != NULL, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	*data = NULL;

	/* Check dependencies
	 */
	if(!(trans->flags & PM_TRANS_FLAG_NODEPS)) {
		PMList *i;

		EVENT(trans, PM_TRANS_EVT_CHECKDEPS_START, NULL, NULL);

		_alpm_log(PM_LOG_FLOW1, "looking for conflicts or unsatisfied dependencies");
		lp = checkdeps(db, trans->type, trans->packages);
		if(lp != NULL) {
			int errorout = 0;

			/* look for unsatisfied dependencies */
			_alpm_log(PM_LOG_FLOW2, "looking for unsatisfied dependencies");
			for(i = lp; i; i = i->next) {
				pmdepmissing_t* miss = i->data;

				if(miss->type == PM_DEP_TYPE_DEPEND || miss->type == PM_DEP_TYPE_REQUIRED) {
					if(!errorout) {
						errorout = 1;
					}
					if((miss = (pmdepmissing_t *)malloc(sizeof(pmdepmissing_t))) == NULL) {
						FREELIST(lp);
						FREELIST(*data);
						RET_ERR(PM_ERR_MEMORY, -1);
					}
					*miss = *(pmdepmissing_t*)i->data;
					*data = pm_list_add(*data, miss);
				}
			}
			if(errorout) {
				FREELIST(lp);
				RET_ERR(PM_ERR_UNSATISFIED_DEPS, -1);
			}

			/* no unsatisfied deps, so look for conflicts */
			_alpm_log(PM_LOG_FLOW2, "looking for conflicts");
			for(i = lp; i; i = i->next) {
				pmdepmissing_t* miss = (pmdepmissing_t *)i->data;
				if(miss->type == PM_DEP_TYPE_CONFLICT) {
					if(!errorout) {
						errorout = 1;
					}
					MALLOC(miss, sizeof(pmdepmissing_t));
					*miss = *(pmdepmissing_t*)i->data;
					*data = pm_list_add(*data, miss);
				}
			}
			if(errorout) {
				FREELIST(lp);
				RET_ERR(PM_ERR_CONFLICTING_DEPS, -1);
			}
			FREELIST(lp);
		}

		/* re-order w.r.t. dependencies */
		_alpm_log(PM_LOG_FLOW1, "sorting by dependencies");
		lp = sortbydeps(trans->packages, PM_TRANS_TYPE_ADD);
		/* free the old alltargs */
		FREELISTPTR(trans->packages);
		trans->packages = lp;

		EVENT(trans, PM_TRANS_EVT_CHECKDEPS_DONE, NULL, NULL);
	}

	/* Check for file conflicts
	 */
	if(!(trans->flags & PM_TRANS_FLAG_FORCE)) {
		EVENT(trans, PM_TRANS_EVT_FILECONFLICTS_START, NULL, NULL);

		_alpm_log(PM_LOG_FLOW1, "looking for file conflicts");
		lp = db_find_conflicts(db, trans->packages, handle->root);
		if(lp != NULL) {
			*data = lp;
			RET_ERR(PM_ERR_FILE_CONFLICTS, -1);
		}

		EVENT(trans, PM_TRANS_EVT_FILECONFLICTS_DONE, NULL, NULL);
	}

	return(0);
}

int add_commit(pmtrans_t *trans, pmdb_t *db)
{
	int i, ret = 0, errors = 0;
	TAR *tar = NULL;
	char expath[PATH_MAX];
	time_t t;
	PMList *targ, *lp;

	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));

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
		unsigned short pmo_upgrade;
		char pm_install[PATH_MAX];
		pmpkg_t *info = (pmpkg_t *)targ->data;
		pmpkg_t *oldpkg = NULL;
		errors = 0;

		pmo_upgrade = (trans->type == PM_TRANS_TYPE_UPGRADE) ? 1 : 0;

		/* see if this is an upgrade.  if so, remove the old package first */
		if(pmo_upgrade) {
			pmpkg_t *local = db_get_pkgfromcache(db, info->name);
			if(local) {
				EVENT(trans, PM_TRANS_EVT_UPGRADE_START, info, NULL);
				_alpm_log(PM_LOG_FLOW1, "upgrading package %s-%s", info->name, info->version);

				/* we'll need to save some record for backup checks later */
				oldpkg = pkg_new();
				if(oldpkg) {
					STRNCPY(oldpkg->name, local->name, PKG_NAME_LEN);
					STRNCPY(oldpkg->version, local->version, PKG_VERSION_LEN);
					oldpkg->backup = _alpm_list_strdup(local->backup);
				}

				/* pre_upgrade scriptlet */
				if(info->scriptlet) {
					_alpm_runscriptlet(handle->root, info->data, "pre_upgrade", info->version, oldpkg ? oldpkg->version : NULL);
				}

				if(oldpkg) {
					pmtrans_t *tr;

					_alpm_log(PM_LOG_FLOW1, "removing old package first (%s-%s)", oldpkg->name, oldpkg->version);
					tr = trans_new();
					if(tr == NULL) {
						RET_ERR(PM_ERR_TRANS_ABORT, -1);
					}
					if(trans_init(tr, PM_TRANS_TYPE_UPGRADE, trans->flags, NULL) == -1) {
						FREETRANS(tr);
						RET_ERR(PM_ERR_TRANS_ABORT, -1);
					}
					/* copy over the install reason */
					info->reason = local->reason;
					if(remove_loadtarget(tr, db, info->name) == -1) {
						FREETRANS(tr);
						RET_ERR(PM_ERR_TRANS_ABORT, -1);
					}
					if(remove_commit(tr, db) == -1) {
						FREETRANS(tr);
						RET_ERR(PM_ERR_TRANS_ABORT, -1);
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
			EVENT(trans, PM_TRANS_EVT_ADD_START, info, NULL);
			_alpm_log(PM_LOG_FLOW1, "adding package %s-%s", info->name, info->version);

			/* pre_install scriptlet */
			if(info->scriptlet) {
				_alpm_runscriptlet(handle->root, info->data, "pre_install", info->version, NULL);
			}
		} else {
			_alpm_log(PM_LOG_FLOW1, "adding new package (%s-%s)", info->name, info->version);
		}

		/* Add the package to the database */
		t = time(NULL);

		/* Update the requiredby field by scaning the whole database 
		 * looking for packages depending on the package to add */
		for(lp = db_get_pkgcache(db); lp; lp = lp->next) {
			pmpkg_t *tmpp = lp->data;
			PMList *tmppm = NULL;
			if(tmpp == NULL) {
				continue;
			}
			for(tmppm = tmpp->depends; tmppm; tmppm = tmppm->next) {
				pmdepend_t depend;
				if(splitdep(tmppm->data, &depend)) {
					continue;
				}
				if(tmppm->data && !strcmp(depend.name, info->name)) {
					info->requiredby = pm_list_add(info->requiredby, strdup(tmpp->name));
				}
			}
		}

		/* make an install date (in UTC) */
		STRNCPY(info->installdate, asctime(gmtime(&t)), sizeof(info->installdate));
		/* remove the extra line feed appended by asctime() */
		info->installdate[strlen(info->installdate)-1] = 0;

		_alpm_log(PM_LOG_FLOW1, "updating database");
		_alpm_log(PM_LOG_FLOW2, "adding database entry %s", info->name);
		if(db_write(db, info, INFRQ_ALL)) {
			_alpm_log(PM_LOG_ERROR, "could not update database entry %s/%s-%s", db->treename, info->name, info->version);
			alpm_logaction(NULL, "error updating database for %s-%s!", info->name, info->version);
			RET_ERR(PM_ERR_DB_WRITE, -1);
		}
		if(db_add_pkgincache(db, info) == -1) {
			_alpm_log(PM_LOG_ERROR, "could not add entry %s in cache", info->name);
		}

		/* update dependency packages' REQUIREDBY fields */
		_alpm_log(PM_LOG_FLOW2, "updating dependency packages 'requiredby' fields");
		for(lp = info->depends; lp; lp = lp->next) {
			pmpkg_t *depinfo;
			pmdepend_t depend;
			if(splitdep(lp->data, &depend)) {
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
			depinfo->requiredby = pm_list_add(depinfo->requiredby, strdup(info->name));
			_alpm_log(PM_LOG_DEBUG, "updating 'requiredby' field for package %s", depinfo->name);
			if(db_write(db, depinfo, INFRQ_DEPENDS)) {
				_alpm_log(PM_LOG_ERROR, "could not update 'requiredby' database entry %s/%s-%s", db->treename, depinfo->name, depinfo->version);
			}
		}

		/* Extract the .tar.gz package */
		if(tar_open(&tar, info->data, &gztype, O_RDONLY, 0, TAR_GNU) == -1) {
			RET_ERR(PM_ERR_PKG_OPEN, -1);
		}
		_alpm_log(PM_LOG_FLOW1, "extracting files");
		for(i = 0; !th_read(tar); i++) {
			int nb = 0;
			int notouch = 0;
			char *md5_orig = NULL;
			char pathname[PATH_MAX];
			struct stat buf;

			STRNCPY(pathname, th_get_pathname(tar), PATH_MAX);

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
						/* op == PM_TRANS_TYPE_UPGRADE */
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
				temp = strdup("/tmp/alpm_XXXXXX");
				mkstemp(temp);
				if(tar_extract_file(tar, temp)) {
					alpm_logaction("could not extract %s: %s", pathname, strerror(errno));
					errors++;
					FREE(md5_local);
					continue;
				}
				md5_pkg = MDFile(temp);
				/* append the new md5 hash to it's respective entry in info->backup
				 * (it will be the new orginal)
				 */
				for(lp = info->backup; lp; lp = lp->next) {
					char *fn;
					char *file = lp->data;

					if(!file) continue;
					if(!strcmp(file, pathname)) {
						/* 32 for the hash, 1 for the terminating NULL, and 1 for the tab delimiter */
						MALLOC(fn, strlen(file)+34);
						sprintf(fn, "%s\t%s", file, md5_pkg);
						FREE(file);
						lp->data = fn;
					}
				}

				_alpm_log(PM_LOG_DEBUG, " checking md5 hashes for %s", pathname);
				_alpm_log(PM_LOG_DEBUG, " current:  %s", md5_local);
				_alpm_log(PM_LOG_DEBUG, " new:      %s", md5_pkg);
				if(md5_orig) {
					_alpm_log(PM_LOG_DEBUG, " original: %s", md5_orig);
				}

				if(!pmo_upgrade) {
					/* PM_ADD */

					/* if a file already exists with a different md5 hash,
					 * then we rename it to a .pacorig extension and continue */
					if(strcmp(md5_local, md5_pkg)) {
						char newpath[PATH_MAX];
						snprintf(newpath, PATH_MAX, "%s.pacorig", expath);
						if(rename(expath, newpath)) {
							_alpm_log(PM_LOG_ERROR, "could not rename %s: %s", pathname, strerror(errno));
							alpm_logaction("error: could not rename %s: %s", expath, strerror(errno));
						}
						if(_alpm_copyfile(temp, expath)) {
							_alpm_log(PM_LOG_ERROR, "could not copy %s to %s: %s", temp, pathname, strerror(errno));
							alpm_logaction("error: could not copy %s to %s: %s", temp, expath, strerror(errno));
							errors++;
						} else {
							_alpm_log(PM_LOG_WARNING, "%s saved as %s.pacorig", pathname, pathname);
							alpm_logaction("warning: %s saved as %s", expath, newpath);
						}
					}
				} else if(md5_orig) {
					/* PM_UPGRADE */
					int installnew = 0;

					/* the fun part */
					if(!strcmp(md5_orig, md5_local)) {
						if(!strcmp(md5_local, md5_pkg)) {
							_alpm_log(PM_LOG_DEBUG, " action: installing new file");
							installnew = 1;
						} else {
							_alpm_log(PM_LOG_DEBUG, " action: installing new file");
							installnew = 1;
						}
					} else if(!strcmp(md5_orig, md5_pkg)) {
						_alpm_log(PM_LOG_DEBUG, " action: leaving existing file in place");
					} else if(!strcmp(md5_local, md5_pkg)) {
						_alpm_log(PM_LOG_DEBUG, " action: installing new file");
						installnew = 1;
					} else {
						char newpath[PATH_MAX];
						_alpm_log(PM_LOG_DEBUG, " action: saving current file and installing new one");
						installnew = 1;
						snprintf(newpath, PATH_MAX, "%s.pacsave", expath);
						if(rename(expath, newpath)) {
							_alpm_log(PM_LOG_ERROR, "could not rename %s: %s", pathname, strerror(errno));
							alpm_logaction("error: could not rename %s: %s", expath, strerror(errno));
						} else {
							_alpm_log(PM_LOG_WARNING, "%s saved as %s.pacsave", pathname, pathname);
							alpm_logaction("warning: %s saved as %s", expath, newpath);
						}
					}

					if(installnew) {
						/*_alpm_log(PM_LOG_FLOW2, "  %s", expath);*/
						if(_alpm_copyfile(temp, expath)) {
							_alpm_log(PM_LOG_ERROR, "could not copy %s to %s: %s", temp, pathname, strerror(errno));
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
					_alpm_log(PM_LOG_FLOW2, "extracting %s", pathname);
				} else {
					_alpm_log(PM_LOG_FLOW2, "%s is in NoUpgrade - skipping", pathname);
					strncat(expath, ".pacnew", PATH_MAX);
					_alpm_log(PM_LOG_WARNING, "extracting %s as %s.pacnew", pathname, pathname);
					alpm_logaction("warning: extracting %s%s as %s", handle->root, pathname, expath);
					/*tar_skip_regfile(tar);*/
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
					char *file = lp->data;

					if(!file) continue;
					if(!strcmp(file, pathname)) {
						snprintf(path, PATH_MAX, "%s%s", handle->root, file);
						md5 = MDFile(path);
						/* 32 for the hash, 1 for the terminating NULL, and 1 for the tab delimiter */
						MALLOC(fn, strlen(file)+34);
						sprintf(fn, "%s\t%s", file, md5);
						FREE(md5);
						FREE(file);
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
		if(info->scriptlet) {
			snprintf(pm_install, PATH_MAX, "%s%s/%s/%s-%s/install", handle->root, handle->dbpath, db->treename, info->name, info->version);
			if(pmo_upgrade) {
				_alpm_runscriptlet(handle->root, pm_install, "post_upgrade", info->version, oldpkg ? oldpkg->version : NULL);
			} else {
				_alpm_runscriptlet(handle->root, pm_install, "post_install", info->version, NULL);
			}
		}

		if(pmo_upgrade) {
			EVENT(trans, PM_TRANS_EVT_UPGRADE_DONE, info, NULL);
			alpm_logaction("upgraded %s (%s -> %s)", info->name,
				oldpkg ? oldpkg->version : NULL, info->version);
		} else {
			EVENT(trans, PM_TRANS_EVT_ADD_DONE, info, NULL);
			alpm_logaction("installed %s (%s)", info->name, info->version);
		}

		FREEPKG(oldpkg);
	}

	/* run ldconfig if it exists */
	_alpm_log(PM_LOG_FLOW1, "running \"ldconfig -r %s\"", handle->root);
	_alpm_ldconfig(handle->root);

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
