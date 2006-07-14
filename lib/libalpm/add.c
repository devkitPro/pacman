/*
 *  add.c
 * 
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
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
#include <limits.h>
#include <zlib.h>
#include <libintl.h>
#include <libtar.h>
/* pacman */
#include "util.h"
#include "error.h"
#include "list.h"
#include "cache.h"
#include "versioncmp.h"
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

static int add_faketarget(pmtrans_t *trans, char *name)
{
	char *ptr, *p;
	char *str = NULL;
	pmpkg_t *dummy = NULL;

	dummy = _alpm_pkg_new(NULL, NULL);
	if(dummy == NULL) {
		RET_ERR(PM_ERR_MEMORY, -1);
	}

	/* Format: field1=value1|field2=value2|...
	 * Valid fields are "name", "version" and "depend"
	 */
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
			dummy->depends = _alpm_list_add(dummy->depends, strdup(q+1));
		} else {
			_alpm_log(PM_LOG_ERROR, _("could not parse token %s"), p);
		}
	}
	FREE(str);
	if(dummy->name[0] == 0 || dummy->version[0] == 0) {
		FREEPKG(dummy);
		RET_ERR(PM_ERR_PKG_INVALID_NAME, -1);
	}

	/* add the package to the transaction */
	trans->packages = _alpm_list_add(trans->packages, dummy);

	return(0);
}

int _alpm_add_loadtarget(pmtrans_t *trans, pmdb_t *db, char *name)
{
	pmpkg_t *info = NULL;
	pmpkg_t *dummy;
	char pkgname[PKG_NAME_LEN], pkgver[PKG_VERSION_LEN];
	PMList *i;
	struct stat buf;

	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(name != NULL && strlen(name) != 0, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	/* Check if we need to add a fake target to the transaction. */
	if(strchr(name, '|')) {
		return(add_faketarget(trans, name));
	}

	_alpm_log(PM_LOG_FLOW2, _("loading target '%s'"), name);

	if(stat(name, &buf)) {
		pm_errno = PM_ERR_NOT_A_FILE;
		goto error;
	}

	if(_alpm_pkg_splitname(name, pkgname, pkgver) == -1) {
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
		if(_alpm_db_get_pkgfromcache(db, pkgname)) {
			pm_errno = PM_ERR_PKG_INSTALLED;
			goto error;
		}
	} else {
		if(trans->flags & PM_TRANS_FLAG_FRESHEN) {
			/* only upgrade/install this package if it is already installed and at a lesser version */
			dummy = _alpm_db_get_pkgfromcache(db, pkgname);
			if(dummy == NULL || _alpm_versioncmp(dummy->version, pkgver) >= 0) {
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
			if(_alpm_versioncmp(pkg->version, pkgver) < 0) {
				pmpkg_t *newpkg;
				_alpm_log(PM_LOG_WARNING, _("replacing older version %s-%s by %s in target list"),
				          pkg->name, pkg->version, pkgver);
				if((newpkg = _alpm_pkg_load(name)) == NULL) {
					/* pm_errno is already set by pkg_load() */
					goto error;
				}
				FREEPKG(i->data);
				i->data = newpkg;
			} else {
				_alpm_log(PM_LOG_WARNING, _("newer version %s-%s is in the target list -- skipping"),
				          pkg->name, pkg->version, pkgver);
			}
			return(0);
		}
	}

	_alpm_log(PM_LOG_FLOW2, _("reading '%s' metadata"), pkgname);
	info = _alpm_pkg_load(name);
	if(info == NULL) {
		/* pm_errno is already set by pkg_load() */
		goto error;
	}
	/* check to verify we're not getting fooled by a corrupted package */
	if(strcmp(pkgname, info->name) != 0 || strcmp(pkgver, info->version) != 0) {
		pm_errno = PM_ERR_PKG_INVALID;
		goto error;
	}

	if(trans->flags & PM_TRANS_FLAG_ALLDEPS) {
		info->reason = PM_PKG_REASON_DEPEND;
	}

	/* add the package to the transaction */
	trans->packages = _alpm_list_add(trans->packages, info);

	return(0);

error:
	FREEPKG(info);
	return(-1);
}

int _alpm_add_prepare(pmtrans_t *trans, pmdb_t *db, PMList **data)
{
	PMList *lp;

	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));

	/* Check dependencies
	 */
	if(!(trans->flags & PM_TRANS_FLAG_NODEPS)) {
		EVENT(trans, PM_TRANS_EVT_CHECKDEPS_START, NULL, NULL);

		/* look for unsatisfied dependencies */
		_alpm_log(PM_LOG_FLOW1,_( "looking for unsatisfied dependencies"));
		lp = _alpm_checkdeps(db, trans->type, trans->packages);
		if(lp != NULL) {
			if(data) {
				*data = lp;
			} else {
				FREELIST(lp);
			}
			RET_ERR(PM_ERR_UNSATISFIED_DEPS, -1);
		}

		/* no unsatisfied deps, so look for conflicts */
		_alpm_log(PM_LOG_FLOW1, _("looking for conflicts"));
		lp = _alpm_checkconflicts(db, trans->packages);
		if(lp != NULL) {
			if(data) {
				*data = lp;
			} else {
				FREELIST(lp);
			}
			RET_ERR(PM_ERR_CONFLICTING_DEPS, -1);
		}

		/* re-order w.r.t. dependencies */
		_alpm_log(PM_LOG_FLOW1, _("sorting by dependencies"));
		lp = _alpm_sortbydeps(trans->packages, PM_TRANS_TYPE_ADD);
		/* free the old alltargs */
		FREELISTPTR(trans->packages);
		trans->packages = lp;

		EVENT(trans, PM_TRANS_EVT_CHECKDEPS_DONE, NULL, NULL);
	}

	/* Check for file conflicts
	 */
	if(!(trans->flags & PM_TRANS_FLAG_FORCE)) {
		PMList *skiplist = NULL;

		EVENT(trans, PM_TRANS_EVT_FILECONFLICTS_START, NULL, NULL);

		_alpm_log(PM_LOG_FLOW1, _("looking for file conflicts"));
		lp = _alpm_db_find_conflicts(db, trans->packages, handle->root, &skiplist);
		if(lp != NULL) {
			if(data) {
				*data = lp;
			} else {
				FREELIST(lp);
			}
			FREELIST(skiplist);
			RET_ERR(PM_ERR_FILE_CONFLICTS, -1);
		}

		/* copy the file skiplist into the transaction */
		trans->skiplist = skiplist;

		EVENT(trans, PM_TRANS_EVT_FILECONFLICTS_DONE, NULL, NULL);
	}

	return(0);
}

int _alpm_add_commit(pmtrans_t *trans, pmdb_t *db)
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

		if(handle->trans->state == STATE_INTERRUPTED) {
			break;
		}

		pmo_upgrade = (trans->type == PM_TRANS_TYPE_UPGRADE) ? 1 : 0;

		/* see if this is an upgrade.  if so, remove the old package first */
		if(pmo_upgrade) {
			pmpkg_t *local = _alpm_db_get_pkgfromcache(db, info->name);
			if(local) {
				EVENT(trans, PM_TRANS_EVT_UPGRADE_START, info, NULL);
				_alpm_log(PM_LOG_FLOW1, _("upgrading package %s-%s"), info->name, info->version);

				/* we'll need to save some record for backup checks later */
				oldpkg = _alpm_pkg_new(local->name, local->version);
				if(oldpkg) {
					if(!(local->infolevel & INFRQ_FILES)) {
						_alpm_log(PM_LOG_DEBUG, _("loading FILES info for '%s'"), local->name);
						_alpm_db_read(db, INFRQ_FILES, local);
					}
					oldpkg->backup = _alpm_list_strdup(local->backup);
				}

				/* copy over the install reason */
				if(!(local->infolevel & INFRQ_DESC)) {
					_alpm_log(PM_LOG_DEBUG, _("loading DESC info for '%s'"), local->name);
					_alpm_db_read(db, INFRQ_DESC, local);
				}
				info->reason = local->reason;

				/* pre_upgrade scriptlet */
				if(info->scriptlet && !(trans->flags & PM_TRANS_FLAG_NOSCRIPTLET)) {
					_alpm_runscriptlet(handle->root, info->data, "pre_upgrade", info->version, oldpkg ? oldpkg->version : NULL);
				}

				if(oldpkg) {
					pmtrans_t *tr;
					_alpm_log(PM_LOG_FLOW1, _("removing old package first (%s-%s)"), oldpkg->name, oldpkg->version);
					tr = _alpm_trans_new();
					if(tr == NULL) {
						RET_ERR(PM_ERR_TRANS_ABORT, -1);
					}
					if(_alpm_trans_init(tr, PM_TRANS_TYPE_UPGRADE, trans->flags, NULL, NULL) == -1) {
						FREETRANS(tr);
						RET_ERR(PM_ERR_TRANS_ABORT, -1);
					}
					if(_alpm_remove_loadtarget(tr, db, info->name) == -1) {
						FREETRANS(tr);
						RET_ERR(PM_ERR_TRANS_ABORT, -1);
					}
					/* copy the skiplist over */
					tr->skiplist = _alpm_list_strdup(trans->skiplist);
					if(_alpm_remove_commit(tr, db) == -1) {
						FREETRANS(tr);
						RET_ERR(PM_ERR_TRANS_ABORT, -1);
					}
					FREETRANS(tr);
				}
			} else {
				/* no previous package version is installed, so this is actually
				 * just an install.  */
				pmo_upgrade = 0;
			}
		}
		if(!pmo_upgrade) {
			EVENT(trans, PM_TRANS_EVT_ADD_START, info, NULL);
			_alpm_log(PM_LOG_FLOW1, _("adding package %s-%s"), info->name, info->version);

			/* pre_install scriptlet */
			if(info->scriptlet && !(trans->flags & PM_TRANS_FLAG_NOSCRIPTLET)) {
				_alpm_runscriptlet(handle->root, info->data, "pre_install", info->version, NULL);
			}
		} else {
			_alpm_log(PM_LOG_FLOW1, _("adding new package %s-%s"), info->name, info->version);
		}

		if(!(trans->flags & PM_TRANS_FLAG_DBONLY)) {
			_alpm_log(PM_LOG_FLOW1, _("extracting files"));

			/* Extract the .tar.gz package */
			if(tar_open(&tar, info->data, &gztype, O_RDONLY, 0, TAR_GNU) == -1) {
				RET_ERR(PM_ERR_PKG_OPEN, -1);
			}

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

				/* if a file is in NoExtract then we never extract it.
				 *
				 * eg, /home/httpd/html/index.html may be removed so index.php
				 * could be used.
				 */
				if(_alpm_list_is_strin(pathname, handle->noextract)) {
					alpm_logaction(_("notice: %s is in NoExtract -- skipping extraction"), pathname);
					tar_skip_regfile(tar);
					continue;
				}

				if(!stat(expath, &buf) && !S_ISDIR(buf.st_mode)) {
					/* file already exists */
					if(_alpm_list_is_strin(pathname, handle->noupgrade)) {
						notouch = 1;
					} else {
						if(!pmo_upgrade || oldpkg == NULL) {
							nb = _alpm_list_is_strin(pathname, info->backup);
						} else {
							/* op == PM_TRANS_TYPE_UPGRADE */
							md5_orig = _alpm_needbackup(pathname, oldpkg->backup);
							if(md5_orig) {
								nb = 1;
							}
						}
					}
				}

				if(nb) {
					char *temp;
					char *md5_local, *md5_pkg;
					int fd;

					/* extract the package's version to a temporary file and md5 it */
					temp = strdup("/tmp/alpm_XXXXXX");
					fd = mkstemp(temp);
					if(tar_extract_file(tar, temp)) {
						alpm_logaction(_("could not extract %s (%s)"), pathname, strerror(errno));
						errors++;
						unlink(temp);
						FREE(temp);
						FREE(md5_orig);
						close(fd);
						continue;
					}
					md5_local = MDFile(expath);
					md5_pkg = MDFile(temp);
					/* append the new md5 hash to it's respective entry in info->backup
					 * (it will be the new orginal)
					 */
					for(lp = info->backup; lp; lp = lp->next) {
						char *file = lp->data;
						if(!file) {
							continue;
						}
						if(!strcmp(file, pathname)) {
							char *fn;
							/* 32 for the hash, 1 for the terminating NULL, and 1 for the tab delimiter */
							fn = (char *)malloc(strlen(file)+34);
							if(fn == NULL) {
								RET_ERR(PM_ERR_MEMORY, -1);
							}
							sprintf(fn, "%s\t%s", file, md5_pkg);
							FREE(file);
							lp->data = fn;
						}
					}

					_alpm_log(PM_LOG_DEBUG, _("checking md5 hashes for %s"), pathname);
					_alpm_log(PM_LOG_DEBUG, _("current:  %s"), md5_local);
					_alpm_log(PM_LOG_DEBUG, _("new:      %s"), md5_pkg);
					if(md5_orig) {
						_alpm_log(PM_LOG_DEBUG, _("original: %s"), md5_orig);
					}

					if(!pmo_upgrade) {
						/* PM_ADD */

						/* if a file already exists with a different md5 hash,
						 * then we rename it to a .pacorig extension and continue */
						if(strcmp(md5_local, md5_pkg)) {
							char newpath[PATH_MAX];
							snprintf(newpath, PATH_MAX, "%s.pacorig", expath);
							if(rename(expath, newpath)) {
								_alpm_log(PM_LOG_ERROR, _("could not rename %s (%s)"), pathname, strerror(errno));
								alpm_logaction(_("error: could not rename %s (%s)"), expath, strerror(errno));
							}
							if(_alpm_copyfile(temp, expath)) {
								_alpm_log(PM_LOG_ERROR, _("could not copy %s to %s (%s)"), temp, pathname, strerror(errno));
								alpm_logaction(_("error: could not copy %s to %s (%s)"), temp, expath, strerror(errno));
								errors++;
							} else {
								_alpm_log(PM_LOG_WARNING, _("%s saved as %s.pacorig"), pathname, pathname);
								alpm_logaction(_("warning: %s saved as %s"), expath, newpath);
							}
						}
					} else if(md5_orig) {
						/* PM_UPGRADE */
						int installnew = 0;

						/* the fun part */
						if(!strcmp(md5_orig, md5_local)) {
							if(!strcmp(md5_local, md5_pkg)) {
								_alpm_log(PM_LOG_DEBUG, _("action: installing new file"));
								installnew = 1;
							} else {
								_alpm_log(PM_LOG_DEBUG, _("action: installing new file"));
								installnew = 1;
							}
						} else if(!strcmp(md5_orig, md5_pkg)) {
							_alpm_log(PM_LOG_DEBUG, _("action: leaving existing file in place"));
						} else if(!strcmp(md5_local, md5_pkg)) {
							_alpm_log(PM_LOG_DEBUG, _("action: installing new file"));
							installnew = 1;
						} else {
							_alpm_log(PM_LOG_DEBUG, _("action: leaving file in place, installing new one as .pacnew"));
							strncat(expath, ".pacnew", PATH_MAX);
							installnew = 1;
							_alpm_log(PM_LOG_WARNING, _("extracting %s as %s.pacnew"), pathname, pathname);
							alpm_logaction(_("warning: extracting %s%s as %s"), handle->root, pathname, expath);
						}

						if(installnew) {
							_alpm_log(PM_LOG_FLOW2, _("extracting %s"), pathname);
							if(_alpm_copyfile(temp, expath)) {
								_alpm_log(PM_LOG_ERROR, _("could not copy %s to %s (%s)"), temp, pathname, strerror(errno));
								errors++;
							}
						}
					}

					FREE(md5_local);
					FREE(md5_pkg);
					FREE(md5_orig);
					unlink(temp);
					FREE(temp);
					close(fd);
				} else {
					if(!notouch) {
						_alpm_log(PM_LOG_FLOW2, _("extracting %s"), pathname);
					} else {
						_alpm_log(PM_LOG_FLOW2, _("%s is in NoUpgrade -- skipping"), pathname);
						strncat(expath, ".pacnew", PATH_MAX);
						_alpm_log(PM_LOG_WARNING, _("extracting %s as %s.pacnew"), pathname, pathname);
						alpm_logaction(_("warning: extracting %s%s as %s"), handle->root, pathname, expath);
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
						_alpm_log(PM_LOG_ERROR, _("could not extract %s (%s)"), pathname, strerror(errno));
						alpm_logaction(_("error: could not extract %s (%s)"), pathname, strerror(errno));
						errors++;
					}
					/* calculate an md5 hash if this is in info->backup */
					for(lp = info->backup; lp; lp = lp->next) {
						char *fn, *md5;
						char path[PATH_MAX];
						char *file = lp->data;

						if(!file) continue;
						if(!strcmp(file, pathname)) {
							_alpm_log(PM_LOG_DEBUG, _("appending backup entry"));
							snprintf(path, PATH_MAX, "%s%s", handle->root, file);
							md5 = MDFile(path);
							/* 32 for the hash, 1 for the terminating NULL, and 1 for the tab delimiter */
							fn = (char *)malloc(strlen(file)+34);
							if(fn == NULL) {
								RET_ERR(PM_ERR_MEMORY, -1);
							}
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
				_alpm_log(PM_LOG_ERROR, _("errors occurred while %s %s"),
					(pmo_upgrade ? _("upgrading") : _("installing")), info->name);
				alpm_logaction(_("errors occurred while %s %s"),
					(pmo_upgrade ? _("upgrading") : _("installing")), info->name);
			}
		}

		/* Add the package to the database */
		t = time(NULL);

		/* Update the requiredby field by scanning the whole database 
		 * looking for packages depending on the package to add */
		for(lp = _alpm_db_get_pkgcache(db); lp; lp = lp->next) {
			pmpkg_t *tmpp = lp->data;
			PMList *tmppm = NULL;
			if(tmpp == NULL) {
				continue;
			}
			for(tmppm = tmpp->depends; tmppm; tmppm = tmppm->next) {
				pmdepend_t depend;
				if(_alpm_splitdep(tmppm->data, &depend)) {
					continue;
				}
				if(tmppm->data && !strcmp(depend.name, info->name)) {
					_alpm_log(PM_LOG_DEBUG, _("adding '%s' in requiredby field for '%s'"), tmpp->name, info->name);
					info->requiredby = _alpm_list_add(info->requiredby, strdup(tmpp->name));
				}
			}
		}

		/* make an install date (in UTC) */
		STRNCPY(info->installdate, asctime(gmtime(&t)), sizeof(info->installdate));
		/* remove the extra line feed appended by asctime() */
		info->installdate[strlen(info->installdate)-1] = 0;

		_alpm_log(PM_LOG_FLOW1, _("updating database"));
		_alpm_log(PM_LOG_FLOW2, _("adding database entry '%s'"), info->name);
		if(_alpm_db_write(db, info, INFRQ_ALL)) {
			_alpm_log(PM_LOG_ERROR, _("could not update database entry %s-%s"),
			          info->name, info->version);
			alpm_logaction(NULL, _("error updating database for %s-%s!"), info->name, info->version);
			RET_ERR(PM_ERR_DB_WRITE, -1);
		}
		if(_alpm_db_add_pkgincache(db, info) == -1) {
			_alpm_log(PM_LOG_ERROR, _("could not add entry '%s' in cache"), info->name);
		}

		/* update dependency packages' REQUIREDBY fields */
		if(info->depends) {
			_alpm_log(PM_LOG_FLOW2, _("updating dependency packages 'requiredby' fields"));
		}
		for(lp = info->depends; lp; lp = lp->next) {
			pmpkg_t *depinfo;
			pmdepend_t depend;
			if(_alpm_splitdep(lp->data, &depend)) {
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
			_alpm_log(PM_LOG_DEBUG, _("adding '%s' in requiredby field for '%s'"), info->name, depinfo->name);
			depinfo->requiredby = _alpm_list_add(depinfo->requiredby, strdup(info->name));
			if(_alpm_db_write(db, depinfo, INFRQ_DEPENDS)) {
				_alpm_log(PM_LOG_ERROR, _("could not update 'requiredby' database entry %s-%s"),
				          depinfo->name, depinfo->version);
			}
		}

		/* run the post-install script if it exists  */
		if(info->scriptlet && !(trans->flags & PM_TRANS_FLAG_NOSCRIPTLET)) {
			snprintf(pm_install, PATH_MAX, "%s%s/%s/%s-%s/install", handle->root, handle->dbpath, db->treename, info->name, info->version);
			if(pmo_upgrade) {
				_alpm_runscriptlet(handle->root, pm_install, "post_upgrade", info->version, oldpkg ? oldpkg->version : NULL);
			} else {
				_alpm_runscriptlet(handle->root, pm_install, "post_install", info->version, NULL);
			}
		}

		EVENT(trans, (pmo_upgrade) ? PM_TRANS_EVT_UPGRADE_DONE : PM_TRANS_EVT_ADD_DONE, info, oldpkg);

		FREEPKG(oldpkg);
	}

	/* run ldconfig if it exists */
	if(handle->trans->state != STATE_INTERRUPTED) {
		_alpm_log(PM_LOG_FLOW1, _("running \"ldconfig -r %s\""), handle->root);
		_alpm_ldconfig(handle->root);
	}

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
