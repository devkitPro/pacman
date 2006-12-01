/*
 *  add.c
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
#include <libintl.h>
/* pacman */
#include "list.h"
#include "trans.h"
#include "util.h"
#include "error.h"
#include "cache.h"
#include "versioncmp.h"
#include "md5.h"
#include "sha1.h"
#include "log.h"
#include "backup.h"
#include "package.h"
#include "db.h"
#include "provide.h"
#include "conflict.h"
#include "deps.h"
#include "add.h"
#include "remove.h"
#include "handle.h"

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
	pmlist_t *i;
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

	if(_alpm_pkg_splitname(name, pkgname, pkgver, 1) == -1) {
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


static int name_cmp(const void *p1, const void *p2)
{
	return(strcmp(((pmpkg_t *)p1)->name, (const char *)p2));
}

int _alpm_add_prepare(pmtrans_t *trans, pmdb_t *db, pmlist_t **data)
{
	pmlist_t *lp = NULL, *i = NULL;
	pmlist_t *rmlist = NULL;
	char rm_fname[PATH_MAX];
	pmpkg_t *info = NULL;

	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));

	/* Check dependencies
	 */
	if(!(trans->flags & PM_TRANS_FLAG_NODEPS)) {
		EVENT(trans, PM_TRANS_EVT_CHECKDEPS_START, NULL, NULL);

		/* look for unsatisfied dependencies */
		_alpm_log(PM_LOG_FLOW1, _("looking for unsatisfied dependencies"));
		lp = _alpm_checkdeps(trans, db, trans->type, trans->packages);
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
		for(i = lp; i; i = i->next) {
			int skip_this = 0;
			pmdepmissing_t *miss = i->data;

			/* Attempt to resolve conflicts */
			QUESTION(trans, PM_TRANS_CONV_CONFLICT_PKG, miss->target, miss->depend.name, NULL, &skip_this);
			if(skip_this) {
				pmpkg_t *pkg = NULL;
				lp = _alpm_list_remove(lp, miss->depend.name, name_cmp, (void **)&pkg);
				FREEPKG(pkg);
			}
		}
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

	/* Cleaning up
	 */
	EVENT(trans, PM_TRANS_EVT_CLEANUP_START, NULL, NULL);
	_alpm_log(PM_LOG_FLOW1, _("cleaning up"));
	for (lp=trans->packages; lp!=NULL; lp=lp->next) {
		info=(pmpkg_t *)lp->data;
		for (rmlist=info->removes; rmlist!=NULL; rmlist=rmlist->next) {
			snprintf(rm_fname, PATH_MAX, "%s%s", handle->root, (char *)rmlist->data);
			remove(rm_fname);
		}
	}
	EVENT(trans, PM_TRANS_EVT_CLEANUP_DONE, NULL, NULL);

	/* Check for file conflicts
	 */
	if(!(trans->flags & PM_TRANS_FLAG_FORCE)) {
		pmlist_t *skiplist = NULL;

		EVENT(trans, PM_TRANS_EVT_FILECONFLICTS_START, NULL, NULL);

		_alpm_log(PM_LOG_FLOW1, _("looking for file conflicts"));
		lp = _alpm_db_find_conflicts(db, trans, handle->root, &skiplist);
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

#ifndef __sun__
	if(_alpm_check_freespace(trans, data) == -1) {
			/* pm_errno is set by check_freespace */
			return(-1);
	}
#endif

	return(0);
}

int _alpm_add_commit(pmtrans_t *trans, pmdb_t *db)
{
	int i, ret = 0, errors = 0, needdisp = 0;
	double percent = 0.0;
	register struct archive *archive;
	struct archive_entry *entry;
	char expath[PATH_MAX], cwd[PATH_MAX] = "", *what;
	unsigned char cb_state;
	time_t t;
	pmlist_t *targ, *lp;

	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));

	if(trans->packages == NULL) {
		return(0);
	}

	for(targ = trans->packages; targ; targ = targ->next) {
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
				cb_state = PM_TRANS_PROGRESS_UPGRADE_START;
				_alpm_log(PM_LOG_FLOW1, _("upgrading package %s-%s"), info->name, info->version);
				if((what = (char *)malloc(strlen(info->name)+1)) == NULL) {
					RET_ERR(PM_ERR_MEMORY, -1);
				}
				STRNCPY(what, info->name, strlen(info->name)+1);

				/* we'll need to save some record for backup checks later */
				oldpkg = _alpm_pkg_new(local->name, local->version);
				if(oldpkg) {
					if(!(local->infolevel & INFRQ_FILES)) {
						_alpm_log(PM_LOG_DEBUG, _("loading FILES info for '%s'"), local->name);
						_alpm_db_read(db, INFRQ_FILES, local);
					}
					oldpkg->backup = _alpm_list_strdup(local->backup);
					strncpy(oldpkg->name, local->name, PKG_NAME_LEN);
					strncpy(oldpkg->version, local->version, PKG_VERSION_LEN);
				}

				/* copy over the install reason */
				if(!(local->infolevel & INFRQ_DESC)) {
					_alpm_db_read(db, INFRQ_DESC, local);
				}
				info->reason = local->reason;

				/* pre_upgrade scriptlet */
				if(info->scriptlet && !(trans->flags & PM_TRANS_FLAG_NOSCRIPTLET)) {
					_alpm_runscriptlet(handle->root, info->data, "pre_upgrade", info->version, oldpkg ? oldpkg->version : NULL,
						trans);
				}

				if(oldpkg) {
					pmtrans_t *tr;
					_alpm_log(PM_LOG_FLOW1, _("removing old package first (%s-%s)"), oldpkg->name, oldpkg->version);
					tr = _alpm_trans_new();
					if(tr == NULL) {
						RET_ERR(PM_ERR_TRANS_ABORT, -1);
					}
					if(_alpm_trans_init(tr, PM_TRANS_TYPE_UPGRADE, trans->flags, NULL, NULL, NULL) == -1) {
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
			cb_state = PM_TRANS_PROGRESS_ADD_START;
			_alpm_log(PM_LOG_FLOW1, _("adding package %s-%s"), info->name, info->version);
			if((what = (char *)malloc(strlen(info->name)+1)) == NULL) {
				RET_ERR(PM_ERR_MEMORY, -1);
			}
			STRNCPY(what, info->name, strlen(info->name)+1);

			/* pre_install scriptlet */
			if(info->scriptlet && !(trans->flags & PM_TRANS_FLAG_NOSCRIPTLET)) {
				_alpm_runscriptlet(handle->root, info->data, "pre_install", info->version, NULL, trans);
			}
		} else {
			_alpm_log(PM_LOG_FLOW1, _("adding new package %s-%s"), info->name, info->version);
		}

		if(!(trans->flags & PM_TRANS_FLAG_DBONLY)) {
			_alpm_log(PM_LOG_FLOW1, _("extracting files"));

			/* Extract the package */
			if ((archive = archive_read_new ()) == NULL)
				RET_ERR(PM_ERR_LIBARCHIVE_ERROR, -1);

			archive_read_support_compression_all (archive);
			archive_read_support_format_all (archive);

			if (archive_read_open_file (archive, info->data, ARCHIVE_DEFAULT_BYTES_PER_BLOCK) != ARCHIVE_OK) {
				RET_ERR(PM_ERR_PKG_OPEN, -1);
			}

			/* save the cwd so we can restore it later */
			if(getcwd(cwd, PATH_MAX) == NULL) {
				_alpm_log(PM_LOG_ERROR, _("could not get current working directory"));
				/* in case of error, cwd content is undefined: so we set it to something */
				cwd[0] = 0;
			}

			/* libarchive requires this for extracting hard links */
			chdir(handle->root);

			for(i = 0; archive_read_next_header (archive, &entry) == ARCHIVE_OK; i++) {
				int nb = 0;
				int notouch = 0;
				char *md5_orig = NULL;
				char *sha1_orig = NULL;
				char pathname[PATH_MAX];
				struct stat buf;

				STRNCPY(pathname, archive_entry_pathname (entry), PATH_MAX);

				if (info->size != 0)
		    			percent = (double)archive_position_uncompressed(archive) / info->size;
				if (needdisp == 0) {
					PROGRESS(trans, cb_state, what, (int)(percent * 100), _alpm_list_count(trans->packages), (_alpm_list_count(trans->packages) - _alpm_list_count(targ) +1));
				}

				if(!strcmp(pathname, ".PKGINFO") || !strcmp(pathname, ".FILELIST")) {
					archive_read_data_skip (archive);
					continue;
				}

				/*if(!strcmp(pathname, "._install") || !strcmp(pathname, ".INSTALL")) {
				*	 the install script goes inside the db 
				*	snprintf(expath, PATH_MAX, "%s/%s-%s/install", db->path, info->name, info->version); */
				if(!strcmp(pathname, "._install") || !strcmp(pathname, ".INSTALL") ||
					!strcmp(pathname, ".CHANGELOG")) {
					if(!strcmp(pathname, ".CHANGELOG")) {
						/* the changelog goes inside the db */
						snprintf(expath, PATH_MAX, "%s/%s-%s/changelog", db->path,
							info->name, info->version);
					} else {
						/* the install script goes inside the db */
						snprintf(expath, PATH_MAX, "%s/%s-%s/install", db->path,
							info->name, info->version);
					}
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
					archive_read_data_skip (archive);
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
							sha1_orig = _alpm_needbackup(pathname, oldpkg->backup);
							if(md5_orig || sha1_orig) {
								nb = 1;
							}
						}
					}
				}

				if(nb) {
					char *temp;
					char *md5_local, *md5_pkg;
					char *sha1_local, *sha1_pkg;
					int fd;

					/* extract the package's version to a temporary file and md5 it */
					temp = strdup("/tmp/alpm_XXXXXX");
					fd = mkstemp(temp);
					
					archive_entry_set_pathname (entry, temp);

					if(archive_read_extract (archive, entry, ARCHIVE_EXTRACT_FLAGS) != ARCHIVE_OK) {
						alpm_logaction(_("could not extract %s (%s)"), pathname, strerror(errno));
						errors++;
						unlink(temp);
						FREE(temp);
						FREE(md5_orig);
						FREE(sha1_orig);
						close(fd);
						continue;
					}
					md5_local = _alpm_MDFile(expath);
					md5_pkg = _alpm_MDFile(temp);
					sha1_local = _alpm_SHAFile(expath);
					sha1_pkg = _alpm_SHAFile(temp);
					/* append the new md5 or sha1 hash to it's respective entry in info->backup
					 * (it will be the new orginal)
					 */
					for(lp = info->backup; lp; lp = lp->next) {
						char *fn;
						char *file = lp->data;

						if(!file) continue;
						if(!strcmp(file, pathname)) {
						    if(info->sha1sum != NULL && info->sha1sum != '\0') {
							/* 32 for the hash, 1 for the terminating NULL, and 1 for the tab delimiter */
							if((fn = (char *)malloc(strlen(file)+34)) == NULL) {
								RET_ERR(PM_ERR_MEMORY, -1);
							}
							sprintf(fn, "%s\t%s", file, md5_pkg);
							FREE(file);
							lp->data = fn;
						    } else {
							/* 41 for the hash, 1 for the terminating NULL, and 1 for the tab delimiter */
							if((fn = (char *)malloc(strlen(file)+43)) == NULL) {
								RET_ERR(PM_ERR_MEMORY, -1);
							}
							sprintf(fn, "%s\t%s", file, sha1_pkg);
							FREE(file);
							lp->data = fn;
						    }
						}
					}

					if (info->sha1sum != NULL && info->sha1sum != '\0') {
					_alpm_log(PM_LOG_DEBUG, _("checking md5 hashes for %s"), pathname);
					_alpm_log(PM_LOG_DEBUG, _("current:  %s"), md5_local);
					_alpm_log(PM_LOG_DEBUG, _("new:      %s"), md5_pkg);
					if(md5_orig) {
						_alpm_log(PM_LOG_DEBUG, _("original: %s"), md5_orig);
					}
					} else {
                                        _alpm_log(PM_LOG_DEBUG, _("checking sha1 hashes for %s"), pathname);
					_alpm_log(PM_LOG_DEBUG, _("current:  %s"), sha1_local);
                                        _alpm_log(PM_LOG_DEBUG, _("new:      %s"), sha1_pkg);
                                        if(sha1_orig) {
                                        _alpm_log(PM_LOG_DEBUG, _("original: %s"), sha1_orig);
					}
					}

					if(!pmo_upgrade) {
						/* PM_ADD */

						/* if a file already exists with a different md5 or sha1 hash,
						 * then we rename it to a .pacorig extension and continue */
						if(strcmp(md5_local, md5_pkg) || strcmp(sha1_local, sha1_pkg)) {
							char newpath[PATH_MAX];
							snprintf(newpath, PATH_MAX, "%s.pacorig", expath);
							if(rename(expath, newpath)) {
								archive_entry_set_pathname (entry, expath);
								_alpm_log(PM_LOG_ERROR, _("could not rename %s (%s)"), pathname, strerror(errno));
								alpm_logaction(_("error: could not rename %s (%s)"), expath, strerror(errno));
							}
							if(_alpm_copyfile(temp, expath)) {
								archive_entry_set_pathname (entry, expath);
								_alpm_log(PM_LOG_ERROR, _("could not copy %s to %s (%s)"), temp, pathname, strerror(errno));
								alpm_logaction(_("error: could not copy %s to %s (%s)"), temp, expath, strerror(errno));
								errors++;
							} else {
								archive_entry_set_pathname (entry, expath);
								_alpm_log(PM_LOG_WARNING, _("%s saved as %s.pacorig"), pathname, pathname);
								alpm_logaction(_("warning: %s saved as %s"), expath, newpath);
							}
						}
					} else if(md5_orig || sha1_orig) {
						/* PM_UPGRADE */
						int installnew = 0;

						/* the fun part */
						if(!strcmp(md5_orig, md5_local)|| !strcmp(sha1_orig, sha1_local)) {
							if(!strcmp(md5_local, md5_pkg) || !strcmp(sha1_local, sha1_pkg)) {
								_alpm_log(PM_LOG_DEBUG, _("action: installing new file"));
								installnew = 1;
							} else {
								_alpm_log(PM_LOG_DEBUG, _("action: installing new file"));
								installnew = 1;
							}
						} else if(!strcmp(md5_orig, md5_pkg) || !strcmp(sha1_orig, sha1_pkg)) {
							_alpm_log(PM_LOG_DEBUG, _("action: leaving existing file in place"));
						} else if(!strcmp(md5_local, md5_pkg) || !strcmp(sha1_local, sha1_pkg)) {
							_alpm_log(PM_LOG_DEBUG, _("action: installing new file"));
							installnew = 1;
						} else {
							char newpath[PATH_MAX];
							_alpm_log(PM_LOG_DEBUG, _("action: keeping current file and installing new one with .pacnew ending"));
							installnew = 0;
							snprintf(newpath, PATH_MAX, "%s.pacnew", expath);
							if(_alpm_copyfile(temp, newpath)) {
								_alpm_log(PM_LOG_ERROR, _("could not install %s as %s: %s"), expath, newpath, strerror(errno));
								alpm_logaction(_("error: could not install %s as %s: %s"), expath, newpath, strerror(errno));
							} else {
								_alpm_log(PM_LOG_WARNING, _("%s installed as %s"), expath, newpath);
								alpm_logaction(_("warning: %s installed as %s"), expath, newpath);
							}
						}

						if(installnew) {
							_alpm_log(PM_LOG_FLOW2, _("extracting %s"), pathname);
							if(_alpm_copyfile(temp, expath)) {
								_alpm_log(PM_LOG_ERROR, _("could not copy %s to %s (%s)"), temp, pathname, strerror(errno));
								errors++;
							}
						    archive_entry_set_pathname (entry, expath);
						}
					}

					FREE(md5_local);
					FREE(md5_pkg);
					FREE(md5_orig);
					FREE(sha1_local);
					FREE(sha1_pkg);
					FREE(sha1_orig);
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
					archive_entry_set_pathname (entry, expath);
					if(archive_read_extract (archive, entry, ARCHIVE_EXTRACT_FLAGS) != ARCHIVE_OK) {
						_alpm_log(PM_LOG_ERROR, _("could not extract %s (%s)"), expath, strerror(errno));
						alpm_logaction(_("error: could not extract %s (%s)"), expath, strerror(errno));
						errors++;
					}
					/* calculate an md5 or sha1 hash if this is in info->backup */
					for(lp = info->backup; lp; lp = lp->next) {
						char *fn, *md5, *sha1;
						char path[PATH_MAX];
						char *file = lp->data;

						if(!file) continue;
						if(!strcmp(file, pathname)) {
							_alpm_log(PM_LOG_DEBUG, _("appending backup entry"));
							snprintf(path, PATH_MAX, "%s%s", handle->root, file);
							if (info->sha1sum != NULL && info->sha1sum != '\0') {
							    md5 = _alpm_MDFile(path);
							    /* 32 for the hash, 1 for the terminating NULL, and 1 for the tab delimiter */
							    if((fn = (char *)malloc(strlen(file)+34)) == NULL) {
										RET_ERR(PM_ERR_MEMORY, -1);
									}
							    sprintf(fn, "%s\t%s", file, md5);
							    FREE(md5);
							} else {
							    /* 41 for the hash, 1 for the terminating NULL, and 1 for the tab delimiter */
							    sha1 = _alpm_SHAFile(path);
							    if((fn = (char *)malloc(strlen(file)+43)) == NULL) {
										RET_ERR(PM_ERR_MEMORY, -1);
									}
							    sprintf(fn, "%s\t%s", file, sha1);
							    FREE(sha1);
							}
							FREE(file);
							lp->data = fn;
						}
					}
				}
			}
			if(strlen(cwd)) {
				chdir(cwd);
			}
			archive_read_finish (archive);

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
		for(lp = _alpm_db_get_pkgcache(db, INFRQ_DEPENDS); lp; lp = lp->next) {
			pmpkg_t *tmpp = lp->data;
			pmlist_t *tmppm = NULL;
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
				pmlist_t *provides = _alpm_db_whatprovides(db, depend.name);
				if(provides) {
					/* TODO: should check _all_ packages listed in provides, not just
					 *       the first one.
					 */
					/* use the first one */
					depinfo = _alpm_db_get_pkgfromcache(db, ((pmpkg_t *)provides->data)->name);
					FREELISTPTR(provides);
				}
				if(depinfo == NULL) {
					/* We'll let the front end deal with this case... it may be intentional */
					_alpm_log(PM_LOG_DEBUG, _("could not find dependency '%s'"), depend.name);
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

		PROGRESS(trans, cb_state, what, 100, _alpm_list_count(trans->packages), (_alpm_list_count(trans->packages) - _alpm_list_count(targ) +1));
		needdisp = 0;
		EVENT(trans, PM_TRANS_EVT_EXTRACT_DONE, NULL, NULL);
		FREE(what);

		/* run the post-install script if it exists  */
		if(info->scriptlet && !(trans->flags & PM_TRANS_FLAG_NOSCRIPTLET)) {
			snprintf(pm_install, PATH_MAX, "%s%s/%s/%s-%s/install", handle->root, handle->dbpath, db->treename, info->name, info->version);
			if(pmo_upgrade) {
				_alpm_runscriptlet(handle->root, pm_install, "post_upgrade", info->version, oldpkg ? oldpkg->version : NULL, trans);
			} else {
				_alpm_runscriptlet(handle->root, pm_install, "post_install", info->version, NULL, trans);
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
