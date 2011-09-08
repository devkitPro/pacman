/*
 *  diskspace.c
 *
 *  Copyright (c) 2010-2011 Pacman Development Team <pacman-dev@archlinux.org>
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

#include <errno.h>
#if defined(HAVE_MNTENT_H)
#include <mntent.h>
#endif
#if defined(HAVE_SYS_STATVFS_H)
#include <sys/statvfs.h>
#endif
#if defined(HAVE_SYS_PARAM_H)
#include <sys/param.h>
#endif
#if defined(HAVE_SYS_MOUNT_H)
#include <sys/mount.h>
#endif
#if defined(HAVE_SYS_UCRED_H)
#include <sys/ucred.h>
#endif
#if defined(HAVE_SYS_TYPES_H)
#include <sys/types.h>
#endif

/* libalpm */
#include "diskspace.h"
#include "alpm_list.h"
#include "util.h"
#include "log.h"
#include "trans.h"
#include "handle.h"

static int mount_point_cmp(const void *p1, const void *p2)
{
	const alpm_mountpoint_t *mp1 = p1;
	const alpm_mountpoint_t *mp2 = p2;
	/* the negation will sort all mountpoints before their parent */
	return -strcmp(mp1->mount_dir, mp2->mount_dir);
}

static alpm_list_t *mount_point_list(alpm_handle_t *handle)
{
	alpm_list_t *mount_points = NULL, *ptr;
	alpm_mountpoint_t *mp;

#if defined HAVE_GETMNTENT
	struct mntent *mnt;
	FILE *fp;
	struct statvfs fsp;

	fp = setmntent(MOUNTED, "r");

	if(fp == NULL) {
		return NULL;
	}

	while((mnt = getmntent(fp))) {
		if(!mnt) {
			_alpm_log(handle, ALPM_LOG_WARNING, _("could not get filesystem information\n"));
			continue;
		}
		if(statvfs(mnt->mnt_dir, &fsp) != 0) {
			_alpm_log(handle, ALPM_LOG_WARNING,
					_("could not get filesystem information for %s: %s\n"),
					mnt->mnt_dir, strerror(errno));
			continue;
		}

		CALLOC(mp, 1, sizeof(alpm_mountpoint_t), RET_ERR(handle, ALPM_ERR_MEMORY, NULL));
		mp->mount_dir = strdup(mnt->mnt_dir);
		mp->mount_dir_len = strlen(mp->mount_dir);
		memcpy(&(mp->fsp), &fsp, sizeof(struct statvfs));
		mp->read_only = fsp.f_flag & ST_RDONLY;

		mount_points = alpm_list_add(mount_points, mp);
	}

	endmntent(fp);
#elif defined HAVE_GETMNTINFO
	int entries;
	FSSTATSTYPE *fsp;

	entries = getmntinfo(&fsp, MNT_NOWAIT);

	if(entries < 0) {
		return NULL;
	}

	for(; entries-- > 0; fsp++) {
		CALLOC(mp, 1, sizeof(alpm_mountpoint_t), RET_ERR(handle, ALPM_ERR_MEMORY, NULL));
		mp->mount_dir = strdup(fsp->f_mntonname);
		mp->mount_dir_len = strlen(mp->mount_dir);
		memcpy(&(mp->fsp), fsp, sizeof(FSSTATSTYPE));
#if defined(HAVE_GETMNTINFO_STATVFS) && defined(HAVE_STRUCT_STATVFS_F_FLAG)
		mp->read_only = fsp->f_flag & ST_RDONLY;
#elif defined(HAVE_GETMNTINFO_STATFS) && defined(HAVE_STRUCT_STATFS_F_FLAGS)
		mp->read_only = fsp->f_flags & MNT_RDONLY;
#endif

		mount_points = alpm_list_add(mount_points, mp);
	}
#endif

	mount_points = alpm_list_msort(mount_points, alpm_list_count(mount_points),
			mount_point_cmp);
	for(ptr = mount_points; ptr != NULL; ptr = ptr->next) {
		mp = ptr->data;
		_alpm_log(handle, ALPM_LOG_DEBUG, "mountpoint: %s\n", mp->mount_dir);
	}
	return mount_points;
}

static alpm_mountpoint_t *match_mount_point(const alpm_list_t *mount_points,
		const char *real_path)
{
	const alpm_list_t *mp;

	for(mp = mount_points; mp != NULL; mp = mp->next) {
		alpm_mountpoint_t *data = mp->data;

		if(strncmp(data->mount_dir, real_path, data->mount_dir_len) == 0) {
			return data;
		}
	}

	/* should not get here... */
	return NULL;
}

static int calculate_removed_size(alpm_handle_t *handle,
		const alpm_list_t *mount_points, alpm_pkg_t *pkg)
{
	size_t i;
	alpm_filelist_t *filelist = alpm_pkg_get_files(pkg);

	if(!filelist->count) {
		return 0;
	}

	for(i = 0; i < filelist->count; i++) {
		const alpm_file_t *file = filelist->files + i;
		alpm_mountpoint_t *mp;
		struct stat st;
		char path[PATH_MAX];
		const char *filename = file->name;

		snprintf(path, PATH_MAX, "%s%s", handle->root, filename);
		_alpm_lstat(path, &st);

		/* skip directories and symlinks to be consistent with libarchive that
		 * reports them to be zero size */
		if(S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode)) {
			continue;
		}

		mp = match_mount_point(mount_points, path);
		if(mp == NULL) {
			_alpm_log(handle, ALPM_LOG_WARNING,
					_("could not determine mount point for file %s\n"), filename);
			continue;
		}

		/* the addition of (divisor - 1) performs ceil() with integer division */
		mp->blocks_needed -=
			(st.st_size + mp->fsp.f_bsize - 1) / mp->fsp.f_bsize;
		mp->used |= USED_REMOVE;
	}

	return 0;
}

static int calculate_installed_size(alpm_handle_t *handle,
		const alpm_list_t *mount_points, alpm_pkg_t *pkg)
{
	size_t i;
	alpm_filelist_t *filelist = alpm_pkg_get_files(pkg);

	if(!filelist->count) {
		return 0;
	}

	for(i = 0; i < filelist->count; i++) {
		const alpm_file_t *file = filelist->files + i;
		alpm_mountpoint_t *mp;
		char path[PATH_MAX];
		const char *filename = file->name;

		/* libarchive reports these as zero size anyways */
		/* NOTE: if we do start accounting for directory size, a dir matching a
		 * mountpoint needs to be attributed to the parent, not the mountpoint. */
		if(S_ISDIR(file->mode) || S_ISLNK(file->mode)) {
			continue;
		}

		/* approximate space requirements for db entries */
		if(filename[0] == '.') {
			filename = handle->dbpath;
		}

		snprintf(path, PATH_MAX, "%s%s", handle->root, filename);

		mp = match_mount_point(mount_points, path);
		if(mp == NULL) {
			_alpm_log(handle, ALPM_LOG_WARNING,
					_("could not determine mount point for file %s\n"), filename);
			continue;
		}

		/* the addition of (divisor - 1) performs ceil() with integer division */
		mp->blocks_needed +=
			(file->size + mp->fsp.f_bsize - 1) / mp->fsp.f_bsize;
		mp->used |= USED_INSTALL;
	}

	return 0;
}

int _alpm_check_diskspace(alpm_handle_t *handle)
{
	alpm_list_t *mount_points, *i;
	alpm_mountpoint_t *root_mp;
	size_t replaces = 0, current = 0, numtargs;
	int error = 0;
	alpm_list_t *targ;
	alpm_trans_t *trans = handle->trans;

	numtargs = alpm_list_count(trans->add);
	mount_points = mount_point_list(handle);
	if(mount_points == NULL) {
		_alpm_log(handle, ALPM_LOG_ERROR, _("could not determine filesystem mount points\n"));
		return -1;
	}
	root_mp = match_mount_point(mount_points, handle->root);
	if(root_mp == NULL) {
		_alpm_log(handle, ALPM_LOG_ERROR, _("could not determine root mount point %s\n"),
				handle->root);
		return -1;
	}

	replaces = alpm_list_count(trans->remove);
	if(replaces) {
		numtargs += replaces;
		for(targ = trans->remove; targ; targ = targ->next, current++) {
			alpm_pkg_t *local_pkg;
			int percent = (current * 100) / numtargs;
			PROGRESS(handle, ALPM_PROGRESS_DISKSPACE_START, "", percent,
					numtargs, current);

			local_pkg = targ->data;
			calculate_removed_size(handle, mount_points, local_pkg);
		}
	}

	for(targ = trans->add; targ; targ = targ->next, current++) {
		alpm_pkg_t *pkg, *local_pkg;
		int percent = (current * 100) / numtargs;
		PROGRESS(handle, ALPM_PROGRESS_DISKSPACE_START, "", percent,
				numtargs, current);

		pkg = targ->data;
		/* is this package already installed? */
		local_pkg = _alpm_db_get_pkgfromcache(handle->db_local, pkg->name);
		if(local_pkg) {
			calculate_removed_size(handle, mount_points, local_pkg);
		}
		calculate_installed_size(handle, mount_points, pkg);

		for(i = mount_points; i; i = i->next) {
			alpm_mountpoint_t *data = i->data;
			if(data->blocks_needed > data->max_blocks_needed) {
				data->max_blocks_needed = data->blocks_needed;
			}
		}
	}

	PROGRESS(handle, ALPM_PROGRESS_DISKSPACE_START, "", 100,
			numtargs, current);

	for(i = mount_points; i; i = i->next) {
		alpm_mountpoint_t *data = i->data;
		if(data->used && data->read_only) {
			_alpm_log(handle, ALPM_LOG_ERROR, _("Partition %s is mounted read only\n"),
					data->mount_dir);
			error = 1;
		} else if(data->used & USED_INSTALL) {
			/* cushion is roughly min(5% capacity, 20MiB) */
			fsblkcnt_t fivepc = (data->fsp.f_blocks / 20) + 1;
			fsblkcnt_t twentymb = (20 * 1024 * 1024 / data->fsp.f_bsize) + 1;
			fsblkcnt_t cushion = fivepc < twentymb ? fivepc : twentymb;
			blkcnt_t needed = data->max_blocks_needed + cushion;

			_alpm_log(handle, ALPM_LOG_DEBUG,
					"partition %s, needed %jd, cushion %ju, free %ju\n",
					data->mount_dir, (intmax_t)data->max_blocks_needed,
					(uintmax_t)cushion, (uintmax_t)data->fsp.f_bfree);
			if(needed >= 0 && (fsblkcnt_t)needed > data->fsp.f_bfree) {
				_alpm_log(handle, ALPM_LOG_ERROR,
						_("Partition %s too full: %jd blocks needed, %jd blocks free\n"),
						data->mount_dir, (intmax_t)needed, (uintmax_t)data->fsp.f_bfree);
				error = 1;
			}
		}
	}

	for(i = mount_points; i; i = i->next) {
		alpm_mountpoint_t *data = i->data;
		FREE(data->mount_dir);
	}
	FREELIST(mount_points);

	if(error) {
		RET_ERR(handle, ALPM_ERR_DISK_SPACE, -1);
	}

	return 0;
}

/* vim: set ts=2 sw=2 noet: */
