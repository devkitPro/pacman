/*
 *  pactree.c - a simple dependency tree viewer
 *
 *  Copyright (c) 2010-2016 Pacman Development Team <pacman-dev@archlinux.org>
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

#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <alpm.h>
#include <alpm_list.h>

#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif

#include "util-common.h"

#define LINE_MAX     512

typedef struct tdepth {
	struct tdepth *prev;
	struct tdepth *next;
	int level;
} tdepth;

/* output */
struct graph_style {
	const char *provides;
	const char *tip;
	const char *last;
	const char *limb;
	int indent;
};

#define UTF_V   "\342\224\202"  /* U+2502, Vertical line drawing char */
#define UTF_VR  "\342\224\234"  /* U+251C, Vertical and right */
#define UTF_H   "\342\224\200"  /* U+2500, Horizontal */
#define UTF_UR  "\342\224\224"  /* U+2514, Up and right */

static struct graph_style graph_utf8 = {
	" provides",
	UTF_VR UTF_H,
	UTF_UR UTF_H,
	UTF_V " ",
	2
};

static struct graph_style graph_default = {
	" provides",
	"|-",
	"`-",
	"|",
	2
};

static struct graph_style graph_linear = {
	"",
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

/* long operations */
enum {
	OP_CONFIG = 1000
};

/* globals */
alpm_handle_t *handle = NULL;
alpm_list_t *walked = NULL;
alpm_list_t *provisions = NULL;

/* options */
struct color_choices *color = &no_color;
struct graph_style *style = &graph_utf8;
int graphviz = 0;
int max_depth = -1;
int reverse = 0;
int unique = 0;
int searchsyncs = 0;
const char *dbpath = DBPATH;
const char *configfile = CONFFILE;

static int register_syncs(void)
{
	FILE *fp;
	char *section = NULL;
	char line[LINE_MAX];
	const alpm_siglevel_t level = ALPM_SIG_DATABASE | ALPM_SIG_DATABASE_OPTIONAL;

	fp = fopen(configfile, "r");
	if(!fp) {
		fprintf(stderr, "error: config file %s could not be read\n", configfile);
		return 1;
	}

	while(fgets(line, LINE_MAX, fp)) {
		size_t linelen;
		char *ptr;

		/* ignore whole line and end of line comments */
		if((ptr = strchr(line, '#'))) {
			*ptr = '\0';
		}

		linelen = strtrim(line);

		if(linelen == 0) {
			continue;
		}

		if(line[0] == '[' && line[linelen - 1] == ']') {
			free(section);
			section = strndup(&line[1], linelen - 2);

			if(section && strcmp(section, "options") != 0) {
				alpm_db_t *db = alpm_register_syncdb(handle, section, level);
				alpm_db_set_usage(db, ALPM_DB_USAGE_ALL);
			}
		}
	}

	free(section);
	fclose(fp);

	return 0;
}

static int parse_options(int argc, char *argv[])
{
	int opt, option_index = 0;
	char *endptr = NULL;

	static const struct option opts[] = {
		{"ascii",   no_argument,          0, 'a'},
		{"dbpath",  required_argument,    0, 'b'},
		{"color",   no_argument,          0, 'c'},
		{"depth",   required_argument,    0, 'd'},
		{"graph",   no_argument,          0, 'g'},
		{"help",    no_argument,          0, 'h'},
		{"linear",  no_argument,          0, 'l'},
		{"reverse", no_argument,          0, 'r'},
		{"sync",    no_argument,          0, 's'},
		{"unique",  no_argument,          0, 'u'},

		{"config",  required_argument,    0, OP_CONFIG},
		{0, 0, 0, 0}
	};

#ifdef HAVE_LANGINFO_H
	setlocale(LC_ALL, "");
	if(strcmp(nl_langinfo(CODESET), "UTF-8") == 0) {
		style = &graph_utf8;
	}
#else
	style = &graph_default;
#endif

	while((opt = getopt_long(argc, argv, "ab:cd:ghlrsu", opts, &option_index))) {
		if(opt < 0) {
			break;
		}

		switch(opt) {
			case 'a':
				style = &graph_default;
				break;
			case OP_CONFIG:
				configfile = optarg;
				break;
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
			case 's':
				searchsyncs = 1;
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
	fprintf(stderr, "pactree (pacman) v" PACKAGE_VERSION "\n\n"
			"A simple dependency tree viewer.\n\n"
			"Usage: pactree [options] PACKAGE\n\n"
			"  -a, --ascii          use ASCII characters for tree formatting\n"
			"  -b, --dbpath <path>  set an alternate database location\n"
			"  -c, --color          colorize output\n"
			"  -d, --depth <#>      limit the depth of recursion\n"
			"  -g, --graph          generate output for graphviz's dot\n"
			"  -h, --help           display this help message\n"
			"  -l, --linear         enable linear output\n"
			"  -r, --reverse        list packages that depend on the named package\n"
			"  -s, --sync           search sync databases instead of local\n"
			"  -u, --unique         show dependencies with no duplicates (implies -l)\n"
			"      --config <path>  set an alternate configuration file\n");
}

static void cleanup(void)
{
	alpm_list_free(walked);
	alpm_list_free(provisions);
	alpm_release(handle);
}

/* pkg provides provision */
static void print_text(const char *pkg, const char *provision,
		tdepth *depth, int last)
{
	const char *tip = "";
	int level = 1;
	if(!pkg && !provision) {
		/* not much we can do */
		return;
	}

	if(depth->level > 0) {
		tip = last ? style->last : style->tip;

		/* print limbs */
		while(depth->prev) {
			depth = depth->prev;
		}
		printf("%s", color->branch1);
		while(depth->next) {
			printf("%*s%-*s", style->indent * (depth->level - level), "",
					style->indent, style->limb);
			level = depth->level + 1;
			depth = depth->next;
		}
		printf("%*s", style->indent * (depth->level - level), "");
	}

	/* print tip */
	/* If style->provides is empty (e.g. when using linear style), we do not
	 * want to print the provided package. This makes output easier to parse and
	 * to reuse. */
	if(!pkg && provision) {
		printf("%s%s%s%s [unresolvable]%s\n", tip, color->leaf1,
				provision, color->branch1, color->off);
	} else if(provision && strcmp(pkg, provision) != 0 && *(style->provides) != '\0') {
		printf("%s%s%s%s%s %s%s%s\n", tip, color->leaf1, pkg,
				color->leaf2, style->provides, color->leaf1, provision,
				color->off);
	} else {
		printf("%s%s%s%s\n", tip, color->leaf1, pkg, color->off);
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
static void print(const char *parentname, const char *pkgname,
		const char *depname, tdepth *depth, int last)
{
	if(graphviz) {
		print_graph(parentname, pkgname, depname);
	} else {
		print_text(pkgname, depname, depth, last);
	}
}

static void print_start(const char *pkgname, const char *provname)
{
	if(graphviz) {
		printf("digraph G { START [color=red, style=filled];\n"
				"node [style=filled, color=green];\n"
				" \"START\" -> \"%s\";\n", pkgname);
	} else {
		tdepth d = {
			NULL,
			NULL,
			0
		};
		print_text(pkgname, provname, &d, 0);
	}
}

static void print_end(void)
{
	if(graphviz) {
		/* close graph output */
		printf("}\n");
	}
}

static alpm_list_t *get_pkg_dep_names(alpm_pkg_t *pkg)
{
	alpm_list_t *i, *names = NULL;
	for(i = alpm_pkg_get_depends(pkg); i; i = alpm_list_next(i)) {
		alpm_depend_t *d = i->data;
		names = alpm_list_add(names, d->name);
	}
	return names;
}

/**
 * walk dependencies, showing dependencies of the target
 */
static void walk_deps(alpm_list_t *dblist, alpm_pkg_t *pkg, tdepth *depth, int rev)
{
	alpm_list_t *deps, *i;

	if(!pkg || ((max_depth >= 0) && (depth->level > max_depth))) {
		return;
	}

	walked = alpm_list_add(walked, (void *)alpm_pkg_get_name(pkg));

	if(rev) {
		deps = alpm_pkg_compute_requiredby(pkg);
	} else {
		deps = get_pkg_dep_names(pkg);
	}

	for(i = deps; i; i = alpm_list_next(i)) {
		const char *pkgname = i->data;
		int last = alpm_list_next(i) ? 0 : 1;

		alpm_pkg_t *dep_pkg = alpm_find_dbs_satisfier(handle, dblist, pkgname);

		if(alpm_list_find_str(walked, dep_pkg ? alpm_pkg_get_name(dep_pkg) : pkgname)) {
			/* if we've already seen this package, don't print in "unique" output
			 * and don't recurse */
			if(!unique) {
				print(alpm_pkg_get_name(pkg), alpm_pkg_get_name(dep_pkg), pkgname, depth, last);
			}
		} else {
			print(alpm_pkg_get_name(pkg), alpm_pkg_get_name(dep_pkg), pkgname, depth, last);
			if(dep_pkg) {
				tdepth d = {
					depth,
					NULL,
					depth->level + 1
				};
				depth->next = &d;
				/* last dep, cut off the limb here */
				if(last) {
					if(depth->prev) {
						depth->prev->next = &d;
						d.prev = depth->prev;
						depth = &d;
					} else {
						d.prev = NULL;
					}
				}
				walk_deps(dblist, dep_pkg, &d, rev);
				depth->next = NULL;
			}
		}
	}

	if(rev) {
		FREELIST(deps);
	} else {
		alpm_list_free(deps);
	}
}

int main(int argc, char *argv[])
{
	int freelist = 0, ret = 0;
	alpm_errno_t err;
	const char *target_name;
	alpm_pkg_t *pkg;
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

	if(searchsyncs) {
		if(register_syncs() != 0) {
			ret = 1;
			goto finish;
		}
		dblist = alpm_get_syncdbs(handle);
	} else {
		dblist = alpm_list_add(dblist, alpm_get_localdb(handle));
		freelist = 1;
	}

	/* we only care about the first non option arg for walking */
	target_name = argv[optind];

	pkg = alpm_find_dbs_satisfier(handle, dblist, target_name);
	if(!pkg) {
		fprintf(stderr, "error: package '%s' not found\n", target_name);
		ret = 1;
		goto finish;
	}

	print_start(alpm_pkg_get_name(pkg), target_name);

	tdepth d = {
		NULL,
		NULL,
		1
	};
	walk_deps(dblist, pkg, &d, reverse);

	print_end();

	if(freelist) {
		alpm_list_free(dblist);
	}

finish:
	cleanup();
	return ret;
}

/* vim: set noet: */
