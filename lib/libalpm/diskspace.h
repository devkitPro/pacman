/*
 *  diskspace.h
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

#ifndef _ALPM_DISKSPACE_H
#define _ALPM_DISKSPACE_H

#if defined HAVE_GETMNTINFO_STATFS
#include <sys/mount.h>
#else
#include <sys/statvfs.h>
#endif

#include "alpm.h"

typedef struct __alpm_mountpoint_t {
	/* mount point information */
	char *mount_dir;
#if defined HAVE_GETMNTINFO_STATFS
	struct statfs *fsp;
#else
	struct statvfs *fsp;
#endif
	/* storage for additional disk usage calculations */
	long blocks_needed;
	long max_blocks_needed;
	int used;
} alpm_mountpoint_t;

int _alpm_check_diskspace(pmtrans_t *trans, pmdb_t *db);

#endif /* _ALPM_DISKSPACE_H */

/* vim: set ts=2 sw=2 noet: */
