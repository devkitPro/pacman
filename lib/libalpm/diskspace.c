/*
 *  diskspace.c
 *
 *  Copyright (c) 2010 Pacman Development Team <pacman-dev@archlinux.org>
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

#if defined HAVE_GETMNTENT
#include <mntent.h>
#include <sys/statvfs.h>
#elif defined HAVE_GETMNTINFO_STATFS
#include <sys/param.h>
#include <sys/mount.h>
#if HAVE_SYS_UCRED_H
#include <sys/ucred.h>
#endif
#elif defined HAVE_GETMNTINFO_STATVFS
#include <sys/types.h>
#include <sys/statvfs.h>
#endif

/* libalpm */
#include "diskspace.h"
#include "alpm_list.h"
#include "util.h"
#include "log.h"
#include "handle.h"

static int mount_point_cmp(const alpm_mountpoint_t *mp1, const alpm_mountpoint_t *mp2)
{
	return(strcmp(mp1->mount_dir, mp2->mount_dir));
}

static alpm_list_t *mount_point_list()
{
	alpm_list_t *mount_points = NULL;
	alpm_mountpoint_t *mp;

#if defined HAVE_GETMNTENT
	struct mntent *mnt;
	FILE *fp;
	struct statvfs fsp;

	fp = setmntent(MOUNTED, "r");

	if (fp == NULL) {
		return NULL;
	}

	while((mnt = getmntent (fp))) {
		if(statvfs(mnt->mnt_dir, &fsp) != 0) {
			_alpm_log(PM_LOG_WARNING, "could not get filesystem information for %s\n", mnt->mnt_dir);
			continue;
		}

		MALLOC(mp, sizeof(alpm_mountpoint_t), RET_ERR(PM_ERR_MEMORY, NULL));
		mp->mount_dir = strdup(mnt->mnt_dir);

		MALLOC(mp->fsp, sizeof(struct statvfs), RET_ERR(PM_ERR_MEMORY, NULL));
		memcpy((void *)(mp->fsp), (void *)(&fsp), sizeof(struct statvfs));

		mp->blocks_needed = 0;
		mp->max_blocks_needed = 0;
		mp->used = 0;

		mount_points = alpm_list_add(mount_points, mp);
	}

	endmntent(fp);
#elif defined HAVE_GETMNTINFO_STATFS
	int entries;
	struct statfs *fsp;

	entries = getmntinfo(&fsp, MNT_NOWAIT);

	if (entries < 0) {
		return NULL;
	}

	for(; entries-- > 0; fsp++) {
		MALLOC(mp, sizeof(alpm_mountpoint_t), RET_ERR(PM_ERR_MEMORY, NULL));
		mp->mount_dir = strdup(fsp->f_mntonname);

		MALLOC(mp->fsp, sizeof(struct statfs), RET_ERR(PM_ERR_MEMORY, NULL));
		memcpy((void *)(mp->fsp), (void *)fsp, sizeof(struct statfs));

		mp->blocks_needed = 0;
		mp->max_blocks_needed = 0;

		mount_points = alpm_list_add(mount_points, mp);
	}
#elif defined HAVE_GETMNTINFO_STATVFS
	int entries;
	struct statvfs *fsp;

	entries = getmntinfo(&fsp, MNT_NOWAIT);

	if (entries < 0) {
		return NULL;
	}

	for (; entries-- > 0; fsp++) {
		MALLOC(mp, sizeof(alpm_mountpoint_t), RET_ERR(PM_ERR_MEMORY, NULL));
		mp->mount_dir = strdup(fsp->f_mntonname);

		MALLOC(mp->fsp, sizeof(struct statvfs), RET_ERR(PM_ERR_MEMORY, NULL));
		memcpy((void *)(mp->fsp), (void *)fsp, sizeof(struct statvfs));

		mp->blocks_needed = 0;
		mp->max_blocks_needed = 0;

		mount_points = alpm_list_add(mount_points, mp);
	}
#endif

	mount_points = alpm_list_msort(mount_points, alpm_list_count(mount_points),
	                                   (alpm_list_fn_cmp)mount_point_cmp);
	return(mount_points);
}

static alpm_list_t *match_mount_point(const alpm_list_t *mount_points, const char *file)
{
	char real_path[PATH_MAX];
	snprintf(real_path, PATH_MAX, "%s%s", handle->root, file);

	alpm_list_t *mp = alpm_list_last(mount_points);
	do {
		alpm_mountpoint_t *data = mp->data;

		if(strncmp(data->mount_dir, real_path, strlen(data->mount_dir)) == 0) {
			return mp;
		}

		mp = mp->prev;
	} while (mp != alpm_list_last(mount_points));

	/* should not get here... */
	return NULL;
}

int _alpm_check_diskspace(pmtrans_t *trans, pmdb_t *db)
{
	alpm_list_t *mount_points;

	mount_points = mount_point_list();
	if(mount_points == NULL) {
		_alpm_log(PM_LOG_ERROR, _("count not determine filesystem mount points"));
		return -1;
	}

	for(i = mount_points; i; i = alpm_list_next(i)) {
		alpm_mountpoint_t *data = i->data;
		FREE(data->mount_dir);
		FREE(data->fsp);
	}
	FREELIST(mount_points);

	return 0;
}

/* vim: set ts=2 sw=2 noet: */
