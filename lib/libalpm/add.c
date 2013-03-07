/*
 *  add.c
 *
 *  Copyright (c) 2006-2013 Pacman Development Team <pacman-dev@archlinux.org>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h> /* int64_t */

/* libarchive */
#include <archive.h>
#include <archive_entry.h>

/* libalpm */
#include "add.h"
#include "alpm.h"
#include "alpm_list.h"
#include "handle.h"
#include "libarchive-compat.h"
#include "trans.h"
#include "util.h"
#include "log.h"
#include "backup.h"
#include "package.h"
#include "db.h"
#include "remove.h"
#include "handle.h"

/** Add a package to the transaction. */
int SYMEXPORT alpm_add_pkg(alpm_handle_t *handle, alpm_pkg_t *pkg)
{
	const char *pkgname, *pkgver;
	alpm_trans_t *trans;
	alpm_pkg_t *local;

	/* Sanity checks */
	CHECK_HANDLE(handle, return -1);
	ASSERT(pkg != NULL, RET_ERR(handle, ALPM_ERR_WRONG_ARGS, -1));
	ASSERT(handle == pkg->handle, RET_ERR(handle, ALPM_ERR_WRONG_ARGS, -1));
	trans = handle->trans;
	ASSERT(trans != NULL, RET_ERR(handle, ALPM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_INITIALIZED,
			RET_ERR(handle, ALPM_ERR_TRANS_NOT_INITIALIZED, -1));

	pkgname = pkg->name;
	pkgver = pkg->version;

	_alpm_log(handle, ALPM_LOG_DEBUG, "adding package '%s'\n", pkgname);

	if(alpm_pkg_find(trans->add, pkgname)) {
		RET_ERR(handle, ALPM_ERR_TRANS_DUP_TARGET, -1);
	}

	local = _alpm_db_get_pkgfromcache(handle->db_local, pkgname);
	if(local) {
		const char *localpkgname = local->name;
		const char *localpkgver = local->version;
		int cmp = _alpm_pkg_compare_versions(pkg, local);

		if(cmp == 0) {
			if(trans->flags & ALPM_TRANS_FLAG_NEEDED) {
				/* with the NEEDED flag, packages up to date are not reinstalled */
				_alpm_log(handle, ALPM_LOG_WARNING, _("%s-%s is up to date -- skipping\n"),
						localpkgname, localpkgver);
				return 0;
			} else if(!(trans->flags & ALPM_TRANS_FLAG_DOWNLOADONLY)) {
				_alpm_log(handle, ALPM_LOG_WARNING, _("%s-%s is up to date -- reinstalling\n"),
						localpkgname, localpkgver);
			}
		} else if(cmp < 0) {
			/* local version is newer */
			_alpm_log(handle, ALPM_LOG_WARNING, _("downgrading package %s (%s => %s)\n"),
					localpkgname, localpkgver, pkgver);
		}
	}

	/* add the package to the transaction */
	pkg->reason = ALPM_PKG_REASON_EXPLICIT;
	_alpm_log(handle, ALPM_LOG_DEBUG, "adding package %s-%s to the transaction add list\n",
						pkgname, pkgver);
	trans->add = alpm_list_add(trans->add, pkg);

	return 0;
}

static int perform_extraction(alpm_handle_t *handle, struct archive *archive,
		struct archive_entry *entry, const char *filename, const char *origname)
{
	int ret;
	const int archive_flags = ARCHIVE_EXTRACT_OWNER |
	                          ARCHIVE_EXTRACT_PERM |
	                          ARCHIVE_EXTRACT_TIME;

	archive_entry_set_pathname(entry, filename);

	ret = archive_read_extract(archive, entry, archive_flags);
	if(ret == ARCHIVE_WARN && archive_errno(archive) != ENOSPC) {
		/* operation succeeded but a "non-critical" error was encountered */
		_alpm_log(handle, ALPM_LOG_WARNING, _("warning given when extracting %s (%s)\n"),
				origname, archive_error_string(archive));
	} else if(ret != ARCHIVE_OK) {
		_alpm_log(handle, ALPM_LOG_ERROR, _("could not extract %s (%s)\n"),
				origname, archive_error_string(archive));
		alpm_logaction(handle, ALPM_CALLER_PREFIX,
				"error: could not extract %s (%s)\n",
				origname, archive_error_string(archive));
		return 1;
	}
	return 0;
}

static int try_rename(alpm_handle_t *handle, const char *src, const char *dest)
{
	if(rename(src, dest)) {
		_alpm_log(handle, ALPM_LOG_ERROR, _("could not rename %s to %s (%s)\n"),
				src, dest, strerror(errno));
		alpm_logaction(handle, ALPM_CALLER_PREFIX,
				"error: could not rename %s to %s (%s)\n", src, dest, strerror(errno));
		return 1;
	}
	return 0;
}

static int extract_single_file(alpm_handle_t *handle, struct archive *archive,
		struct archive_entry *entry, alpm_pkg_t *newpkg, alpm_pkg_t *oldpkg)
{
	const char *entryname;
	mode_t entrymode;
	char filename[PATH_MAX]; /* the actual file we're extracting */
	int needbackup = 0, notouch = 0;
	const char *hash_orig = NULL;
	char *entryname_orig = NULL;
	int errors = 0;

	entryname = archive_entry_pathname(entry);
	entrymode = archive_entry_mode(entry);

	if(strcmp(entryname, ".INSTALL") == 0) {
		/* the install script goes inside the db */
		snprintf(filename, PATH_MAX, "%s%s-%s/install",
				_alpm_db_path(handle->db_local), newpkg->name, newpkg->version);
		archive_entry_set_perm(entry, 0644);
	} else if(strcmp(entryname, ".CHANGELOG") == 0) {
		/* the changelog goes inside the db */
		snprintf(filename, PATH_MAX, "%s%s-%s/changelog",
				_alpm_db_path(handle->db_local), newpkg->name, newpkg->version);
		archive_entry_set_perm(entry, 0644);
	} else if(strcmp(entryname, ".MTREE") == 0) {
		/* the mtree file goes inside the db */
		snprintf(filename, PATH_MAX, "%s%s-%s/mtree",
				_alpm_db_path(handle->db_local), newpkg->name, newpkg->version);
		archive_entry_set_perm(entry, 0644);
	} else if(*entryname == '.') {
		/* for now, ignore all files starting with '.' that haven't
		 * already been handled (for future possibilities) */
		_alpm_log(handle, ALPM_LOG_DEBUG, "skipping extraction of '%s'\n", entryname);
		archive_read_data_skip(archive);
		return 0;
	} else {
		/* build the new entryname relative to handle->root */
		snprintf(filename, PATH_MAX, "%s%s", handle->root, entryname);
	}

	/* if a file is in NoExtract then we never extract it */
	if(alpm_list_find(handle->noextract, entryname, _alpm_fnmatch)) {
		_alpm_log(handle, ALPM_LOG_DEBUG, "%s is in NoExtract,"
				" skipping extraction of %s\n",
				entryname, filename);
		alpm_logaction(handle, ALPM_CALLER_PREFIX,
				"note: %s is in NoExtract, skipping extraction\n", entryname);
		archive_read_data_skip(archive);
		return 0;
	}

	/* Check for file existence. This is one of the more crucial parts
	 * to get 'right'. Here are the possibilities, with the filesystem
	 * on the left and the package on the top:
	 * (F=file, N=node, S=symlink, D=dir)
	 *               |  F/N  |  S  |  D
	 *  non-existent |   1   |  2  |  3
	 *  F/N          |   4   |  5  |  6
	 *  S            |   7   |  8  |  9
	 *  D            |   10  |  11 |  12
	 *
	 *  1,2,3- extract, no magic necessary. lstat (_alpm_lstat) will fail here.
	 *  4,5,6,7,8- conflict checks should have caught this. either overwrite
	 *      or backup the file.
	 *  9- follow the symlink, hopefully it is a directory, check it.
	 *  10- file replacing directory- don't allow it.
	 *  11- don't extract symlink- a dir exists here. we don't want links to
	 *      links, etc.
	 *  12- skip extraction, dir already exists.
	 */

	/* do both a lstat and a stat, so we can see what symlinks point to */
	struct stat lsbuf, sbuf;
	if(_alpm_lstat(filename, &lsbuf) != 0 || stat(filename, &sbuf) != 0) {
		/* cases 1,2,3: couldn't stat an existing file, skip all backup checks */
	} else {
		if(S_ISDIR(lsbuf.st_mode)) {
			if(S_ISDIR(entrymode)) {
				/* case 12: existing dir, ignore it */
				if(lsbuf.st_mode != entrymode) {
					/* if filesystem perms are different than pkg perms, warn user */
					mode_t mask = 07777;
					_alpm_log(handle, ALPM_LOG_WARNING, _("directory permissions differ on %s\n"
								"filesystem: %o  package: %o\n"), filename, lsbuf.st_mode & mask,
							entrymode & mask);
					alpm_logaction(handle, ALPM_CALLER_PREFIX,
							"warning: directory permissions differ on %s\n"
							"filesystem: %o  package: %o\n", filename, lsbuf.st_mode & mask,
							entrymode & mask);
				}
				_alpm_log(handle, ALPM_LOG_DEBUG, "extract: skipping dir extraction of %s\n",
						filename);
				archive_read_data_skip(archive);
				return 0;
			} else {
				/* case 10/11: trying to overwrite dir with file/symlink, don't allow it */
				_alpm_log(handle, ALPM_LOG_ERROR, _("extract: not overwriting dir with file %s\n"),
						filename);
				archive_read_data_skip(archive);
				return 1;
			}
		} else if(S_ISLNK(lsbuf.st_mode) && S_ISDIR(entrymode)) {
			/* case 9: existing symlink, dir in package */
			if(S_ISDIR(sbuf.st_mode)) {
				/* the symlink on FS is to a directory, so we'll use it */
				_alpm_log(handle, ALPM_LOG_DEBUG, "extract: skipping symlink overwrite of %s\n",
						filename);
				archive_read_data_skip(archive);
				return 0;
			} else {
				/* this is BAD. symlink was not to a directory */
				_alpm_log(handle, ALPM_LOG_ERROR, _("extract: symlink %s does not point to dir\n"),
						filename);
				archive_read_data_skip(archive);
				return 1;
			}
		} else if(S_ISREG(lsbuf.st_mode) && S_ISDIR(entrymode)) {
			/* case 6: trying to overwrite file with dir */
			_alpm_log(handle, ALPM_LOG_DEBUG, "extract: overwriting file with dir %s\n",
					filename);
		} else if(S_ISREG(entrymode)) {
			/* case 4,7: */
			/* if file is in NoUpgrade, don't touch it */
			if(alpm_list_find(handle->noupgrade, entryname, _alpm_fnmatch)) {
				notouch = 1;
			} else {
				alpm_backup_t *backup;
				/* go to the backup array and see if our conflict is there */
				/* check newpkg first, so that adding backup files is retroactive */
				backup = _alpm_needbackup(entryname, newpkg);
				if(backup) {
					/* if we force hash_orig to be non-NULL retroactive backup works */
					hash_orig = "";
					needbackup = 1;
				}

				/* check oldpkg for a backup entry, store the hash if available */
				if(oldpkg) {
					backup = _alpm_needbackup(entryname, oldpkg);
					if(backup) {
						hash_orig = backup->hash;
						needbackup = 1;
					}
				}
			}
		}
		/* else if(S_ISLNK(entrymode)) */
		/* case 5,8: don't need to do anything special */
	}

	/* we need access to the original entryname later after calls to
	 * archive_entry_set_pathname(), so we need to dupe it and free() later */
	STRDUP(entryname_orig, entryname, RET_ERR(handle, ALPM_ERR_MEMORY, -1));

	if(needbackup) {
		char *checkfile;
		char *hash_local = NULL, *hash_pkg = NULL;
		size_t len;

		len = strlen(filename) + 10;
		MALLOC(checkfile, len,
				errors++; handle->pm_errno = ALPM_ERR_MEMORY; goto needbackup_cleanup);
		snprintf(checkfile, len, "%s.paccheck", filename);

		if(perform_extraction(handle, archive, entry, checkfile, entryname_orig)) {
			errors++;
			goto needbackup_cleanup;
		}

		hash_local = alpm_compute_md5sum(filename);
		hash_pkg = alpm_compute_md5sum(checkfile);

		/* update the md5 hash in newpkg's backup (it will be the new orginal) */
		alpm_list_t *i;
		for(i = alpm_pkg_get_backup(newpkg); i; i = i->next) {
			alpm_backup_t *backup = i->data;
			char *newhash;
			if(!backup->name || strcmp(backup->name, entryname_orig) != 0) {
				continue;
			}
			STRDUP(newhash, hash_pkg, RET_ERR(handle, ALPM_ERR_MEMORY, -1));
			FREE(backup->hash);
			backup->hash = newhash;
		}

		_alpm_log(handle, ALPM_LOG_DEBUG, "checking hashes for %s\n", entryname_orig);
		_alpm_log(handle, ALPM_LOG_DEBUG, "current:  %s\n", hash_local);
		_alpm_log(handle, ALPM_LOG_DEBUG, "new:      %s\n", hash_pkg);
		_alpm_log(handle, ALPM_LOG_DEBUG, "original: %s\n", hash_orig);

		if(!oldpkg) {
			if(hash_local && hash_pkg && strcmp(hash_local, hash_pkg) != 0) {
				/* looks like we have a local file that has a different hash as the
				 * file in the package, move it to a .pacorig */
				char *newpath;
				size_t newlen = strlen(filename) + 9;
				MALLOC(newpath, newlen,
						errors++; handle->pm_errno = ALPM_ERR_MEMORY; goto needbackup_cleanup);
				snprintf(newpath, newlen, "%s.pacorig", filename);

				/* move the existing file to the "pacorig" */
				if(try_rename(handle, filename, newpath)) {
						errors++;
					errors++;
				} else {
					/* rename the file we extracted to the real name */
					if(try_rename(handle, checkfile, filename)) {
						errors++;
					} else {
						_alpm_log(handle, ALPM_LOG_WARNING, _("%s saved as %s\n"), filename, newpath);
						alpm_logaction(handle, ALPM_CALLER_PREFIX,
								"warning: %s saved as %s\n", filename, newpath);
					}
				}
				free(newpath);
			} else {
				/* local file is identical to pkg one, so just remove pkg one */
				unlink(checkfile);
			}
		} else if(hash_orig) {
			/* the fun part */

			if(hash_local && strcmp(hash_orig, hash_local) == 0) {
				/* installed file has NOT been changed by user */
				if(hash_pkg && strcmp(hash_orig, hash_pkg) != 0) {
					_alpm_log(handle, ALPM_LOG_DEBUG, "action: installing new file: %s\n",
							entryname_orig);

					if(try_rename(handle, checkfile, filename)) {
						errors++;
					}
				} else {
					/* no sense in installing the same file twice, install
					 * ONLY if the original and package hashes differ */
					_alpm_log(handle, ALPM_LOG_DEBUG, "action: leaving existing file in place\n");
					unlink(checkfile);
				}
			} else if(hash_pkg && strcmp(hash_orig, hash_pkg) == 0) {
				/* originally installed file and new file are the same - this
				 * implies the case above failed - i.e. the file was changed by a
				 * user */
				_alpm_log(handle, ALPM_LOG_DEBUG, "action: leaving existing file in place\n");
				unlink(checkfile);
			} else if(hash_local && hash_pkg && strcmp(hash_local, hash_pkg) == 0) {
				/* this would be magical.  The above two cases failed, but the
				 * user changes just so happened to make the new file exactly the
				 * same as the one in the package... skip it */
				_alpm_log(handle, ALPM_LOG_DEBUG, "action: leaving existing file in place\n");
				unlink(checkfile);
			} else {
				char *newpath;
				size_t newlen = strlen(filename) + 8;
				_alpm_log(handle, ALPM_LOG_DEBUG, "action: keeping current file and installing"
						" new one with .pacnew ending\n");
				MALLOC(newpath, newlen,
						errors++; handle->pm_errno = ALPM_ERR_MEMORY; goto needbackup_cleanup);
				snprintf(newpath, newlen, "%s.pacnew", filename);
				if(try_rename(handle, checkfile, newpath)) {
					errors++;
				} else {
					_alpm_log(handle, ALPM_LOG_WARNING, _("%s installed as %s\n"),
							filename, newpath);
					alpm_logaction(handle, ALPM_CALLER_PREFIX,
							"warning: %s installed as %s\n", filename, newpath);
				}
				free(newpath);
			}
		}

needbackup_cleanup:
		free(checkfile);
		free(hash_local);
		free(hash_pkg);
	} else {
		/* we didn't need a backup */
		if(notouch) {
			/* change the path to a .pacnew extension */
			_alpm_log(handle, ALPM_LOG_DEBUG, "%s is in NoUpgrade -- skipping\n", filename);
			_alpm_log(handle, ALPM_LOG_WARNING, _("extracting %s as %s.pacnew\n"), filename, filename);
			alpm_logaction(handle, ALPM_CALLER_PREFIX,
					"warning: extracting %s as %s.pacnew\n", filename, filename);
			strncat(filename, ".pacnew", PATH_MAX - strlen(filename));
		} else {
			_alpm_log(handle, ALPM_LOG_DEBUG, "extracting %s\n", filename);
		}

		if(handle->trans->flags & ALPM_TRANS_FLAG_FORCE) {
			/* if FORCE was used, unlink() each file (whether it's there
			 * or not) before extracting. This prevents the old "Text file busy"
			 * error that crops up if forcing a glibc or pacman upgrade. */
			unlink(filename);
		}

		if(perform_extraction(handle, archive, entry, filename, entryname_orig)) {
			/* error */
			free(entryname_orig);
			errors++;
			return errors;
		}

		/* calculate an hash if this is in newpkg's backup */
		alpm_list_t *i;
		for(i = alpm_pkg_get_backup(newpkg); i; i = i->next) {
			alpm_backup_t *backup = i->data;
			char *newhash;
			if(!backup->name || strcmp(backup->name, entryname_orig) != 0) {
				continue;
			}
			_alpm_log(handle, ALPM_LOG_DEBUG, "appending backup entry for %s\n", entryname_orig);
			newhash = alpm_compute_md5sum(filename);
			FREE(backup->hash);
			backup->hash = newhash;
		}
	}
	free(entryname_orig);
	return errors;
}

static int commit_single_pkg(alpm_handle_t *handle, alpm_pkg_t *newpkg,
		size_t pkg_current, size_t pkg_count)
{
	int i, ret = 0, errors = 0;
	int is_upgrade = 0;
	alpm_pkg_t *oldpkg = NULL;
	alpm_db_t *db = handle->db_local;
	alpm_trans_t *trans = handle->trans;
	alpm_progress_t event = ALPM_PROGRESS_ADD_START;
	alpm_event_t done = ALPM_EVENT_ADD_DONE, start = ALPM_EVENT_ADD_START;
	const char *log_msg = "adding";
	const char *pkgfile;

	ASSERT(trans != NULL, return -1);

	/* see if this is an upgrade. if so, remove the old package first */
	alpm_pkg_t *local = _alpm_db_get_pkgfromcache(db, newpkg->name);
	if(local) {
		int cmp = _alpm_pkg_compare_versions(newpkg, local);
		if(cmp < 0) {
			log_msg = "downgrading";
			event = ALPM_PROGRESS_DOWNGRADE_START;
			start = ALPM_EVENT_DOWNGRADE_START;
			done = ALPM_EVENT_DOWNGRADE_DONE;
		} else if(cmp == 0) {
			log_msg = "reinstalling";
			event = ALPM_PROGRESS_REINSTALL_START;
			start = ALPM_EVENT_REINSTALL_START;
			done = ALPM_EVENT_REINSTALL_DONE;
		} else {
			log_msg = "upgrading";
			event = ALPM_PROGRESS_UPGRADE_START;
			start = ALPM_EVENT_UPGRADE_START;
			done = ALPM_EVENT_UPGRADE_DONE;
		}
		is_upgrade = 1;

		/* we'll need to save some record for backup checks later */
		if(_alpm_pkg_dup(local, &oldpkg) == -1) {
			ret = -1;
			goto cleanup;
		}

		/* copy over the install reason */
		newpkg->reason = alpm_pkg_get_reason(local);
	}

	EVENT(handle, start, newpkg, local);

	pkgfile = newpkg->origin_data.file;

	_alpm_log(handle, ALPM_LOG_DEBUG, "%s package %s-%s\n",
			log_msg, newpkg->name, newpkg->version);
		/* pre_install/pre_upgrade scriptlet */
	if(alpm_pkg_has_scriptlet(newpkg) &&
			!(trans->flags & ALPM_TRANS_FLAG_NOSCRIPTLET)) {
		const char *scriptlet_name = is_upgrade ? "pre_upgrade" : "pre_install";

		_alpm_runscriptlet(handle, pkgfile, scriptlet_name,
				newpkg->version, oldpkg ? oldpkg->version : NULL, 1);
	}

	/* we override any pre-set reason if we have alldeps or allexplicit set */
	if(trans->flags & ALPM_TRANS_FLAG_ALLDEPS) {
		newpkg->reason = ALPM_PKG_REASON_DEPEND;
	} else if(trans->flags & ALPM_TRANS_FLAG_ALLEXPLICIT) {
		newpkg->reason = ALPM_PKG_REASON_EXPLICIT;
	}

	if(oldpkg) {
		/* set up fake remove transaction */
		if(_alpm_remove_single_package(handle, oldpkg, newpkg, 0, 0) == -1) {
			handle->pm_errno = ALPM_ERR_TRANS_ABORT;
			ret = -1;
			goto cleanup;
		}
	}

	/* prepare directory for database entries so permission are correct after
	   changelog/install script installation (FS#12263) */
	if(_alpm_local_db_prepare(db, newpkg)) {
		alpm_logaction(handle, ALPM_CALLER_PREFIX,
				"error: could not create database entry %s-%s\n",
				newpkg->name, newpkg->version);
		handle->pm_errno = ALPM_ERR_DB_WRITE;
		ret = -1;
		goto cleanup;
	}

	if(!(trans->flags & ALPM_TRANS_FLAG_DBONLY)) {
		struct archive *archive;
		struct archive_entry *entry;
		struct stat buf;
		int fd, cwdfd;

		_alpm_log(handle, ALPM_LOG_DEBUG, "extracting files\n");

		fd = _alpm_open_archive(db->handle, pkgfile, &buf,
				&archive, ALPM_ERR_PKG_OPEN);
		if(fd < 0) {
			ret = -1;
			goto cleanup;
		}

		/* save the cwd so we can restore it later */
		OPEN(cwdfd, ".", O_RDONLY);
		if(cwdfd < 0) {
			_alpm_log(handle, ALPM_LOG_ERROR, _("could not get current working directory\n"));
		}

		/* libarchive requires this for extracting hard links */
		if(chdir(handle->root) != 0) {
			_alpm_log(handle, ALPM_LOG_ERROR, _("could not change directory to %s (%s)\n"),
					handle->root, strerror(errno));
			_alpm_archive_read_free(archive);
			CLOSE(fd);
			ret = -1;
			goto cleanup;
		}

		/* call PROGRESS once with 0 percent, as we sort-of skip that here */
		PROGRESS(handle, event, newpkg->name, 0, pkg_count, pkg_current);

		for(i = 0; archive_read_next_header(archive, &entry) == ARCHIVE_OK; i++) {
			int percent;

			if(newpkg->size != 0) {
				/* Using compressed size for calculations here, as newpkg->isize is not
				 * exact when it comes to comparing to the ACTUAL uncompressed size
				 * (missing metadata sizes) */
				int64_t pos = _alpm_archive_compressed_ftell(archive);
				percent = (pos * 100) / newpkg->size;
				if(percent >= 100) {
					percent = 100;
				}
			} else {
				percent = 0;
			}

			PROGRESS(handle, event, newpkg->name, percent, pkg_count, pkg_current);

			/* extract the next file from the archive */
			errors += extract_single_file(handle, archive, entry, newpkg, oldpkg);
		}
		_alpm_archive_read_free(archive);
		CLOSE(fd);

		/* restore the old cwd if we have it */
		if(cwdfd >= 0) {
			if(fchdir(cwdfd) != 0) {
				_alpm_log(handle, ALPM_LOG_ERROR,
						_("could not restore working directory (%s)\n"), strerror(errno));
			}
			CLOSE(cwdfd);
		}

		if(errors) {
			ret = -1;
			if(is_upgrade) {
				_alpm_log(handle, ALPM_LOG_ERROR, _("problem occurred while upgrading %s\n"),
						newpkg->name);
				alpm_logaction(handle, ALPM_CALLER_PREFIX,
						"error: problem occurred while upgrading %s\n",
						newpkg->name);
			} else {
				_alpm_log(handle, ALPM_LOG_ERROR, _("problem occurred while installing %s\n"),
						newpkg->name);
				alpm_logaction(handle, ALPM_CALLER_PREFIX,
						"error: problem occurred while installing %s\n",
						newpkg->name);
			}
		}
	}

	/* make an install date (in UTC) */
	newpkg->installdate = time(NULL);

	_alpm_log(handle, ALPM_LOG_DEBUG, "updating database\n");
	_alpm_log(handle, ALPM_LOG_DEBUG, "adding database entry '%s'\n", newpkg->name);

	if(_alpm_local_db_write(db, newpkg, INFRQ_ALL)) {
		_alpm_log(handle, ALPM_LOG_ERROR, _("could not update database entry %s-%s\n"),
				newpkg->name, newpkg->version);
		alpm_logaction(handle, ALPM_CALLER_PREFIX,
				"error: could not update database entry %s-%s\n",
				newpkg->name, newpkg->version);
		handle->pm_errno = ALPM_ERR_DB_WRITE;
		ret = -1;
		goto cleanup;
	}

	if(_alpm_db_add_pkgincache(db, newpkg) == -1) {
		_alpm_log(handle, ALPM_LOG_ERROR, _("could not add entry '%s' in cache\n"),
				newpkg->name);
	}

	PROGRESS(handle, event, newpkg->name, 100, pkg_count, pkg_current);

	/* run the post-install script if it exists  */
	if(alpm_pkg_has_scriptlet(newpkg)
			&& !(trans->flags & ALPM_TRANS_FLAG_NOSCRIPTLET)) {
		char *scriptlet = _alpm_local_db_pkgpath(db, newpkg, "install");
		const char *scriptlet_name = is_upgrade ? "post_upgrade" : "post_install";

		_alpm_runscriptlet(handle, scriptlet, scriptlet_name,
				newpkg->version, oldpkg ? oldpkg->version : NULL, 0);
		free(scriptlet);
	}

	EVENT(handle, done, newpkg, oldpkg);

cleanup:
	_alpm_pkg_free(oldpkg);
	return ret;
}

int _alpm_upgrade_packages(alpm_handle_t *handle)
{
	size_t pkg_count, pkg_current;
	int skip_ldconfig = 0, ret = 0;
	alpm_list_t *targ;
	alpm_trans_t *trans = handle->trans;

	if(trans->add == NULL) {
		return 0;
	}

	pkg_count = alpm_list_count(trans->add);
	pkg_current = 1;

	/* loop through our package list adding/upgrading one at a time */
	for(targ = trans->add; targ; targ = targ->next) {
		alpm_pkg_t *newpkg = targ->data;

		if(handle->trans->state == STATE_INTERRUPTED) {
			return ret;
		}

		if(commit_single_pkg(handle, newpkg, pkg_current, pkg_count)) {
			/* something screwed up on the commit, abort the trans */
			trans->state = STATE_INTERRUPTED;
			handle->pm_errno = ALPM_ERR_TRANS_ABORT;
			/* running ldconfig at this point could possibly screw system */
			skip_ldconfig = 1;
			ret = -1;
		}

		pkg_current++;
	}

	if(!skip_ldconfig) {
		/* run ldconfig if it exists */
		_alpm_ldconfig(handle);
	}

	return ret;
}

/* vim: set ts=2 sw=2 noet: */
