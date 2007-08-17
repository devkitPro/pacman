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

/* libalpm */
#include "add.h"
#include "alpm_list.h"
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
#include "remove.h"
#include "handle.h"

int _alpm_add_loadtarget(pmtrans_t *trans, pmdb_t *db, char *name)
{
	pmpkg_t *info = NULL;
	pmpkg_t *dummy;
	char pkgname[PKG_NAME_LEN], pkgver[PKG_VERSION_LEN];
	alpm_list_t *i;
	struct stat buf;

	ALPM_LOG_FUNC;

	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));
	ASSERT(name != NULL && strlen(name) != 0, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	_alpm_log(PM_LOG_DEBUG, _("loading target '%s'"), name);

	/* TODO FS#5120 we need a better way to check if a package is a valid package,
	 * and read the metadata instead of relying on the filename for package name
	 * and version */
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
				          pkg->name, pkg->version);
			}
			return(0);
		}
	}

	_alpm_log(PM_LOG_DEBUG, _("reading '%s' metadata"), pkgname);
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
	trans->packages = alpm_list_add(trans->packages, info);

	return(0);

error:
	FREEPKG(info);
	return(-1);
}


/* This is still messy. We have a lot of compare functions, and we should
 * try to consolidate them as much as we can (between add and sync) */
/*static int deppkg_cmp(const void *p1, const void *p2)
{
	return(strcmp(((pmdepmissing_t *)p1)->target,
				        ((pmdepmissing_t *)p2)->target));
}*/

int _alpm_add_prepare(pmtrans_t *trans, pmdb_t *db, alpm_list_t **data)
{
	alpm_list_t *lp = NULL, *i = NULL;
	alpm_list_t *rmlist = NULL;
	char rm_fname[PATH_MAX];
	pmpkg_t *info = NULL;

	ALPM_LOG_FUNC;

	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));

	/* Check dependencies
	 */
	if(!(trans->flags & PM_TRANS_FLAG_NODEPS)) {
		EVENT(trans, PM_TRANS_EVT_CHECKDEPS_START, NULL, NULL);

		/* look for unsatisfied dependencies */
		_alpm_log(PM_LOG_DEBUG, _("looking for unsatisfied dependencies"));
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
		_alpm_log(PM_LOG_DEBUG, _("looking for conflicts"));
		lp = _alpm_checkconflicts(db, trans->packages);
		for(i = lp; i; i = i->next) {
			pmdepmissing_t *miss = i->data;

			_alpm_log(PM_LOG_ERROR, _("replacing packages with -A and -U is not supported yet"));
			_alpm_log(PM_LOG_ERROR, _("please remove '%s' first, using -Rd"), miss->depend.name);
			RET_ERR(PM_ERR_CONFLICTING_DEPS, -1);
			
			/* Attempt to resolve conflicts */
			/*
			int skip_this = 0;
			QUESTION(trans, PM_TRANS_CONV_CONFLICT_PKG, miss->target, miss->depend.name, NULL, &skip_this);
			if(skip_this) {
				pmdepmissing_t *pkg = NULL;
				lp = alpm_list_remove(lp, (void *)miss, deppkg_cmp, (void*)&pkg);
				*/
				/* TODO: We remove the conflict from the list but never actually remove
				 * the package. Need to do this to fix FS #3492. The sync code should
				 * provide an example of how to do this, as it handles replaces and
				 * removes. We run into problems because we do a file conflict check
				 * below and it fails there. A force flag will skip that part, but
				 * still not remove the original package designated here for removal.
				 * Better yet, dump all this shitty duplicate code and somehow combine
				 * it with the sync code. */
				/*
				FREE(pkg);
				if(lp == NULL) {
					break;
				}
			}
			*/
		}
		/* Removal code should go here, as described above. Instead of simply
		 * removing items, perhaps throw them in another list to be removed, then
		 * proceed as sync.c would? I'm not sure because I'm not familiar enough
		 * with the codebase. */
		if(lp != NULL) {
			if(data) {
				*data = lp;
			} else {
				FREELIST(lp);
			}
			RET_ERR(PM_ERR_CONFLICTING_DEPS, -1);
		}

		/* re-order w.r.t. dependencies */
		_alpm_log(PM_LOG_DEBUG, _("sorting by dependencies"));
		lp = _alpm_sortbydeps(trans->packages, PM_TRANS_TYPE_ADD);
		/* free the old alltargs */
		FREELISTPTR(trans->packages);
		trans->packages = lp;

		EVENT(trans, PM_TRANS_EVT_CHECKDEPS_DONE, NULL, NULL);
	}

	/* Cleaning up
	 */
	EVENT(trans, PM_TRANS_EVT_CLEANUP_START, NULL, NULL);
	_alpm_log(PM_LOG_DEBUG, _("cleaning up"));
	for (lp=trans->packages; lp!=NULL; lp=lp->next) {
		info=(pmpkg_t *)lp->data;
		for (rmlist = alpm_pkg_get_removes(info); rmlist; rmlist = rmlist->next) {
			snprintf(rm_fname, PATH_MAX, "%s%s", handle->root, (char *)rmlist->data);
			remove(rm_fname);
		}
	}
	EVENT(trans, PM_TRANS_EVT_CLEANUP_DONE, NULL, NULL);

	/* Check for file conflicts
	 */
	if(!(trans->flags & PM_TRANS_FLAG_FORCE)) {
		EVENT(trans, PM_TRANS_EVT_FILECONFLICTS_START, NULL, NULL);

		_alpm_log(PM_LOG_DEBUG, _("looking for file conflicts"));
		lp = _alpm_db_find_conflicts(db, trans, handle->root);
		if(lp != NULL) {
			if(data) {
				*data = lp;
			} else {
				FREELIST(lp);
			}
			RET_ERR(PM_ERR_FILE_CONFLICTS, -1);
		}

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
	int i, ret = 0, errors = 0, pkg_count = 0;
	struct archive *archive;
	struct archive_entry *entry;
	char cwd[PATH_MAX] = "";
	alpm_list_t *targ, *lp;

	ALPM_LOG_FUNC;

	ASSERT(trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));

	if(trans->packages == NULL) {
		return(0);
	}

	pkg_count = alpm_list_count(trans->targets);
	
	for(targ = trans->packages; targ; targ = targ->next) {
		char scriptlet[PATH_MAX+1];
		int targ_count = 0, is_upgrade = 0, use_md5 = 0;
		double percent = 0.0;
		pmpkg_t *newpkg = (pmpkg_t *)targ->data;
		pmpkg_t *oldpkg = NULL;
		errors = 0;

		if(handle->trans->state == STATE_INTERRUPTED) {
			break;
		}

		snprintf(scriptlet, PATH_MAX, "%s%s-%s/install", db->path,
						 alpm_pkg_get_name(newpkg), alpm_pkg_get_version(newpkg));

		/* check if we have a valid sha1sum, if not, use MD5 */
		if(strlen(newpkg->sha1sum) == 0) {
			use_md5 = 1;
		}

		/* see if this is an upgrade.  if so, remove the old package first */
		pmpkg_t *local = _alpm_db_get_pkgfromcache(db, newpkg->name);
		if(local) {
			is_upgrade = 1;

			EVENT(trans, PM_TRANS_EVT_UPGRADE_START, newpkg, NULL);
			_alpm_log(PM_LOG_DEBUG, _("upgrading package %s-%s"), newpkg->name, newpkg->version);

			/* we'll need to save some record for backup checks later */
			oldpkg = _alpm_pkg_new(local->name, local->version);
			if(oldpkg) {
				oldpkg->backup = alpm_list_strdup(alpm_pkg_get_backup(local));
				oldpkg->provides = alpm_list_strdup(alpm_pkg_get_provides(local));
				strncpy(oldpkg->name, local->name, PKG_NAME_LEN);
				strncpy(oldpkg->version, local->version, PKG_VERSION_LEN);
			} else {
				RET_ERR(PM_ERR_MEMORY, -1);
			}

			/* copy over the install reason */
			newpkg->reason = alpm_pkg_get_reason(local);

			/* pre_upgrade scriptlet */
			if(alpm_pkg_has_scriptlet(newpkg) && !(trans->flags & PM_TRANS_FLAG_NOSCRIPTLET)) {
				_alpm_runscriptlet(handle->root, newpkg->data, "pre_upgrade", newpkg->version, oldpkg->version, trans);
			}
		} else {
			is_upgrade = 0;

			EVENT(trans, PM_TRANS_EVT_ADD_START, newpkg, NULL);
			_alpm_log(PM_LOG_DEBUG, _("adding package %s-%s"), newpkg->name, newpkg->version);
			
			/* pre_install scriptlet */
			if(alpm_pkg_has_scriptlet(newpkg) && !(trans->flags & PM_TRANS_FLAG_NOSCRIPTLET)) {
				_alpm_runscriptlet(handle->root, newpkg->data, "pre_install", newpkg->version, NULL, trans);
			}
		}

		if(oldpkg) {
			/* this is kinda odd.  If the old package exists, at this point we make a
			 * NEW transaction, unrelated to handle->trans, and instantiate a "remove"
			 * with the type PM_TRANS_TYPE_UPGRADE. TODO: kill this weird behavior. */
			pmtrans_t *tr = _alpm_trans_new();
			_alpm_log(PM_LOG_DEBUG, _("removing old package first (%s-%s)"), oldpkg->name, oldpkg->version);

			if(!tr) {
				RET_ERR(PM_ERR_TRANS_ABORT, -1);
			}

			if(_alpm_trans_init(tr, PM_TRANS_TYPE_UPGRADE, trans->flags, NULL, NULL, NULL) == -1) {
				FREETRANS(tr);
				RET_ERR(PM_ERR_TRANS_ABORT, -1);
			}

			if(_alpm_remove_loadtarget(tr, db, newpkg->name) == -1) {
				FREETRANS(tr);
				RET_ERR(PM_ERR_TRANS_ABORT, -1);
			}

			/* copy the remove skiplist over */
			tr->skip_remove = alpm_list_strdup(trans->skip_remove);
			alpm_list_t *b;

			/* Add files in the NEW package's backup array to the noupgrade array
			 * so this removal operation doesn't kill them */
			/* TODO if we add here, all backup=() entries for all targets, new and
			 * old, we cover all bases, including backup=() locations changing hands.
			 * But is this viable? */
			alpm_list_t *old_noupgrade = alpm_list_strdup(handle->noupgrade);
			for(b = alpm_pkg_get_backup(newpkg); b; b = b->next) {
				const char *backup = b->data;
				_alpm_log(PM_LOG_DEBUG, _("adding %s to the NoUpgrade array temporarily"), backup);
				handle->noupgrade = alpm_list_add(handle->noupgrade, strdup(backup));
			}

			int ret = _alpm_remove_commit(tr, db);

			FREETRANS(tr);
			/* restore our "NoUpgrade" list to previous state */
			alpm_list_free_inner(handle->noupgrade, free);
			alpm_list_free(handle->noupgrade);
			handle->noupgrade = old_noupgrade;

			if(ret == -1) {
				RET_ERR(PM_ERR_TRANS_ABORT, -1);
			}
		}

		if(!(trans->flags & PM_TRANS_FLAG_DBONLY)) {
			_alpm_log(PM_LOG_DEBUG, _("extracting files"));

			if ((archive = archive_read_new()) == NULL) {
				RET_ERR(PM_ERR_LIBARCHIVE_ERROR, -1);
			}

			archive_read_support_compression_all(archive);
			archive_read_support_format_all(archive);

			if(archive_read_open_file(archive, newpkg->data, ARCHIVE_DEFAULT_BYTES_PER_BLOCK) != ARCHIVE_OK) {
				RET_ERR(PM_ERR_PKG_OPEN, -1);
			}

			/* save the cwd so we can restore it later */
			if(getcwd(cwd, PATH_MAX) == NULL) {
				_alpm_log(PM_LOG_ERROR, _("could not get current working directory"));
				cwd[0] = 0;
			}

			/* libarchive requires this for extracting hard links */
			chdir(handle->root);

			targ_count = alpm_list_count(targ);
			/* call PROGRESS once with 0 percent, as we sort-of skip that here */
			PROGRESS(trans, (is_upgrade ? PM_TRANS_PROGRESS_UPGRADE_START : PM_TRANS_PROGRESS_ADD_START),
							 newpkg->name, 0, pkg_count, (pkg_count - targ_count +1));

			for(i = 0; archive_read_next_header(archive, &entry) == ARCHIVE_OK; i++) {
				const char *entryname; /* the name of the file in the archive */
				char filename[PATH_MAX]; /* the actual file we're extracting */
				int needbackup = 0, notouch = 0;
				char *hash_orig = NULL;
				struct stat buf;

				entryname = archive_entry_pathname(entry);

				if(newpkg->size != 0) {
					/* Using compressed size for calculations here, as newpkg->isize is not
					 * exact when it comes to comparing to the ACTUAL uncompressed size
					 * (missing metadata sizes) */
					unsigned long pos = archive_position_compressed(archive);
					percent = (double)pos / (double)newpkg->size;
					_alpm_log(PM_LOG_DEBUG, "decompression progress: %f%% (%ld / %ld)", percent*100.0, pos, newpkg->size);
					if(percent >= 1.0) {
						percent = 1.0;
					}
				}
				
				PROGRESS(trans, (is_upgrade ? PM_TRANS_PROGRESS_UPGRADE_START : PM_TRANS_PROGRESS_ADD_START),
								 newpkg->name, (int)(percent * 100), pkg_count, (pkg_count - targ_count +1));

				memset(filename, 0, PATH_MAX); /* just to be sure */

				if(strcmp(entryname, ".PKGINFO") == 0 || strcmp(entryname, ".FILELIST") == 0) {
					archive_read_data_skip(archive);
					continue;
				} else if(strcmp(entryname, ".INSTALL") == 0) {
					/* the install script goes inside the db */
					snprintf(filename, PATH_MAX, "%s/%s-%s/install", db->path,
									 newpkg->name, newpkg->version);
				} else if(strcmp(entryname, ".CHANGELOG") == 0) {
					/* the changelog goes inside the db */
					snprintf(filename, PATH_MAX, "%s/%s-%s/changelog", db->path,
									 newpkg->name, newpkg->version);
				} else {
					/* build the new entryname relative to handle->root */
					snprintf(filename, PATH_MAX, "%s%s", handle->root, entryname);
				}

				/* if a file is in NoExtract then we never extract it */
				if(alpm_list_find_str(handle->noextract, entryname)) {
					_alpm_log(PM_LOG_DEBUG, _("%s is in NoExtract, skipping extraction"), entryname);
					alpm_logaction(_("%s is in NoExtract, skipping extraction"), entryname);
					archive_read_data_skip(archive);
					continue;
				}

				/* if a file is in the add skiplist we never extract it */
				if(alpm_list_find_str(trans->skip_add, filename)) {
					_alpm_log(PM_LOG_DEBUG, _("%s is in trans->skip_add, skipping extraction"), entryname);
					archive_read_data_skip(archive);
					continue;
				}

				/* check is file already exists */
				if(stat(filename, &buf) == 0 && !S_ISDIR(buf.st_mode)) {
					/* it does, is it a backup=() file?
					 * always check the newpkg first, so when we do add a backup=() file,
					 * we don't have to wait a full upgrade cycle */
					needbackup = alpm_list_find_str(alpm_pkg_get_backup(newpkg), entryname);

					if(is_upgrade) {
						hash_orig = _alpm_needbackup(entryname, alpm_pkg_get_backup(oldpkg));
						if(hash_orig) {
							needbackup = 1;
						}
					}

					/* this is kind of gross.  if we force hash_orig to be non-NULL we can
					 * catch the pro-active backup=() case (when the backup entry is in
					 * the new package, and not the old */
					if(needbackup && !hash_orig) {
						hash_orig = strdup("");
					}
					
					/* NoUpgrade skips all this backup stuff, because it's just never
					 * touched */
					if(alpm_list_find_str(handle->noupgrade, entryname)) {
						notouch = 1;
						needbackup = 0;
					}
				}

				if(needbackup) {
					char *tempfile = NULL;
					char *hash_local = NULL, *hash_pkg = NULL;
					int fd;

					/* extract the package's version to a temporary file and md5 it */
					tempfile = strdup("/tmp/alpm_XXXXXX");
					fd = mkstemp(tempfile);
					
					archive_entry_set_pathname(entry, tempfile);

					if(archive_read_extract(archive, entry, ARCHIVE_EXTRACT_FLAGS) != ARCHIVE_OK) {
						_alpm_log(PM_LOG_ERROR, _("could not extract %s (%s)"), entryname, strerror(errno));
						alpm_logaction(_("could not extract %s (%s)"), entryname, strerror(errno));
						errors++;
						unlink(tempfile);
						FREE(hash_orig);
						close(fd);
						continue;
					}

					if(use_md5) {
						hash_local = _alpm_MDFile(filename);
						hash_pkg = _alpm_MDFile(tempfile);
					} else {
						hash_local = _alpm_SHAFile(filename);
						hash_pkg = _alpm_SHAFile(tempfile);
					}

					/* append the new md5 or sha1 hash to it's respective entry in newpkg's backup
					 * (it will be the new orginal) */
					for(lp = alpm_pkg_get_backup(newpkg); lp; lp = lp->next) {
						if(!lp->data || strcmp(lp->data, entryname) != 0) {
							continue;
						}
						char *backup = NULL;
						int backup_len = strlen(lp->data) + 2; /* tab char and null byte */

						if(use_md5) {
							backup_len += 32; /* MD5s are 32 chars in length */
						} else {
							backup_len += 40; /* SHA1s are 40 chars in length */
						}

						backup = malloc(backup_len);
						if(!backup) {
							RET_ERR(PM_ERR_MEMORY, -1);
						}

						sprintf(backup, "%s\t%s", (char *)lp->data, hash_pkg);
						backup[backup_len-1] = '\0';
						FREE(lp->data);
						lp->data = backup;
					}

					if(use_md5) {
						_alpm_log(PM_LOG_DEBUG, _("checking md5 hashes for %s"), entryname);
					} else {
						_alpm_log(PM_LOG_DEBUG, _("checking sha1 hashes for %s"), entryname);
					}
					_alpm_log(PM_LOG_DEBUG, _("current:  %s"), hash_local);
					_alpm_log(PM_LOG_DEBUG, _("new:      %s"), hash_pkg);
					_alpm_log(PM_LOG_DEBUG, _("original: %s"), hash_orig);

					if(!is_upgrade) {
						/* looks like we have a local file that has a different hash as the
						 * file in the package, move it to a .pacorig */
						if(strcmp(hash_local, hash_pkg) != 0) {
							char newpath[PATH_MAX];
							snprintf(newpath, PATH_MAX, "%s.pacorig", filename);

							/* move the existing file to the "pacorig" */
							if(rename(filename, newpath)) {
								archive_entry_set_pathname(entry, filename);
								_alpm_log(PM_LOG_ERROR, _("could not rename %s (%s)"), filename, strerror(errno));
								alpm_logaction(_("error: could not rename %s (%s)"), filename, strerror(errno));
								errors++;
							} else {
								/* copy the tempfile we extracted to the real path */
								if(_alpm_copyfile(tempfile, filename)) {
									archive_entry_set_pathname(entry, filename);
									_alpm_log(PM_LOG_ERROR, _("could not copy tempfile to %s (%s)"), filename, strerror(errno));
									alpm_logaction(_("error: could not copy tempfile to %s (%s)"), filename, strerror(errno));
									errors++;
								} else {
									archive_entry_set_pathname(entry, filename);
									_alpm_log(PM_LOG_WARNING, _("%s saved as %s"), filename, newpath);
									alpm_logaction(_("warning: %s saved as %s"), filename, newpath);
								}
							}
						}
					} else if(hash_orig) {
						/* the fun part */

						if(strcmp(hash_orig, hash_local) == 0) {
							/* installed file has NOT been changed by user */
							if(strcmp(hash_orig, hash_pkg) != 0) {
								_alpm_log(PM_LOG_DEBUG, _("action: installing new file: %s"), entryname);

								if(_alpm_copyfile(tempfile, filename)) {
									_alpm_log(PM_LOG_ERROR, _("could not copy tempfile to %s (%s)"), filename, strerror(errno));
									errors++;
								}
								archive_entry_set_pathname(entry, filename);
							} else {
								/* there's no sense in installing the same file twice, install
								 * ONLY is the original and package hashes differ */
								_alpm_log(PM_LOG_DEBUG, _("action: leaving existing file in place"));
							}
						} else if(strcmp(hash_orig, hash_pkg) == 0) {
							/* originally installed file and new file are the same - this
							 * implies the case above failed - i.e. the file was changed by a
							 * user */
							_alpm_log(PM_LOG_DEBUG, _("action: leaving existing file in place"));
						} else if(strcmp(hash_local, hash_pkg) == 0) {
							/* this would be magical.  The above two cases failed, but the
							 * user changes just so happened to make the new file exactly the
							 * same as the one in the package... skip it */
							_alpm_log(PM_LOG_DEBUG, _("action: leaving existing file in place"));
						} else {
							char newpath[PATH_MAX];
							_alpm_log(PM_LOG_DEBUG, _("action: keeping current file and installing new one with .pacnew ending"));
							snprintf(newpath, PATH_MAX, "%s.pacnew", filename);
							if(_alpm_copyfile(tempfile, newpath)) {
								_alpm_log(PM_LOG_ERROR, _("could not install %s as %s: %s"), filename, newpath, strerror(errno));
								alpm_logaction(_("error: could not install %s as %s: %s"), filename, newpath, strerror(errno));
							} else {
								_alpm_log(PM_LOG_WARNING, _("%s installed as %s"), filename, newpath);
								alpm_logaction(_("warning: %s installed as %s"), filename, newpath);
							}
						}
					}

					FREE(hash_local);
					FREE(hash_pkg);
					FREE(hash_orig);
					unlink(tempfile);
					FREE(tempfile);
					close(fd);
				} else { /* ! needbackup */

					if(notouch) {
						_alpm_log(PM_LOG_DEBUG, _("%s is in NoUpgrade -- skipping"), filename);
						_alpm_log(PM_LOG_WARNING, _("extracting %s as %s.pacnew"), filename, filename);
						alpm_logaction(_("warning: extracting %s as %s.pacnew"), filename, filename);
						strncat(filename, ".pacnew", PATH_MAX);
					} else {
						_alpm_log(PM_LOG_DEBUG, _("extracting %s"), filename);
					}

					if(trans->flags & PM_TRANS_FLAG_FORCE) {
						/* if FORCE was used, then unlink() each file (whether it's there
						 * or not) before extracting.  this prevents the old "Text file busy"
						 * error that crops up if one tries to --force a glibc or pacman
						 * upgrade.
						 */
						unlink(filename);
					}

					archive_entry_set_pathname(entry, filename);

					int ret = archive_read_extract(archive, entry, 
								ARCHIVE_EXTRACT_FLAGS | ARCHIVE_EXTRACT_NO_OVERWRITE);
					if(ret != ARCHIVE_OK && ret != ARCHIVE_WARN) {
						_alpm_log(PM_LOG_ERROR, _("could not extract %s (%s)"), filename, strerror(errno));
						alpm_logaction(_("error: could not extract %s (%s)"), filename, strerror(errno));
						errors++;
					}

					/* calculate an hash if this is in newpkg's backup */
					for(lp = alpm_pkg_get_backup(newpkg); lp; lp = lp->next) {
						char *backup = NULL, *hash = NULL;
						int backup_len = strlen(lp->data) + 2; /* tab char and null byte */

						if(!lp->data || strcmp(lp->data, entryname) != 0) {
							continue;
						}
						_alpm_log(PM_LOG_DEBUG, _("appending backup entry for %s"), filename);

						if(use_md5) {
							backup_len += 32; /* MD5s are 32 chars in length */
							hash = _alpm_MDFile(filename);
						} else {
							backup_len += 40; /* SHA1s are 40 chars in length */
							hash = _alpm_SHAFile(filename);
						}

						backup = malloc(backup_len);
						if(!backup) {
							RET_ERR(PM_ERR_MEMORY, -1);
						}

						sprintf(backup, "%s\t%s", (char *)lp->data, hash);
						backup[backup_len-1] = '\0';
						FREE(hash);
						FREE(lp->data);
						lp->data = backup;
					}
				}
			}
			archive_read_finish(archive);

			/* restore the old cwd is we have it */
			if(strlen(cwd)) {
				chdir(cwd);
			}

			if(errors) {
				ret = 1;
				_alpm_log(PM_LOG_ERROR, _("errors occurred while %s %s"),
					(is_upgrade ? _("upgrading") : _("installing")), newpkg->name);
				alpm_logaction(_("errors occurred while %s %s"),
					(is_upgrade ? _("upgrading") : _("installing")), newpkg->name);
			}
		}

		/* Update the requiredby field by scanning the whole database 
		 * looking for packages depending on the package to add */
		_alpm_pkg_update_requiredby(newpkg);

		/* special case: if our provides list has changed from oldpkg to newpkg AND
		 * we get here, we need to make sure we find the actual provision that
		 * still satisfies this case, and update its 'requiredby' field... ugh */
		alpm_list_t *provdiff, *prov;
		provdiff = alpm_list_diff(alpm_pkg_get_provides(oldpkg),
														 	alpm_pkg_get_provides(newpkg),
															_alpm_str_cmp);
		for(prov = provdiff; prov; prov = prov->next) {
			const char *provname = prov->data;
			_alpm_log(PM_LOG_DEBUG, _("provision '%s' has been removed from package %s (%s => %s)"),
								provname, alpm_pkg_get_name(oldpkg),
								alpm_pkg_get_version(oldpkg), alpm_pkg_get_version(newpkg));

			alpm_list_t *p = _alpm_db_whatprovides(handle->db_local, provname);
			if(p) {
				/* we now have all the provisions in the local DB for this virtual
				 * package... seeing as we can't really determine which is the 'correct'
				 * provision, we'll use the FIRST for now.
				 * TODO figure out a way to find a "correct" provision */
				pmpkg_t *provpkg = p->data;
				const char *pkgname = alpm_pkg_get_name(provpkg);
				_alpm_log(PM_LOG_DEBUG, _("updating '%s' due to provision change (%s)"), pkgname, provname);
				_alpm_pkg_update_requiredby(provpkg);

				if(_alpm_db_write(db, provpkg, INFRQ_DEPENDS)) {
					_alpm_log(PM_LOG_ERROR, _("could not update provision '%s' from '%s'"), provname, pkgname);
					alpm_logaction(_("could not update provision '%s' from '%s'"), provname, pkgname);
					RET_ERR(PM_ERR_DB_WRITE, -1);
				}
			}
		}
		alpm_list_free(provdiff);

		/* make an install date (in UTC) */
		time_t t = time(NULL);
		strncpy(newpkg->installdate, asctime(gmtime(&t)), PKG_DATE_LEN);
		/* remove the extra line feed appended by asctime() */
		newpkg->installdate[strlen(newpkg->installdate)-1] = 0;

		_alpm_log(PM_LOG_DEBUG, _("updating database"));
		_alpm_log(PM_LOG_DEBUG, _("adding database entry '%s'"), newpkg->name);

		if(_alpm_db_write(db, newpkg, INFRQ_ALL)) {
			_alpm_log(PM_LOG_ERROR, _("could not update database entry %s-%s"),
								alpm_pkg_get_name(newpkg), alpm_pkg_get_version(newpkg));
			alpm_logaction(_("could not update database entry %s-%s"),
										 alpm_pkg_get_name(newpkg), alpm_pkg_get_version(newpkg));
			RET_ERR(PM_ERR_DB_WRITE, -1);
		}
		
		if(_alpm_db_add_pkgincache(db, newpkg) == -1) {
			_alpm_log(PM_LOG_ERROR, _("could not add entry '%s' in cache"),
										 alpm_pkg_get_name(newpkg));
		}

		/* update dependency packages' REQUIREDBY fields */
		_alpm_trans_update_depends(trans, newpkg);

		PROGRESS(trans, (is_upgrade ? PM_TRANS_PROGRESS_UPGRADE_START : PM_TRANS_PROGRESS_ADD_START),
						 alpm_pkg_get_name(newpkg), 100, pkg_count, (pkg_count - targ_count +1));
		EVENT(trans, PM_TRANS_EVT_EXTRACT_DONE, NULL, NULL);

		/* run the post-install script if it exists  */
		if(alpm_pkg_has_scriptlet(newpkg) && !(trans->flags & PM_TRANS_FLAG_NOSCRIPTLET)) {
			if(is_upgrade) {
				_alpm_runscriptlet(handle->root, scriptlet, "post_upgrade",
													alpm_pkg_get_version(newpkg), oldpkg ? alpm_pkg_get_version(oldpkg) : NULL,
													trans);
			} else {
				_alpm_runscriptlet(handle->root, scriptlet, "post_install",
													 alpm_pkg_get_version(newpkg), NULL, trans);
			}
		}

		EVENT(trans, (is_upgrade) ? PM_TRANS_EVT_UPGRADE_DONE : PM_TRANS_EVT_ADD_DONE, newpkg, oldpkg);

		FREEPKG(oldpkg);
	}

	/* run ldconfig if it exists */
	if(handle->trans->state != STATE_INTERRUPTED) {
		_alpm_log(PM_LOG_DEBUG, _("running \"ldconfig -r %s\""), handle->root);
		_alpm_ldconfig(handle->root);
	}

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
