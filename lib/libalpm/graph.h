/*
 *  graph.h - helpful graph structure and setup/teardown methods
 *
 *  Copyright (c) 2006-2010 Pacman Development Team <pacman-dev@archlinux.org>
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

#include <sys/types.h> /* off_t */

#include "alpm_list.h"
#include "util.h" /* MALLOC() */
#include "alpm.h"

struct __pmgraph_t {
	char state; /* 0: untouched, -1: entered, other: leaving time */
	void *data;
	off_t weight; /* weight of the node */
	struct __pmgraph_t *parent; /* where did we come from? */
	alpm_list_t *children;
	alpm_list_t *childptr; /* points to a child in children list */
};
typedef struct __pmgraph_t pmgraph_t;

static pmgraph_t *_alpm_graph_new(void)
{
	pmgraph_t *graph = NULL;

	MALLOC(graph, sizeof(pmgraph_t), RET_ERR(PM_ERR_MEMORY, NULL));

	if(graph) {
		graph->state = 0;
		graph->data = NULL;
		graph->parent = NULL;
		graph->children = NULL;
		graph->childptr = NULL;
	}
	return(graph);
}

static void _alpm_graph_free(void *data)
{
	pmgraph_t *graph = data;
	alpm_list_free(graph->children);
	free(graph);
}

