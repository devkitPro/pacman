/*
 *  conflict.c
 *
 *  Copyright (c) 2006-2013 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2006 by David Kimpe <dnaku@frugalware.org>
 *  Copyright (c) 2006 by Miklos Vajna <vmiklos@frugalware.org>
 *  Copyright (c) 2006 by Christian Hamar <krics@linuxforum.hu>
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
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <dirent.h>

/* libalpm */
#include "conflict.h"
#include "alpm_list.h"
#include "alpm.h"
#include "handle.h"
#include "trans.h"
#include "util.h"
#include "log.h"
#include "deps.h"
#include "filelist.h"

static alpm_conflict_t *conflict_new(alpm_pkg_t *pkg1, alpm_pkg_t *pkg2,
		alpm_depend_t *reason)
{
	alpm_conflict_t *conflict;

	MALLOC(conflict, sizeof(alpm_conflict_t), return NULL);

	conflict->package1_hash = pkg1->name_hash;
	conflict->package2_hash = pkg2->name_hash;
	STRDUP(conflict->package1, pkg1->name, return NULL);
	STRDUP(conflict->package2, pkg2->name, return NULL);
	conflict->reason = reason;

	return conflict;
}

void _alpm_conflict_free(alpm_conflict_t *conflict)
{
	FREE(conflict->package2);
	FREE(conflict->package1);
	FREE(conflict);
}

alpm_conflict_t *_alpm_conflict_dup(const alpm_conflict_t *conflict)
{
	alpm_conflict_t *newconflict;
	CALLOC(newconflict, 1, sizeof(alpm_conflict_t), return NULL);

	newconflict->package1_hash = conflict->package1_hash;
	newconflict->package2_hash = conflict->package2_hash;
	STRDUP(newconflict->package1, conflict->package1, return NULL);
	STRDUP(newconflict->package2, conflict->package2, return NULL);
	newconflict->reason = conflict->reason;

	return newconflict;
}

static int conflict_isin(alpm_conflict_t *needle, alpm_list_t *haystack)
{
	alpm_list_t *i;
	for(i = haystack; i; i = i->next) {
		alpm_conflict_t *conflict = i->data;
		if(needle->package1_hash == conflict->package1_hash
				&& needle->package2_hash == conflict->package2_hash
				&& strcmp(needle->package1, conflict->package1) == 0
				&& strcmp(needle->package2, conflict->package2) == 0) {
			return 1;
		}
	}

	return 0;
}

/** Adds the pkg1/pkg2 conflict to the baddeps list.
 * @param handle the context handle
 * @param baddeps list to add conflict to
 * @param pkg1 first package
 * @param pkg2 package causing conflict
 * @param reason reason for this conflict
 */
static int add_conflict(alpm_handle_t *handle, alpm_list_t **baddeps,
		alpm_pkg_t *pkg1, alpm_pkg_t *pkg2, alpm_depend_t *reason)
{
	alpm_conflict_t *conflict = conflict_new(pkg1, pkg2, reason);
	if(!conflict) {
		return -1;
	}
	if(!conflict_isin(conflict, *baddeps)) {
		char *conflict_str = alpm_dep_compute_string(reason);
		*baddeps = alpm_list_add(*baddeps, conflict);
		_alpm_log(handle, ALPM_LOG_DEBUG, "package %s conflicts with %s (by %s)\n",
				pkg1->name, pkg2->name, conflict_str);
		free(conflict_str);
	} else {
		_alpm_conflict_free(conflict);
	}
	return 0;
}

/** Check if packages from list1 conflict with packages from list2.
 * This looks at the conflicts fields of all packages from list1, and sees
 * if they match packages from list2.
 * If a conflict (pkg1, pkg2) is found, it is added to the baddeps list
 * in this order if order >= 0, or reverse order (pkg2,pkg1) otherwise.
 *
 * @param handle the context handle
 * @param list1 first list of packages
 * @param list2 second list of packages
 * @param baddeps list to store conflicts
 * @param order if >= 0 the conflict order is preserved, if < 0 it's reversed
 */
static void check_conflict(alpm_handle_t *handle,
		alpm_list_t *list1, alpm_list_t *list2,
		alpm_list_t **baddeps, int order)
{
	alpm_list_t *i;

	if(!baddeps) {
		return;
	}
	for(i = list1; i; i = i->next) {
		alpm_pkg_t *pkg1 = i->data;
		alpm_list_t *j;

		for(j = alpm_pkg_get_conflicts(pkg1); j; j = j->next) {
			alpm_depend_t *conflict = j->data;
			alpm_list_t *k;

			for(k = list2; k; k = k->next) {
				alpm_pkg_t *pkg2 = k->data;

				if(pkg1->name_hash == pkg2->name_hash
						&& strcmp(pkg1->name, pkg2->name) == 0) {
					/* skip the package we're currently processing */
					continue;
				}

				if(_alpm_depcmp(pkg2, conflict)) {
					if(order >= 0) {
						add_conflict(handle, baddeps, pkg1, pkg2, conflict);
					} else {
						add_conflict(handle, baddeps, pkg2, pkg1, conflict);
					}
				}
			}
		}
	}
}

/* Check for inter-conflicts */
alpm_list_t *_alpm_innerconflicts(alpm_handle_t *handle, alpm_list_t *packages)
{
	alpm_list_t *baddeps = NULL;

	_alpm_log(handle, ALPM_LOG_DEBUG, "check targets vs targets\n");
	check_conflict(handle, packages, packages, &baddeps, 0);

	return baddeps;
}

/* Check for target vs (db - target) conflicts */
alpm_list_t *_alpm_outerconflicts(alpm_db_t *db, alpm_list_t *packages)
{
	alpm_list_t *baddeps = NULL;

	if(db == NULL) {
		return NULL;
	}

	alpm_list_t *dblist = alpm_list_diff(_alpm_db_get_pkgcache(db),
			packages, _alpm_pkg_cmp);

	/* two checks to be done here for conflicts */
	_alpm_log(db->handle, ALPM_LOG_DEBUG, "check targets vs db\n");
	check_conflict(db->handle, packages, dblist, &baddeps, 1);
	_alpm_log(db->handle, ALPM_LOG_DEBUG, "check db vs targets\n");
	check_conflict(db->handle, dblist, packages, &baddeps, -1);

	alpm_list_free(dblist);
	return baddeps;
}

/** Check the package conflicts in a database
 *
 * @param handle the context handle
 * @param pkglist the list of packages to check
 * @return an alpm_list_t of alpm_conflict_t
 */
alpm_list_t SYMEXPORT *alpm_checkconflicts(alpm_handle_t *handle,
		alpm_list_t *pkglist)
{
	CHECK_HANDLE(handle, return NULL);
	return _alpm_innerconflicts(handle, pkglist);
}

/* Adds alpm_fileconflict_t to a conflicts list. Pass the conflicts list, the
 * conflicting file path, and either two packages or one package and NULL.
 */
static alpm_list_t *add_fileconflict(alpm_handle_t *handle,
		alpm_list_t *conflicts, const char *filestr,
		alpm_pkg_t *pkg1, alpm_pkg_t *pkg2)
{
	alpm_fileconflict_t *conflict;
	MALLOC(conflict, sizeof(alpm_fileconflict_t), goto error);

	STRDUP(conflict->target, pkg1->name, goto error);
	STRDUP(conflict->file, filestr, goto error);
	if(pkg2) {
		conflict->type = ALPM_FILECONFLICT_TARGET;
		STRDUP(conflict->ctarget, pkg2->name, goto error);
	} else {
		conflict->type = ALPM_FILECONFLICT_FILESYSTEM;
		STRDUP(conflict->ctarget, "", goto error);
	}

	conflicts = alpm_list_add(conflicts, conflict);
	_alpm_log(handle, ALPM_LOG_DEBUG, "found file conflict %s, packages %s and %s\n",
	          filestr, pkg1->name, pkg2 ? pkg2->name : "(filesystem)");

	return conflicts;

error:
	RET_ERR(handle, ALPM_ERR_MEMORY, conflicts);
}

void _alpm_fileconflict_free(alpm_fileconflict_t *conflict)
{
	FREE(conflict->ctarget);
	FREE(conflict->file);
	FREE(conflict->target);
	FREE(conflict);
}

static int dir_belongsto_pkg(alpm_handle_t *handle, const char *dirpath,
		alpm_pkg_t *pkg)
{
	alpm_list_t *i;
	struct stat sbuf;
	char path[PATH_MAX];
	char abspath[PATH_MAX];
	DIR *dir;
	struct dirent *ent = NULL;
	const char *root = handle->root;

	/* check directory is actually in package - used for subdirectory checks */
	if(!alpm_filelist_contains(alpm_pkg_get_files(pkg), dirpath)) {
		_alpm_log(handle, ALPM_LOG_DEBUG,
				"directory %s not in package %s\n", dirpath, pkg->name);
		return 0;
	}

	/* TODO: this is an overly strict check but currently pacman will not
	 * overwrite a directory with a file (case 10/11 in add.c). Adjusting that
	 * is not simple as even if the directory is being unowned by a conflicting
	 * package, pacman does not sort this to ensure all required directory
	 * "removals" happen before installation of file/symlink */

	/* check that no other _installed_ package owns the directory */
	for(i = _alpm_db_get_pkgcache(handle->db_local); i; i = i->next) {
		if(pkg == i->data) {
			continue;
		}

		if(alpm_filelist_contains(alpm_pkg_get_files(i->data), dirpath)) {
			_alpm_log(handle, ALPM_LOG_DEBUG,
					"file %s also in package %s\n", dirpath,
					((alpm_pkg_t*)i->data)->name);
			return 0;
		}
	}

	/* check all files in directory are owned by the package */
	snprintf(abspath, PATH_MAX, "%s%s", root, dirpath);
	dir = opendir(abspath);
	if(dir == NULL) {
		return 1;
	}

	while((ent = readdir(dir)) != NULL) {
		const char *name = ent->d_name;

		if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
			continue;
		}
		snprintf(path, PATH_MAX, "%s%s", dirpath, name);
		snprintf(abspath, PATH_MAX, "%s%s", root, path);
		if(stat(abspath, &sbuf) != 0) {
			continue;
		}
		if(S_ISDIR(sbuf.st_mode)) {
			if(dir_belongsto_pkg(handle, path, pkg)) {
				continue;
			} else {
				closedir(dir);
				return 0;
			}
		} else {
			if(alpm_filelist_contains(alpm_pkg_get_files(pkg), path)) {
				continue;
			} else {
				closedir(dir);
				_alpm_log(handle, ALPM_LOG_DEBUG,
						"unowned file %s found in directory\n", path);
				return 0;
			}
		}
	}
	closedir(dir);
	return 1;
}

/* Find file conflicts that may occur during the transaction with two checks:
 * 1: check every target against every target
 * 2: check every target against the filesystem */
alpm_list_t *_alpm_db_find_fileconflicts(alpm_handle_t *handle,
		alpm_list_t *upgrade, alpm_list_t *rem)
{
	alpm_list_t *i, *conflicts = NULL;
	size_t numtargs = alpm_list_count(upgrade);
	size_t current;
	size_t rootlen;

	if(!upgrade) {
		return NULL;
	}

	rootlen = strlen(handle->root);

	/* TODO this whole function needs a huge change, which hopefully will
	 * be possible with real transactions. Right now we only do half as much
	 * here as we do when we actually extract files in add.c with our 12
	 * different cases. */
	for(current = 0, i = upgrade; i; i = i->next, current++) {
		alpm_pkg_t *p1 = i->data;
		alpm_list_t *j;
		alpm_filelist_t tmpfiles;
		alpm_pkg_t *dbpkg;
		size_t filenum;

		int percent = (current * 100) / numtargs;
		PROGRESS(handle, ALPM_PROGRESS_CONFLICTS_START, "", percent,
		         numtargs, current);

		_alpm_filelist_resolve(handle, alpm_pkg_get_files(p1));

		/* CHECK 1: check every target against every target */
		_alpm_log(handle, ALPM_LOG_DEBUG, "searching for file conflicts: %s\n",
				p1->name);
		for(j = i->next; j; j = j->next) {
			alpm_list_t *common_files;
			alpm_pkg_t *p2 = j->data;
			_alpm_filelist_resolve(handle, alpm_pkg_get_files(p2));

			common_files = _alpm_filelist_intersection(alpm_pkg_get_files(p1),
					alpm_pkg_get_files(p2));

			if(common_files) {
				alpm_list_t *k;
				char path[PATH_MAX];
				for(k = common_files; k; k = k->next) {
					alpm_file_t *file = k->data;
					snprintf(path, PATH_MAX, "%s%s", handle->root, file->name);
					conflicts = add_fileconflict(handle, conflicts, path, p1, p2);
					if(handle->pm_errno == ALPM_ERR_MEMORY) {
						FREELIST(conflicts);
						FREELIST(common_files);
						return NULL;
					}
				}
				alpm_list_free(common_files);
			}
		}

		/* CHECK 2: check every target against the filesystem */
		_alpm_log(handle, ALPM_LOG_DEBUG, "searching for filesystem conflicts: %s\n",
				p1->name);
		dbpkg = _alpm_db_get_pkgfromcache(handle->db_local, p1->name);
		_alpm_filelist_resolve(handle, alpm_pkg_get_files(dbpkg));

		/* Do two different checks here. If the package is currently installed,
		 * then only check files that are new in the new package. If the package
		 * is not currently installed, then simply stat the whole filelist. Note
		 * that the former list needs to be freed while the latter list should NOT
		 * be freed. */
		if(dbpkg) {
			alpm_list_t *difference;
			/* older ver of package currently installed */
			difference = _alpm_filelist_difference(alpm_pkg_get_files(p1),
					alpm_pkg_get_files(dbpkg));
			tmpfiles.count = alpm_list_count(difference);
			tmpfiles.files = alpm_list_to_array(difference, tmpfiles.count,
					sizeof(alpm_file_t));
			alpm_list_free(difference);
		} else {
			/* no version of package currently installed */
			tmpfiles = *alpm_pkg_get_files(p1);
		}

		for(filenum = 0; filenum < tmpfiles.count; filenum++) {
			alpm_file_t *file = tmpfiles.files + filenum;
			const char *filestr = file->name;
			const char *relative_path;
			alpm_list_t *k;
			/* have we acted on this conflict? */
			int resolved_conflict = 0;
			struct stat lsbuf;
			char path[PATH_MAX];
			size_t pathlen;

			pathlen = snprintf(path, PATH_MAX, "%s%s", handle->root, filestr);

			/* stat the file - if it exists, do some checks */
			if(_alpm_lstat(path, &lsbuf) != 0) {
				continue;
			}

			_alpm_log(handle, ALPM_LOG_DEBUG, "checking possible conflict: %s\n", path);

			if(S_ISDIR(file->mode)) {
				struct stat sbuf;
				if(S_ISDIR(lsbuf.st_mode)) {
					_alpm_log(handle, ALPM_LOG_DEBUG, "file is a directory, not a conflict\n");
					continue;
				}
				stat(path, &sbuf);
				if(S_ISLNK(lsbuf.st_mode) && S_ISDIR(sbuf.st_mode)) {
					_alpm_log(handle, ALPM_LOG_DEBUG,
							"file is a symlink to a dir, hopefully not a conflict\n");
					continue;
				}
				/* if we made it to here, we want all subsequent path comparisons to
				 * not include the trailing slash. This allows things like file ->
				 * directory replacements. */
				path[pathlen - 1] = '\0';
			}

			relative_path = path + rootlen;

			/* Check remove list (will we remove the conflicting local file?) */
			for(k = rem; k && !resolved_conflict; k = k->next) {
				alpm_pkg_t *rempkg = k->data;
				if(rempkg && alpm_filelist_contains(alpm_pkg_get_files(rempkg),
							relative_path)) {
					_alpm_log(handle, ALPM_LOG_DEBUG,
							"local file will be removed, not a conflict\n");
					resolved_conflict = 1;
				}
			}

			/* Look at all the targets to see if file has changed hands */
			for(k = upgrade; k && !resolved_conflict; k = k->next) {
				alpm_pkg_t *p2 = k->data;
				if(!p2 || strcmp(p1->name, p2->name) == 0) {
					continue;
				}
				alpm_pkg_t *localp2 = _alpm_db_get_pkgfromcache(handle->db_local, p2->name);

				/* localp2->files will be removed (target conflicts are handled by CHECK 1) */
				if(localp2 && alpm_filelist_contains(alpm_pkg_get_files(localp2), filestr)) {
					/* skip removal of file, but not add. this will prevent a second
					 * package from removing the file when it was already installed
					 * by its new owner (whether the file is in backup array or not */
					handle->trans->skip_remove =
						alpm_list_add(handle->trans->skip_remove, strdup(filestr));
					_alpm_log(handle, ALPM_LOG_DEBUG,
							"file changed packages, adding to remove skiplist\n");
					resolved_conflict = 1;
				}
			}

			/* check if all files of the dir belong to the installed pkg */
			if(!resolved_conflict && S_ISDIR(lsbuf.st_mode) && dbpkg) {
				char *dir = malloc(strlen(filestr) + 2);
				sprintf(dir, "%s/", filestr);
				if(alpm_filelist_contains(alpm_pkg_get_files(dbpkg), dir)) {
					_alpm_log(handle, ALPM_LOG_DEBUG,
							"checking if all files in %s belong to %s\n",
							dir, dbpkg->name);
					resolved_conflict = dir_belongsto_pkg(handle, dir, dbpkg);
				}
				free(dir);
			}

			/* check if a component of the filepath was a link. canonicalize the path
			 * and look for it in the old package. note that the actual file under
			 * consideration cannot itself be a link, as it might be unowned- path
			 * components can be safely checked as all directories are "unowned". */
			if(!resolved_conflict && dbpkg && !S_ISLNK(lsbuf.st_mode)) {
				char rpath[PATH_MAX];
				if(realpath(path, rpath)) {
					const char *relative_rpath = rpath + rootlen;
					if(alpm_filelist_contains(alpm_pkg_get_files(dbpkg), relative_rpath)) {
						_alpm_log(handle, ALPM_LOG_DEBUG,
								"package contained the resolved realpath\n");
						resolved_conflict = 1;
					}
				}
			}

			/* is the file unowned and in the backup list of the new package? */
			if(!resolved_conflict && _alpm_needbackup(filestr, p1)) {
				alpm_list_t *local_pkgs = _alpm_db_get_pkgcache(handle->db_local);
				int found = 0;
				for(k = local_pkgs; k && !found; k = k->next) {
					if(alpm_filelist_contains(alpm_pkg_get_files(k->data), filestr)) {
							found = 1;
					}
				}
				if(!found) {
					_alpm_log(handle, ALPM_LOG_DEBUG,
							"file was unowned but in new backup list\n");
					resolved_conflict = 1;
				}
			}

			if(!resolved_conflict) {
				conflicts = add_fileconflict(handle, conflicts, path, p1, NULL);
				if(handle->pm_errno == ALPM_ERR_MEMORY) {
					FREELIST(conflicts);
					if(dbpkg) {
						/* only freed if it was generated from _alpm_filelist_difference() */
						free(tmpfiles.files);
					}
					return NULL;
				}
			}
		}
		if(dbpkg) {
			/* only freed if it was generated from _alpm_filelist_difference() */
			free(tmpfiles.files);
		}
	}
	PROGRESS(handle, ALPM_PROGRESS_CONFLICTS_START, "", 100,
			numtargs, current);

	return conflicts;
}

/* vim: set ts=2 sw=2 noet: */
