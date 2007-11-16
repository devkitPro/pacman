/*
 *  delta.c
 *
 *  Copyright (c) 2007 by Judd Vinet <jvinet@zeroflux.org>
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

#include "config.h"

#include <stdlib.h>
#include <string.h>

/* libalpm */
#include "delta.h"
#include "error.h"
#include "util.h"
#include "log.h"
#include "alpm_list.h"
#include "alpm.h"

/** \addtogroup alpm_deltas Delta Functions
 * @brief Functions to manipulate libalpm deltas
 * @{
 */

const char SYMEXPORT *alpm_delta_get_from(pmdelta_t *delta)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(delta != NULL, return(NULL));

	return(delta->from);
}

const char SYMEXPORT *alpm_delta_get_to(pmdelta_t *delta)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(delta != NULL, return(NULL));

	return(delta->to);
}

unsigned long SYMEXPORT alpm_delta_get_size(pmdelta_t *delta)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(delta != NULL, return(-1));

	return(delta->size);
}

const char SYMEXPORT *alpm_delta_get_filename(pmdelta_t *delta)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(delta != NULL, return(NULL));

	return(delta->filename);
}

const char SYMEXPORT *alpm_delta_get_md5sum(pmdelta_t *delta)
{
	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(delta != NULL, return(NULL));

	return(delta->md5sum);
}

/** @} */

/** Calculates the combined size of a list of delta files.
 *
 * @param deltas the list of pmdelta_t * objects
 *
 * @return the combined size
 */
unsigned long _alpm_delta_path_size(alpm_list_t *deltas)
{
	unsigned long sum = 0;
	alpm_list_t *dlts = deltas;

	while(dlts) {
		pmdelta_t *d = (pmdelta_t *)alpm_list_getdata(dlts);
		sum += d->size;

		dlts = alpm_list_next(dlts);
	}

	return(sum);
}

/** Calculates the combined size of a list of delta files that are not
 * in the cache.
 *
 * @param deltas the list of pmdelta_t * objects
 *
 * @return the combined size
 */
unsigned long _alpm_delta_path_size_uncached(alpm_list_t *deltas)
{
	unsigned long sum = 0;
	alpm_list_t *dlts = deltas;

	while(dlts) {
		pmdelta_t *d = (pmdelta_t *)alpm_list_getdata(dlts);
		char *fname = _alpm_filecache_find(d->filename);

		if(!fname) {
			sum += d->size;
		}

		FREE(fname);

		dlts = alpm_list_next(dlts);
	}

	return(sum);
}

/** Calculates the shortest path from one version to another.
 *
 * The shortest path is defined as the path with the smallest combined
 * size, not the length of the path.
 *
 * The algorithm is based on Dijkstra's shortest path algorithm.
 *
 * @param deltas the list of pmdelta_t * objects that a package has
 * @param from the version to start from
 * @param to the version to end at
 * @param path the current path
 *
 * @return the list of pmdelta_t * objects that has the smallest size.
 * NULL (the empty list) is returned if there is no path between the
 * versions.
 */
static alpm_list_t *shortest_delta_path(alpm_list_t *deltas,
		const char *from, const char *to, alpm_list_t *path)
{
	alpm_list_t *d;
	alpm_list_t *shortest = NULL;

	/* Found the 'to' version, this is a good path so return it. */
	if(strcmp(from, to) == 0) {
		return(path);
	}

	for(d = deltas; d; d = alpm_list_next(d)) {
		pmdelta_t *v = alpm_list_getdata(d);

		/* If this vertex has already been visited in the path, go to the
		 * next vertex. */
		if(alpm_list_find_ptr(path, v))
			continue;

		/* Once we find a vertex that starts at the 'from' version,
		 * recursively find the shortest path using the 'to' version of this
		 * current vertex as the 'from' version in the function call. */
		if(strcmp(v->from, from) == 0) {
			alpm_list_t *newpath = alpm_list_copy(path);
			alpm_list_free(path);
			newpath = alpm_list_add(newpath, v);
			newpath = shortest_delta_path(deltas, v->to, to, newpath);

			if(newpath != NULL) {
				/* The path returned works, now use it unless there is already a
				 * shorter path found. */
				if(shortest == NULL) {
					shortest = newpath;
				} else if(_alpm_delta_path_size(shortest) > _alpm_delta_path_size(newpath)) {
					alpm_list_free(shortest);
					shortest = newpath;
				} else {
					alpm_list_free(newpath);
				}
			}
		}
	}

	return(shortest);
}

/** Calculates the shortest path from one version to another.
 *
 * The shortest path is defined as the path with the smallest combined
 * size, not the length of the path.
 *
 * @param deltas the list of pmdelta_t * objects that a package has
 * @param from the version to start from
 * @param to the version to end at
 *
 * @return the list of pmdelta_t * objects that has the smallest size.
 * NULL (the empty list) is returned if there is no path between the
 * versions.
 */
alpm_list_t *_alpm_shortest_delta_path(alpm_list_t *deltas, const char *from,
		const char *to)
{
	alpm_list_t *path = NULL;

	path = shortest_delta_path(deltas, from, to, path);

	return(path);
}

/** Parses the string representation of a pmdelta_t object.
 *
 * This function assumes that the string is in the correct format.
 *
 * @param line the string to parse
 *
 * @return A pointer to the new pmdelta_t object
 */
pmdelta_t *_alpm_delta_parse(char *line)
{
	pmdelta_t *delta;
	char *tmp = line, *tmp2;

	CALLOC(delta, 1, sizeof(pmdelta_t), RET_ERR(PM_ERR_MEMORY, NULL));

	tmp2 = tmp;
	tmp = strchr(tmp, ' ');
	*(tmp++) = '\0';
	strncpy(delta->from, tmp2, DLT_VERSION_LEN);

	tmp2 = tmp;
	tmp = strchr(tmp, ' ');
	*(tmp++) = '\0';
	strncpy(delta->to, tmp2, DLT_VERSION_LEN);

	tmp2 = tmp;
	tmp = strchr(tmp, ' ');
	*(tmp++) = '\0';
	delta->size = atol(tmp2);

	tmp2 = tmp;
	tmp = strchr(tmp, ' ');
	*(tmp++) = '\0';
	strncpy(delta->filename, tmp2, DLT_FILENAME_LEN);

	strncpy(delta->md5sum, tmp, DLT_MD5SUM_LEN);

	return(delta);
}

/* vim: set ts=2 sw=2 noet: */
