/*
 *  delta.c
 *
 *  Copyright (c) 2006-2016 Pacman Development Team <pacman-dev@archlinux.org>
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

#include <stdlib.h>
#include <string.h>
#include <stdint.h> /* intmax_t */
#include <limits.h>
#include <sys/types.h>
#include <regex.h>

/* libalpm */
#include "delta.h"
#include "alpm_list.h"
#include "util.h"
#include "log.h"
#include "graph.h"

static alpm_list_t *graph_init(alpm_list_t *deltas, int reverse)
{
	alpm_list_t *i, *j;
	alpm_list_t *vertices = NULL;
	/* create the vertices */
	for(i = deltas; i; i = i->next) {
		alpm_graph_t *v = _alpm_graph_new();
		if(!v) {
			alpm_list_free(vertices);
			return NULL;
		}
		alpm_delta_t *vdelta = i->data;
		vdelta->download_size = vdelta->delta_size;
		v->weight = LONG_MAX;
		v->data = vdelta;
		vertices = alpm_list_add(vertices, v);
	}

	/* compute the edges */
	for(i = vertices; i; i = i->next) {
		alpm_graph_t *v_i = i->data;
		alpm_delta_t *d_i = v_i->data;
		/* loop a second time so we make all possible comparisons */
		for(j = vertices; j; j = j->next) {
			alpm_graph_t *v_j = j->data;
			alpm_delta_t *d_j = v_j->data;
			/* We want to create a delta tree like the following:
			 *          1_to_2
			 *            |
			 * 1_to_3   2_to_3
			 *   \        /
			 *     3_to_4
			 * If J 'from' is equal to I 'to', then J is a child of I.
			 * */
			if((!reverse && strcmp(d_j->from, d_i->to) == 0) ||
					(reverse && strcmp(d_j->to, d_i->from) == 0)) {
				v_i->children = alpm_list_add(v_i->children, v_j);
			}
		}
		v_i->childptr = v_i->children;
	}
	return vertices;
}

static void graph_init_size(alpm_handle_t *handle, alpm_list_t *vertices)
{
	alpm_list_t *i;

	for(i = vertices; i; i = i->next) {
		char *fpath, *md5sum;
		alpm_graph_t *v = i->data;
		alpm_delta_t *vdelta = v->data;

		/* determine whether the delta file already exists */
		fpath = _alpm_filecache_find(handle, vdelta->delta);
		if(fpath) {
			md5sum = alpm_compute_md5sum(fpath);
			if(md5sum && strcmp(md5sum, vdelta->delta_md5) == 0) {
				vdelta->download_size = 0;
			}
			FREE(md5sum);
			FREE(fpath);
		} else {
			char *fnamepart;
			CALLOC(fnamepart, strlen(vdelta->delta) + 6, sizeof(char), return);
			sprintf(fnamepart, "%s.part", vdelta->delta);
			fpath = _alpm_filecache_find(handle, fnamepart);
			if(fpath) {
				struct stat st;
				if(stat(fpath, &st) == 0) {
					vdelta->download_size = vdelta->delta_size - st.st_size;
					vdelta->download_size = vdelta->download_size < 0 ? 0 : vdelta->download_size;
				}
				FREE(fpath);
			}
			FREE(fnamepart);
		}

		/* determine whether a base 'from' file exists */
		fpath = _alpm_filecache_find(handle, vdelta->from);
		if(fpath) {
			v->weight = vdelta->download_size;
		}
		FREE(fpath);
	}
}


static void dijkstra(alpm_list_t *vertices)
{
	alpm_list_t *i;
	alpm_graph_t *v;
	while(1) {
		v = NULL;
		/* find the smallest vertice not visited yet */
		for(i = vertices; i; i = i->next) {
			alpm_graph_t *v_i = i->data;

			if(v_i->state == -1) {
				continue;
			}

			if(v == NULL || v_i->weight < v->weight) {
				v = v_i;
			}
		}
		if(v == NULL || v->weight == LONG_MAX) {
			break;
		}

		v->state = -1;

		v->childptr = v->children;
		while(v->childptr) {
			alpm_graph_t *v_c = v->childptr->data;
			alpm_delta_t *d_c = v_c->data;
			if(v_c->weight > v->weight + d_c->download_size) {
				v_c->weight = v->weight + d_c->download_size;
				v_c->parent = v;
			}

			v->childptr = (v->childptr)->next;

		}
	}
}

static off_t shortest_path(alpm_list_t *vertices, const char *to, alpm_list_t **path)
{
	alpm_list_t *i;
	alpm_graph_t *v = NULL;
	off_t bestsize = 0;
	alpm_list_t *rpath = NULL;

	for(i = vertices; i; i = i->next) {
		alpm_graph_t *v_i = i->data;
		alpm_delta_t *d_i = v_i->data;

		if(strcmp(d_i->to, to) == 0) {
			if(v == NULL || v_i->weight < v->weight) {
				v = v_i;
				bestsize = v->weight;
			}
		}
	}

	while(v != NULL) {
		alpm_delta_t *vdelta = v->data;
		rpath = alpm_list_add(rpath, vdelta);
		v = v->parent;
	}
	*path = alpm_list_reverse(rpath);
	alpm_list_free(rpath);

	return bestsize;
}

/** Calculates the shortest path from one version to another.
 * The shortest path is defined as the path with the smallest combined
 * size, not the length of the path.
 * @param handle the context handle
 * @param deltas the list of alpm_delta_t * objects that a file has
 * @param to the file to start the search at
 * @param path the pointer to a list location where alpm_delta_t * objects that
 * have the smallest size are placed. NULL is set if there is no path
 * possible with the files available.
 * @return the size of the path stored, or LONG_MAX if path is unfindable
 */
off_t _alpm_shortest_delta_path(alpm_handle_t *handle, alpm_list_t *deltas,
		const char *to, alpm_list_t **path)
{
	alpm_list_t *bestpath = NULL;
	alpm_list_t *vertices;
	off_t bestsize = LONG_MAX;

	if(deltas == NULL) {
		*path = NULL;
		return bestsize;
	}

	_alpm_log(handle, ALPM_LOG_DEBUG, "started delta shortest-path search for '%s'\n", to);

	vertices = graph_init(deltas, 0);
	graph_init_size(handle, vertices);
	dijkstra(vertices);
	bestsize = shortest_path(vertices, to, &bestpath);

	_alpm_log(handle, ALPM_LOG_DEBUG, "delta shortest-path search complete : '%jd'\n", (intmax_t)bestsize);

	alpm_list_free_inner(vertices, _alpm_graph_free);
	alpm_list_free(vertices);

	*path = bestpath;
	return bestsize;
}

static alpm_list_t *find_unused(alpm_list_t *deltas, const char *to, off_t quota)
{
	alpm_list_t *unused = NULL;
	alpm_list_t *vertices;
	alpm_list_t *i;
	vertices = graph_init(deltas, 1);

	for(i = vertices; i; i = i->next) {
		alpm_graph_t *v = i->data;
		alpm_delta_t *vdelta = v->data;
		if(strcmp(vdelta->to, to) == 0)
		{
			v->weight = vdelta->download_size;
		}
	}
	dijkstra(vertices);
	for(i = vertices; i; i = i->next) {
		alpm_graph_t *v = i->data;
		alpm_delta_t *vdelta = v->data;
		if(v->weight > quota) {
			unused = alpm_list_add(unused, vdelta->delta);
		}
	}
	alpm_list_free_inner(vertices, _alpm_graph_free);
	alpm_list_free(vertices);
	return unused;
}

/** \addtogroup alpm_deltas Delta Functions
 * @brief Functions to manipulate libalpm deltas
 * @{
 */

alpm_list_t SYMEXPORT *alpm_pkg_unused_deltas(alpm_pkg_t *pkg)
{
	ASSERT(pkg != NULL, return NULL);
	return find_unused(pkg->deltas, pkg->filename,
			pkg->size * pkg->handle->deltaratio);
}

/** @} */

#define NUM_MATCHES 6

/** Parses the string representation of a alpm_delta_t object.
 * This function assumes that the string is in the correct format.
 * This format is as follows:
 * $deltafile $deltamd5 $deltasize $oldfile $newfile
 * @param handle the context handle
 * @param line the string to parse
 * @return A pointer to the new alpm_delta_t object
 */
alpm_delta_t *_alpm_delta_parse(alpm_handle_t *handle, const char *line)
{
	alpm_delta_t *delta;
	size_t len;
	regmatch_t pmatch[NUM_MATCHES];
	char filesize[32];

	/* this is so we only have to compile the pattern once */
	if(!handle->delta_regex_compiled) {
		/* $deltafile $deltamd5 $deltasize $oldfile $newfile*/
		regcomp(&handle->delta_regex,
				"^([^[:space:]]+) ([[:xdigit:]]{32}) ([[:digit:]]+)"
				" ([^[:space:]]+) ([^[:space:]]+)$",
				REG_EXTENDED | REG_NEWLINE);
		handle->delta_regex_compiled = 1;
	}

	if(regexec(&handle->delta_regex, line, NUM_MATCHES, pmatch, 0) != 0) {
		/* delta line is invalid, return NULL */
		return NULL;
	}

	CALLOC(delta, 1, sizeof(alpm_delta_t), return NULL);

	/* start at index 1 -- match 0 is the entire match */
	len = pmatch[1].rm_eo - pmatch[1].rm_so;
	STRNDUP(delta->delta, &line[pmatch[1].rm_so], len, goto error);

	len = pmatch[2].rm_eo - pmatch[2].rm_so;
	STRNDUP(delta->delta_md5, &line[pmatch[2].rm_so], len, goto error);

	len = pmatch[3].rm_eo - pmatch[3].rm_so;
	if(len < sizeof(filesize)) {
		strncpy(filesize, &line[pmatch[3].rm_so], len);
		filesize[len] = '\0';
		delta->delta_size = _alpm_strtoofft(filesize);
	}

	len = pmatch[4].rm_eo - pmatch[4].rm_so;
	STRNDUP(delta->from, &line[pmatch[4].rm_so], len, goto error);

	len = pmatch[5].rm_eo - pmatch[5].rm_so;
	STRNDUP(delta->to, &line[pmatch[5].rm_so], len, goto error);

	return delta;

error:
	_alpm_delta_free(delta);
	return NULL;
}

#undef NUM_MATCHES

void _alpm_delta_free(alpm_delta_t *delta)
{
	ASSERT(delta != NULL, return);
	FREE(delta->delta);
	FREE(delta->delta_md5);
	FREE(delta->from);
	FREE(delta->to);
	FREE(delta);
}

alpm_delta_t *_alpm_delta_dup(const alpm_delta_t *delta)
{
	alpm_delta_t *newdelta;
	CALLOC(newdelta, 1, sizeof(alpm_delta_t), return NULL);
	STRDUP(newdelta->delta, delta->delta, goto error);
	STRDUP(newdelta->delta_md5, delta->delta_md5, goto error);
	STRDUP(newdelta->from, delta->from, goto error);
	STRDUP(newdelta->to, delta->to, goto error);
	newdelta->delta_size = delta->delta_size;
	newdelta->download_size = delta->download_size;

	return newdelta;

error:
	_alpm_delta_free(newdelta);
	return NULL;
}

/* vim: set noet: */
