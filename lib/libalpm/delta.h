/*
 *  delta.h
 *
 *  Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2007-2006 by Judd Vinet <jvinet@zeroflux.org>
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

#include "config.h" /* ensure off_t is correct length */

#include <sys/types.h> /* off_t */

#include "alpm.h"

pmdelta_t *_alpm_delta_parse(char *line);
void _alpm_delta_free(pmdelta_t *delta);
pmdelta_t *_alpm_delta_dup(const pmdelta_t *delta);
off_t _alpm_shortest_delta_path(pmhandle_t *handle, alpm_list_t *deltas,
		const char *to, alpm_list_t **path);

/* max percent of package size to download deltas */
#define MAX_DELTA_RATIO 0.7

#endif /* _ALPM_DELTA_H */

/* vim: set ts=2 sw=2 noet: */
