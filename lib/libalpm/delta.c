/*
 *  delta.c
 *
 *  Copyright (c) 2006-2010 Pacman Development Team <pacman-dev@archlinux.org>
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

#include "config.h"

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

/** \addtogroup alpm_deltas Delta Functions
 * @brief Functions to manipulate libalpm deltas
 * @{
 */

const char SYMEXPORT *alpm_delta_get_from(pmdelta_t *delta)
{
	ASSERT(delta != NULL, return(NULL));
	return(delta->from);
}

const char SYMEXPORT *alpm_delta_get_to(pmdelta_t *delta)
{
	ASSERT(delta != NULL, return(NULL));
	return(delta->to);
}

const char SYMEXPORT *alpm_delta_get_filename(pmdelta_t *delta)
{
	ASSERT(delta != NULL, return(NULL));
	return(delta->delta);
}

const char SYMEXPORT *alpm_delta_get_md5sum(pmdelta_t *delta)
{
	ASSERT(delta != NULL, return(NULL));
	return(delta->delta_md5);
}

off_t SYMEXPORT alpm_delta_get_size(pmdelta_t *delta)
{
	ASSERT(delta != NULL, return(-1));
	return(delta->delta_size);
}

/** @} */

static alpm_list_t *graph_init(alpm_list_t *deltas, int reverse)
{
	alpm_list_t *i, *j;
	alpm_list_t *vertices = NULL;
	/* create the vertices */
	for(i = deltas; i; i = i->next) {
		pmgraph_t *v = _alpm_graph_new();
		pmdelta_t *vdelta = i->data;
		vdelta->download_size = vdelta->delta_size;
		v->weight = LONG_MAX;
		v->data = vdelta;
		vertices = alpm_list_add(vertices, v);
	}

	/* compute the edges */
	for(i = vertices; i; i = i->next) {
		pmgraph_t *v_i = i->data;
		pmdelta_t *d_i = v_i->data;
		/* loop a second time so we make all possible comparisons */
		for(j = vertices; j; j = j->next) {
			pmgraph_t *v_j = j->data;
			pmdelta_t *d_j = v_j->data;
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
	return(vertices);
}

static void graph_init_size(alpm_list_t *vertices)
{
	alpm_list_t *i;

	for(i = vertices; i; i = i->next) {
		char *fpath, *md5sum;
		pmgraph_t *v = i->data;
		pmdelta_t *vdelta = v->data;

		/* determine whether the delta file already exists */
		fpath = _alpm_filecache_find(vdelta->delta);
		md5sum = alpm_compute_md5sum(fpath);
		if(fpath && md5sum && strcmp(md5sum, vdelta->delta_md5) == 0) {
			vdelta->download_size = 0;
		}
		FREE(fpath);
		FREE(md5sum);

		/* determine whether a base 'from' file exists */
		fpath = _alpm_filecache_find(vdelta->from);
		if(fpath) {
			v->weight = vdelta->download_size;
		}
		FREE(fpath);
	}
}


static void dijkstra(alpm_list_t *vertices)
{
	alpm_list_t *i;
	pmgraph_t *v;
	while(1) {
		v = NULL;
		/* find the smallest vertice not visited yet */
		for(i = vertices; i; i = i->next) {
			pmgraph_t *v_i = i->data;

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
			pmgraph_t *v_c = v->childptr->data;
			pmdelta_t *d_c = v_c->data;
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
	pmgraph_t *v = NULL;
	off_t bestsize = 0;
	alpm_list_t *rpath = NULL;

	for(i = vertices; i; i = i->next) {
		pmgraph_t *v_i = i->data;
		pmdelta_t *d_i = v_i->data;

		if(strcmp(d_i->to, to) == 0) {
			if(v == NULL || v_i->weight < v->weight) {
				v = v_i;
				bestsize = v->weight;
			}
		}
	}

	while(v != NULL) {
		pmdelta_t *vdelta = v->data;
		rpath = alpm_list_add(rpath, vdelta);
		v = v->parent;
	}
	*path = alpm_list_reverse(rpath);
	alpm_list_free(rpath);

	return(bestsize);
}

/** Calculates the shortest path from one version to another.
 * The shortest path is defined as the path with the smallest combined
 * size, not the length of the path.
 * @param deltas the list of pmdelta_t * objects that a file has
 * @param to the file to start the search at
 * @param path the pointer to a list location where pmdelta_t * objects that
 * have the smallest size are placed. NULL is set if there is no path
 * possible with the files available.
 * @return the size of the path stored, or LONG_MAX if path is unfindable
 */
off_t _alpm_shortest_delta_path(alpm_list_t *deltas,
		const char *to, alpm_list_t **path)
{
	alpm_list_t *bestpath = NULL;
	alpm_list_t *vertices;
	off_t bestsize = LONG_MAX;

	ALPM_LOG_FUNC;

	if(deltas == NULL) {
		*path = NULL;
		return(bestsize);
	}

	_alpm_log(PM_LOG_DEBUG, "started delta shortest-path search for '%s'\n", to);

	vertices = graph_init(deltas, 0);
	graph_init_size(vertices);
	dijkstra(vertices);
	bestsize = shortest_path(vertices, to, &bestpath);

	_alpm_log(PM_LOG_DEBUG, "delta shortest-path search complete : '%jd'\n", (intmax_t)bestsize);

	alpm_list_free_inner(vertices, _alpm_graph_free);
	alpm_list_free(vertices);

	*path = bestpath;
	return(bestsize);
}

static alpm_list_t *find_unused(alpm_list_t *deltas, const char *to, off_t quota)
{
	alpm_list_t *unused = NULL;
	alpm_list_t *vertices;
	alpm_list_t *i;
	vertices = graph_init(deltas, 1);

	for(i = vertices; i; i = i->next) {
		pmgraph_t *v = i->data;
		pmdelta_t *vdelta = v->data;
		if(strcmp(vdelta->to, to) == 0)
		{
			v->weight = vdelta->download_size;
		}
	}
	dijkstra(vertices);
	for(i = vertices; i; i = i->next) {
		pmgraph_t *v = i->data;
		pmdelta_t *vdelta = v->data;
		if(v->weight > quota) {
			unused = alpm_list_add(unused, vdelta->delta);
		}
	}
	alpm_list_free_inner(vertices, _alpm_graph_free);
	alpm_list_free(vertices);
	return(unused);
}

alpm_list_t SYMEXPORT *alpm_pkg_unused_deltas(pmpkg_t *pkg)
{
	off_t pkgsize = alpm_pkg_get_size(pkg);
	alpm_list_t *unused = find_unused(
			alpm_pkg_get_deltas(pkg),
			alpm_pkg_get_filename(pkg),
			pkgsize * MAX_DELTA_RATIO);
	return(unused);
}


/** Parses the string representation of a pmdelta_t object.
 * This function assumes that the string is in the correct format.
 * This format is as follows:
 * $deltafile $deltamd5 $deltasize $oldfile $newfile
 * @param line the string to parse
 * @return A pointer to the new pmdelta_t object
 */
/* TODO this does not really belong here, but in a parsing lib */
pmdelta_t *_alpm_delta_parse(char *line)
{
	pmdelta_t *delta;
	char *tmp = line, *tmp2;
	regex_t reg;

	regcomp(&reg,
			"^[^[:space:]]* [[:xdigit:]]{32} [[:digit:]]*"
			" [^[:space:]]* [^[:space:]]*$",
			REG_EXTENDED | REG_NOSUB | REG_NEWLINE);
	if(regexec(&reg, line, 0, 0, 0) != 0) {
		/* delta line is invalid, return NULL */
		regfree(&reg);
		return(NULL);
	}
	regfree(&reg);

	CALLOC(delta, 1, sizeof(pmdelta_t), RET_ERR(PM_ERR_MEMORY, NULL));

	tmp2 = tmp;
	tmp = strchr(tmp, ' ');
	*(tmp++) = '\0';
	STRDUP(delta->delta, tmp2, RET_ERR(PM_ERR_MEMORY, NULL));

	tmp2 = tmp;
	tmp = strchr(tmp, ' ');
	*(tmp++) = '\0';
	STRDUP(delta->delta_md5, tmp2, RET_ERR(PM_ERR_MEMORY, NULL));

	tmp2 = tmp;
	tmp = strchr(tmp, ' ');
	*(tmp++) = '\0';
	delta->delta_size = atol(tmp2);

	tmp2 = tmp;
	tmp = strchr(tmp, ' ');
	*(tmp++) = '\0';
	STRDUP(delta->from, tmp2, RET_ERR(PM_ERR_MEMORY, NULL));

	tmp2 = tmp;
	STRDUP(delta->to, tmp2, RET_ERR(PM_ERR_MEMORY, NULL));

	_alpm_log(PM_LOG_DEBUG, "delta : %s %s '%jd'\n", delta->from, delta->to, (intmax_t)delta->delta_size);

	return(delta);
}

void _alpm_delta_free(pmdelta_t *delta)
{
	FREE(delta->from);
	FREE(delta->to);
	FREE(delta->delta);
	FREE(delta->delta_md5);
	FREE(delta);
}

/* vim: set ts=2 sw=2 noet: */
