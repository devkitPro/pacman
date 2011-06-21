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
struct graph_style {
	const char *provides;
	const char *tip1;
	const char *tip2;
	int indent;
};

static struct graph_style graph_default = {
	" provides",
	"|--",
	"+--",
	3
};

static struct graph_style graph_linear = {
	"",
	"",
	"",
	0
};

/* color choices */
struct color_choices {
	const char *branch1;
	const char *branch2;
	const char *leaf1;
	const char *leaf2;
	const char *off;
};

static struct color_choices use_color = {
	"\033[0;33m", /* yellow */
	"\033[0;37m", /* white */
	"\033[1;32m", /* bold green */
	"\033[0;32m", /* green */
	"\033[0m"
};

static struct color_choices no_color = {
	"",
	"",
	"",
	"",
	""
};

/* globals */
pmhandle_t *handle = NULL;
alpm_list_t *walked = NULL;
alpm_list_t *provisions = NULL;

/* options */
struct color_choices *color = &no_color;
struct graph_style *style = &graph_default;
int graphviz = 0;
int max_depth = -1;
int reverse = 0;
int unique = 0;
const char *dbpath = DBPATH;

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
				dbpath = optarg;
				break;
			case 'c':
				color = &use_color;
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
				style = &graph_linear;
				break;
			case 'r':
				reverse = 1;
				break;
			case 'u':
				unique = 1;
				style = &graph_linear;
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
	alpm_list_free(walked);
	alpm_list_free(provisions);
	alpm_release(handle);
}

/* pkg provides provision */
static void print_text(const char *pkg, const char *provision, int depth)
{
	int indent_sz = (depth + 1) * style->indent;

	if(!pkg && !provision) {
		/* not much we can do */
		return;
	}

	if(!pkg && provision) {
		/* we failed to resolve provision */
		printf("%s%*s%s%s%s [unresolvable]%s\n", color->branch1, indent_sz,
				style->tip1, color->leaf1, provision, color->branch1, color->off);
	} else if(provision && strcmp(pkg, provision) != 0) {
		/* pkg provides provision */
		printf("%s%*s%s%s%s%s %s%s%s\n", color->branch2, indent_sz, style->tip2,
				color->leaf1, pkg, color->leaf2, style->provides, color->leaf1, provision,
				color->off);
	} else {
		/* pkg is a normal package */
		printf("%s%*s%s%s%s\n", color->branch1, indent_sz, style->tip1, color->leaf1,
				pkg, color->off);
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

static pmpkg_t *get_pkg_from_dbs(alpm_list_t *dbs, const char *needle) {
	alpm_list_t *i;
	pmpkg_t *ret;

	for(i = dbs; i; i = alpm_list_next(i)) {
		ret = alpm_db_get_pkg(alpm_list_getdata(i), needle);
		if(ret) {
			return ret;
		}
	}
	return NULL;
}

/**
 * walk dependencies in reverse, showing packages which require the target
 */
static void walk_reverse_deps(alpm_list_t *dblist, pmpkg_t *pkg, int depth)
{
	alpm_list_t *required_by, *i;

	if(!pkg || ((max_depth >= 0) && (depth == max_depth + 1))) {
		return;
	}

	walked = alpm_list_add(walked, (void *)alpm_pkg_get_name(pkg));
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
			walk_reverse_deps(dblist, get_pkg_from_dbs(dblist, pkgname), depth + 1);
		}
	}

	FREELIST(required_by);
}

/**
 * walk dependencies, showing dependencies of the target
 */
static void walk_deps(alpm_list_t *dblist, pmpkg_t *pkg, int depth)
{
	alpm_list_t *i;

	if((max_depth >= 0) && (depth == max_depth + 1)) {
		return;
	}

	walked = alpm_list_add(walked, (void *)alpm_pkg_get_name(pkg));

	for(i = alpm_pkg_get_depends(pkg); i; i = alpm_list_next(i)) {
		pmdepend_t *depend = alpm_list_getdata(i);
		pmpkg_t *provider = alpm_find_dbs_satisfier(handle, dblist, depend->name);

		if(provider) {
			const char *provname = alpm_pkg_get_name(provider);

			if(alpm_list_find_str(walked, provname)) {
				/* if we've already seen this package, don't print in "unique" output
				 * and don't recurse */
				if(!unique) {
					print(alpm_pkg_get_name(pkg), provname, depend->name, depth);
				}
			} else {
				print(alpm_pkg_get_name(pkg), provname, depend->name, depth);
				walk_deps(dblist, provider, depth + 1);
			}
		} else {
			/* unresolvable package */
			print(alpm_pkg_get_name(pkg), NULL, depend->name, depth);
		}
	}
}

int main(int argc, char *argv[])
{
	int ret = 0;
	enum _pmerrno_t err;
	const char *target_name;
	pmpkg_t *pkg;
	alpm_list_t *dblist = NULL;

	if(parse_options(argc, argv) != 0) {
		usage();
		ret = 1;
		goto finish;
	}

	handle = alpm_initialize(ROOTDIR, dbpath, &err);
	if(!handle) {
		fprintf(stderr, "error: cannot initialize alpm: %s\n",
				alpm_strerror(err));
		ret = 1;
		goto finish;
	}

	dblist = alpm_list_add(dblist, alpm_option_get_localdb(handle));

	/* we only care about the first non option arg for walking */
	target_name = argv[optind];

	pkg = alpm_find_dbs_satisfier(handle, dblist, target_name);
	if(!pkg) {
		fprintf(stderr, "error: package '%s' not found\n", target_name);
		ret = 1;
		goto finish;
	}

	print_start(alpm_pkg_get_name(pkg), target_name);

	if(reverse) {
		walk_reverse_deps(dblist, pkg, 1);
	} else {
		walk_deps(dblist, pkg, 1);
	}

	print_end();

	alpm_list_free(dblist);

finish:
	cleanup();
	return ret;
}

/* vim: set ts=2 sw=2 noet: */
