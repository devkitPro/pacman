/*
 *  delta.h
 *
 *  Copyright (c) 2007-2008 by Judd Vinet <jvinet@zeroflux.org>
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
#ifndef _ALPM_DELTA_H
#define _ALPM_DELTA_H

#include "alpm.h"

struct __pmdelta_t {
	char *from;
	char *to;
	unsigned long size;
	char *filename;
	char *md5sum;
};

unsigned long _alpm_delta_path_size(alpm_list_t *deltas);
unsigned long _alpm_delta_path_size_uncached(alpm_list_t *deltas);
pmdelta_t *_alpm_delta_parse(char *line);
void _alpm_delta_free(pmdelta_t *delta);
alpm_list_t *_alpm_shortest_delta_path(alpm_list_t *deltas,
		const char *from, const char *to);

#endif /* _ALPM_DELTA_H */

/* vim: set ts=2 sw=2 noet: */
