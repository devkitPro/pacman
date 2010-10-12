/*
 *  pactree.c - a simple dependency tree viewer
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

#include <getopt.h>
#include <stdio.h>
#include <string.h>

#include <alpm.h>
#include <alpm_list.h>

/* output */
#define PROVIDES      " provides "
#define UNRESOLVABLE  " [unresolvable]"
#define INDENT_SZ     3
#define BRANCH_TIP1   "|--"
#define BRANCH_TIP2   "+--"

/* color */
#define BRANCH1_COLOR "\033[0;33m" /* yellow */
#define BRANCH2_COLOR "\033[0;37m" /* white */
#define LEAF1_COLOR   "\033[1;32m" /* bold green */
#define LEAF2_COLOR   "\033[0;32m" /* green */
#define COLOR_OFF     "\033[0m"

/* globals */
pmdb_t *db_local;
alpm_list_t *walked = NULL;

/* options */
int color = 0;
int graphviz = 0;
int linear = 0;
int max_depth = -1;
int reverse = 0;
int unique = 0;
char *dbpath = NULL;

static int alpm_local_init()
{
	int ret;

	ret = alpm_initialize();
	if(ret != 0) {
		return(ret);
	}

	ret = alpm_option_set_root(ROOTDIR);
	if(ret != 0) {
		return(ret);
	}

	if(dbpath) {
		ret = alpm_option_set_dbpath(dbpath);
	} else {
		ret = alpm_option_set_dbpath(DBPATH);
	}
	if(ret != 0) {
		return(ret);
	}

	db_local = alpm_db_register_local();
	if(!db_local) {
		return(1);
	}

	return(0);
}

static int parse_options(int argc, char *argv[])
{
	int opt, option_index = 0;
	char *endptr = NULL;

	static struct option opts[] = {
		{"dbpath",  required_argument,    0, 'b'},
		{"color",   no_argument,          0, 'c'},
		{"depth",   required_argument,    0, 'd'},
		{"graph",   no_argument,          0, 'g'},
		{"help",    no_argument,          0, 'h'},
		{"linear",  no_argument,          0, 'l'},
		{"reverse", no_argument,          0, 'r'},
		{"unique",  no_argument,          0, 'u'},
		{0, 0, 0, 0}
	};

	while((opt = getopt_long(argc, argv, "b:cd:ghlru", opts, &option_index))) {
		if(opt < 0) {
			break;
		}

		switch(opt) {
			case 'b':
				dbpath = strdup(optarg);
				break;
			case 'c':
				color = 1;
				break;
			case 'd':
				/* validate depth */
				max_depth = strtol(optarg, &endptr, 10);
				if(*endptr != '\0') {
					fprintf(stderr, "error: invalid depth -- %s\n", optarg);
					return 1;
				}
				break;
			case 'g':
				graphviz = 1;
				break;
			case 'l':
				linear = 1;
				break;
			case 'r':
				reverse = 1;
				break;
			case 'u':
				unique = linear = 1;
				break;
			case 'h':
			case '?':
			default:
				return(1);
		}
	}

	if(!argv[optind]) {
		return(1);
	}

	return(0);
}

static void usage(void)
{
	fprintf(stderr, "pactree v" PACKAGE_VERSION "\n"
			"Usage: pactree [options] PACKAGE\n\n"
			"  -b, --dbpath <path>  set an alternate database location\n"
			"  -c, --color          colorize output\n"
			"  -d, --depth <#>      limit the depth of recursion\n"
			"  -g, --graph          generate output for graphviz's dot\n"
			"  -l, --linear         enable linear output\n"
			"  -r, --reverse        show reverse dependencies\n"
			"  -u, --unique         show dependencies with no duplicates (implies -l)\n\n"
			"  -h, --help           display this help message\n");
}

static void cleanup(void)
{
	if(dbpath) {
		free(dbpath);
	}

	alpm_list_free(walked);
	alpm_release();
}

static void print_text(const char *pkg, const char *provider, int depth)
{
	int indent_sz;

	if(unique && alpm_list_find_str(walked, pkg)) {
		return;
	}

	indent_sz =  (depth + 1) * INDENT_SZ;

	if(linear) {
		if(color) {
			printf(LEAF1_COLOR);
		}
		if(provider) {
			printf("%s %s\n", pkg, provider);
		} else {
			printf("%s\n", pkg);
		}
		if(color) {
			printf(COLOR_OFF);
		}
	} else {
		if(provider) {
			if(color) {
				printf(BRANCH2_COLOR "%*s" LEAF1_COLOR "%s" LEAF2_COLOR PROVIDES
						LEAF1_COLOR"%s\n"COLOR_OFF, indent_sz, BRANCH_TIP2, pkg, provider);
			} else {
				printf("%*s%s" PROVIDES "%s\n", indent_sz, BRANCH_TIP2, pkg, provider);
			}
		} else {
			if(color) {
				printf(BRANCH1_COLOR"%*s"LEAF1_COLOR"%s\n"COLOR_OFF, indent_sz,
						BRANCH_TIP1, pkg);
			} else {
				printf("%*s%s\n", indent_sz, BRANCH_TIP1, pkg);
			}
		}
	}
}

static void print_graph(const char *pkg, const char *dep, int provide)
{
	if(unique && alpm_list_find_str(walked, pkg)) {
		return;
	}

	if(provide) {
		printf("\"%s\" -> \"%s\" [color=grey];\n", pkg, dep);
	} else {
		printf("\"%s\" -> \"%s\" [color=chocolate4];\n", pkg, dep);
	}
}

/**
 * walk dependencies in reverse, showing packages which require the target
 */
static void walk_reverse_deps(pmpkg_t *pkg, int depth)
{
	alpm_list_t *required_by, *i;

	if((max_depth >= 0) && (depth == max_depth + 1)) {
		return;
	}

	walked = alpm_list_add(walked, (void*)alpm_pkg_get_name(pkg));
	required_by = alpm_pkg_compute_requiredby(pkg);

	for(i = required_by; i; i = alpm_list_next(i)) {
		const char *pkgname = alpm_list_getdata(i);

		if(graphviz) {
			print_graph(alpm_pkg_get_name(pkg), pkgname, 0);
		} else {
			print_text(pkgname, NULL, depth);
		}

		if(!alpm_list_find_str(walked, pkgname)) {
			walk_reverse_deps(alpm_db_get_pkg(db_local, pkgname), depth + 1);
		}
	}

	FREELIST(required_by);
}

/**
 * walk dependencies, showing dependencies of the target
 */
static void walk_deps(pmpkg_t *pkg, int depth)
{
	alpm_list_t *i;

	if((max_depth >= 0) && (depth == max_depth + 1)) {
	  return;
	}

	walked = alpm_list_add(walked, (void*)alpm_pkg_get_name(pkg));

	for(i = alpm_pkg_get_depends(pkg); i; i = alpm_list_next(i)) {
		pmdepend_t *depend = alpm_list_getdata(i);
		pmpkg_t *provider = alpm_find_satisfier(alpm_db_get_pkgcache(db_local),
				alpm_dep_get_name(depend));

		if(!provider) {
			/* can't resolve, but the show must go on */
			if(color) {
				printf(BRANCH1_COLOR "%*s" LEAF1_COLOR "%s" BRANCH1_COLOR UNRESOLVABLE
						COLOR_OFF "\n", (depth + 1) * INDENT_SZ, BRANCH_TIP1,
						alpm_dep_get_name(depend));
			} else {
				printf("%*s%s" UNRESOLVABLE "\n", (depth + 1) * INDENT_SZ, BRANCH_TIP1,
						alpm_dep_get_name(depend));
			}
		} else {
			if(graphviz) {
				print_graph(alpm_pkg_get_name(pkg), alpm_dep_get_name(depend), 0);
			} else if(strcmp(alpm_pkg_get_name(provider), alpm_dep_get_name(depend)) == 0) {
					print_text(alpm_pkg_get_name(provider), NULL, depth);
			} else {
				print_text(alpm_pkg_get_name(provider), alpm_dep_get_name(depend), depth);
			}

			/* don't recurse if we've walked this package already */
			if(!alpm_list_find_str(walked, alpm_pkg_get_name(provider))) {
				walk_deps(provider, depth + 1);
			}
		}
	}
}

int main(int argc, char *argv[])
{
	int ret;
	const char *target_name;
	pmpkg_t *target;

	ret = parse_options(argc, argv);
	if(ret != 0) {
		usage();
		goto finish;
	}

	ret = alpm_local_init();
	if(ret != 0) {
		fprintf(stderr, "error: cannot initialize alpm: %s\n", alpm_strerrorlast());
		goto finish;
	}

	/* we only care about the first non option arg for walking */
	target_name = argv[optind];

	target = alpm_find_satisfier(alpm_db_get_pkgcache(db_local), target_name);
	if(!target) {
		fprintf(stderr, "error: package '%s' not found\n", target_name);
		ret = 1;
		goto finish;
	}

	if(graphviz) {
		printf("digraph G { START [color=red, style=filled];\n"
				"node [style=filled, color=green];\n"
				" \"START\" -> \"%s\";\n", alpm_pkg_get_name(target));
	} else if(strcmp(target_name, alpm_pkg_get_name(target)) == 0) {
		print_text(target_name, NULL, 0);
	} else {
		print_text(alpm_pkg_get_name(target), target_name, 0);
	}

	if(reverse) {
		walk_reverse_deps(target, 1);
	} else {
		walk_deps(target, 1);
	}

	/* close graph output */
	if(graphviz) {
		printf("}\n");
	}

finish:
	cleanup();
	return(ret);
}

/* vim: set ts=2 sw=2 noet: */
