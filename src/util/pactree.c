/*
 *  pactree.c - a simple dependency tree viewer
 *
 *  Copyright (c) 2010-2011 Pacman Development Team <pacman-dev@archlinux.org>
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
char *provides     = " provides";
char *unresolvable = " [unresolvable]";
char *branch_tip1  = "|--";
char *branch_tip2  = "+--";
int   indent_size  = 3;

/* color */
char *branch1_color = "\033[0;33m"; /* yellow */
char *branch2_color = "\033[0;37m"; /* white */
char *leaf1_color   = "\033[1;32m"; /* bold green */
char *leaf2_color   = "\033[0;32m"; /* green */
char *color_off     = "\033[0m";

/* globals */
pmdb_t *db_local;
alpm_list_t *walked = NULL;
alpm_list_t *provisions = NULL;

/* options */
int color = 0;
int graphviz = 0;
int linear = 0;
int max_depth = -1;
int reverse = 0;
int unique = 0;
char *dbpath = NULL;

static int alpm_local_init(void)
{
	int ret;

	ret = alpm_initialize();
	if(ret != 0) {
		return ret;
	}

	ret = alpm_option_set_root(ROOTDIR);
	if(ret != 0) {
		return ret;
	}

	if(dbpath) {
		ret = alpm_option_set_dbpath(dbpath);
	} else {
		ret = alpm_option_set_dbpath(DBPATH);
	}
	if(ret != 0) {
		return ret;
	}

	db_local = alpm_option_get_localdb();
	if(!db_local) {
		return 1;
	}

	return 0;
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
				max_depth = (int)strtol(optarg, &endptr, 10);
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
				return 1;
		}
	}

	if(!argv[optind]) {
		return 1;
	}

	if(!color) {
		branch1_color = "";
		branch2_color = "";
		leaf1_color   = "";
		leaf2_color   = "";
		color_off     = "";
	}
	if(linear) {
		provides     = "";
		branch_tip1  = "";
		branch_tip2  = "";
		indent_size  = 0;
	}

	return 0;
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
	alpm_list_free(provisions);
	alpm_release();
}

/* pkg provides provision */
static void print_text(const char *pkg, const char *provision, int depth)
{
	int indent_sz = (depth + 1) * indent_size;

	if(!pkg && !provision) {
		/* not much we can do */
		return;
	}

	if(!pkg && provision) {
		/* we failed to resolve provision */
		printf("%s%*s%s%s%s%s%s\n", branch1_color, indent_sz, branch_tip1,
				leaf1_color, provision, branch1_color, unresolvable, color_off);
	} else if(provision && strcmp(pkg, provision) != 0) {
		/* pkg provides provision */
		printf("%s%*s%s%s%s%s %s%s%s\n", branch2_color, indent_sz, branch_tip2,
				leaf1_color, pkg, leaf2_color, provides, leaf1_color, provision,
				color_off);
	} else {
		/* pkg is a normal package */
		printf("%s%*s%s%s%s\n", branch1_color, indent_sz, branch_tip1, leaf1_color,
				pkg, color_off);
	}
}

static void print_graph(const char *parentname, const char *pkgname, const char *depname)
{
	if(depname) {
		printf("\"%s\" -> \"%s\" [color=chocolate4];\n", parentname, depname);
		if(pkgname && strcmp(depname, pkgname) != 0 && !alpm_list_find_str(provisions, depname)) {
			printf("\"%s\" -> \"%s\" [arrowhead=none, color=grey];\n", depname, pkgname);
			provisions = alpm_list_add(provisions, (char *)depname);
		}
	} else if(pkgname) {
		printf("\"%s\" -> \"%s\" [color=chocolate4];\n", parentname, pkgname);
	}
}

/* parent depends on dep which is satisfied by pkg */
static void print(const char *parentname, const char *pkgname, const char *depname, int depth)
{
	if(graphviz) {
		print_graph(parentname, pkgname, depname);
	} else {
		print_text(pkgname, depname, depth);
	}
}

static void print_start(const char *pkgname, const char *provname)
{
	if(graphviz) {
		printf("digraph G { START [color=red, style=filled];\n"
				"node [style=filled, color=green];\n"
				" \"START\" -> \"%s\";\n", pkgname);
	} else {
		print_text(pkgname, provname, 0);
	}
}

static void print_end(void)
{
	if(graphviz) {
		/* close graph output */
		printf("}\n");
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

		if(alpm_list_find_str(walked, pkgname)) {
			/* if we've already seen this package, don't print in "unique" output
			 * and don't recurse */
			if(!unique) {
				print(alpm_pkg_get_name(pkg), pkgname, NULL, depth);
			}
		} else {
			print(alpm_pkg_get_name(pkg), pkgname, NULL, depth);
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

		if(provider) {
			const char *provname = alpm_pkg_get_name(provider);

			if(alpm_list_find_str(walked, provname)) {
				/* if we've already seen this package, don't print in "unique" output
				 * and don't recurse */
				if(!unique) {
					print(alpm_pkg_get_name(pkg), provname, alpm_dep_get_name(depend), depth);
				}
			} else {
				print(alpm_pkg_get_name(pkg), provname, alpm_dep_get_name(depend), depth);
				walk_deps(provider, depth + 1);
			}
		} else {
			/* unresolvable package */
			print(alpm_pkg_get_name(pkg), NULL, alpm_dep_get_name(depend), depth);
		}
	}
}

int main(int argc, char *argv[])
{
	int ret;
	const char *target_name;
	pmpkg_t *pkg;

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

	pkg = alpm_find_satisfier(alpm_db_get_pkgcache(db_local), target_name);
	if(!pkg) {
		fprintf(stderr, "error: package '%s' not found\n", target_name);
		ret = 1;
		goto finish;
	}

	print_start(alpm_pkg_get_name(pkg), target_name);

	if(reverse) {
		walk_reverse_deps(pkg, 1);
	} else {
		walk_deps(pkg, 1);
	}

	print_end();

finish:
	cleanup();
	return ret;
}

/* vim: set ts=2 sw=2 noet: */
