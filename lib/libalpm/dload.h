/*
 *  dload.h
 *
 *  Copyright (c) 2002-2008 by Judd Vinet <jvinet@zeroflux.org>
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
#ifndef _ALPM_DLOAD_H
#define _ALPM_DLOAD_H

#include "alpm_list.h"
#include "alpm.h"

#include <time.h>
#include <download.h>

#define PM_DLBUF_LEN (1024 * 10)

int _alpm_downloadfiles(alpm_list_t *servers, const char *localpath,
		alpm_list_t *files, int *dl_total, unsigned long totalsize);
int _alpm_downloadfiles_forreal(alpm_list_t *servers, const char *localpath,
	alpm_list_t *files, time_t mtime1, time_t *mtime2, int *dl_total,
	unsigned long totalsize);

#endif /* _ALPM_DLOAD_H */

/* vim: set ts=2 sw=2 noet: */
