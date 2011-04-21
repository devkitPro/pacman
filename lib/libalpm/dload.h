/*
 *  dload.h
 *
 *  Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
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
#ifndef _ALPM_DLOAD_H
#define _ALPM_DLOAD_H

#include "alpm_list.h"
#include "alpm.h"

#include <time.h>

/* internal structure for communicating with curl progress callback */
struct fileinfo {
	const char *filename;
	double initial_size;
};

int _alpm_download(const char *url, const char *localpath,
		int force, int allow_resume, int errors_ok);

#endif /* _ALPM_DLOAD_H */

/* vim: set ts=2 sw=2 noet: */
