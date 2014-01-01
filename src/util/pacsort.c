/*
 *  pacsort.c - a sort utility implementing alpm_pkg_vercmp
 *
 *  Copyright (c) 2010-2014 Pacman Development Team <pacman-dev@archlinux.org>
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

#include <errno.h>
#include <fnmatch.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <alpm.h>

#define DELIM ' '

struct buffer_t {
	char *mem;
	size_t len;
	size_t maxlen;
};

struct list_t {
	char **list;
	size_t count;
	size_t maxcount;
};

static struct options_t {
	int order;
	int sortkey;
	int null;
	int filemode;
	char delim;
} opts;

#ifndef HAVE_STRNDUP
/* A quick and dirty implementation derived from glibc */
static size_t strnlen(const char *s, size_t max)
{
	register const char *p;
	for(p = s; *p && max--; ++p);
	return (p - s);
}

char *strndup(const char *s, size_t n)
{
	size_t len = strnlen(s, n);
	char *new = (char *) malloc(len + 1);

	if(new == NULL)
		return NULL;

	new[len] = '\0';
	return (char *)memcpy(new, s, len);
}
#endif

static struct buffer_t *buffer_new(size_t initial_size)
{
	struct buffer_t *buf;

	buf = calloc(1, sizeof(*buf));
	if(!buf) {
		return NULL;
	}

	buf->mem = calloc(initial_size, sizeof(char));
	if(!buf->mem) {
		free(buf);
		return NULL;
	}

	buf->len = 0;
	buf->maxlen = initial_size;

	return buf;
}

static void buffer_free(struct buffer_t *buf)
{
	if(!buf) {
		return;
	}

	if(buf->mem) {
		free(buf->mem);
	}

	free(buf);
}

static int buffer_grow(struct buffer_t *buffer)
{
	size_t newsz = buffer->maxlen * 2.5;
	buffer->mem = realloc(buffer->mem, newsz * sizeof(char));
	if(!buffer->mem) {
		return 1;
	}
	buffer->maxlen = newsz;

	return 0;
}

static struct list_t *list_new(size_t initial_size)
{
	struct list_t *list;

	list = calloc(1, sizeof(struct list_t));
	if(!list) {
		return NULL;
	}

	list->list = calloc(initial_size, sizeof(char *));
	if(!list->list) {
		free(list);
		return NULL;
	}

	list->maxcount = initial_size;

	return list;
}

static int list_grow(struct list_t *list)
{
	size_t newsz = list->maxcount * 2.5;
	list->list = realloc(list->list, newsz * sizeof(char *));
	if(!list->list) {
		return 1;
	}

	list->maxcount = newsz;

	return 0;
}

static int list_add(struct list_t *list, char *name)
{
	if(!list || !name) {
		return 1;
	}

	if(list->count + 1 >= list->maxcount) {
		if(list_grow(list) != 0) {
			return 1;
		}
	}

	list->list[list->count] = name;
	list->count++;

	return 0;
}

static void list_free(struct list_t *list)
{
	size_t i;

	if(!list) {
		return;
	}

	if(list->list) {
		for(i = 0; i < list->count; i++) {
			free(list->list[i]);
		}
		free(list->list);
	}
	free(list);
}

static char *explode(struct buffer_t *buffer, struct list_t *list)
{
	char *name, *ptr, *end;
	const char linedelim = opts.null ? '\0' : '\n';

	ptr = buffer->mem;
	while((end = memchr(ptr, linedelim, &buffer->mem[buffer->len] - ptr))) {
		*end = '\0';
		name = strdup(ptr);
		list_add(list, name);
		ptr = end + 1;
	}

	return ptr;
}

static int splitfile(FILE *stream, struct buffer_t *buffer, struct list_t *list)
{
	size_t nread;
	char *ptr;

	while(!feof(stream)) {
		/* check if a read of BUFSIZ chars will overflow */
		if(buffer->len + BUFSIZ + 1 >= buffer->maxlen) {
			if(buffer_grow(buffer) != 0) {
				return 1;
			}
		}

		nread = fread(&buffer->mem[buffer->len], 1, BUFSIZ, stream);
		if(nread == 0) {
			break; /* EOF */
		}
		buffer->len += nread;

		if((ptr = explode(buffer, list)) == NULL) {
			return 1;
		}

		if(ptr != buffer->mem) {
			/* realign the data in the buffer */
			buffer->len = &buffer->mem[buffer->len] - ptr;
			memmove(&buffer->mem[0], ptr, buffer->len + 1);
		}
	}

	if(buffer->len) {
		char *name = strndup(buffer->mem, buffer->len + 1);
		if(list_add(list, name) != 0) {
			return 1;
		}
	}

	return 0;
}

/* returns a pointer to the nth column of a string without being destructive */
static const char *nth_column(const char *string)
{
	const char *prev, *ptr;
	int col;

	ptr = prev = string;
	for(col = 1; ptr && col <= opts.sortkey; col++) {
		prev = ptr;
		ptr = strchr(ptr, opts.delim);
		if(ptr) {
			ptr++;
		}
	}

	return prev;
}

static int vercmp(const void *p1, const void *p2)
{
	const char *name1, *name2;
	char *fn1 = NULL, *fn2 = NULL;
	int r;

	name1 = *(const char **)p1;
	name2 = *(const char **)p2;

	/* if we're operating in file mode, we modify the strings under certain
	 * conditions to appease alpm_pkg_vercmp(). If and only if both inputs end
	 * with a suffix that appears to be a package name, we strip the suffix and
	 * remove any leading paths. This means that strings such as:
	 *
	 *   /var/cache/pacman/pkg/firefox-18.0-2-x86_64.pkg.tar.xz
	 *   firefox-18.0-2-x86_64.pkg.tar.gz
	 *
	 *  Will be considered equal by this version comparison
	 */
	if(opts.filemode) {
		if(fnmatch("*-*.pkg.tar.?z", name1, 0) == 0 &&
			 fnmatch("*-*.pkg.tar.?z", name2, 0) == 0) {
			const char *start, *end;

			start = strrchr(name1, '/');
			start = start ? start + 1 : name1;
			end = strrchr(name1, '-');
			fn1 = strndup(start, end - start);

			start = strrchr(name2, '/');
			start = start ? start + 1 : name2;
			end = strrchr(name2, '-');
			fn2 = strndup(start, end - start);

			name1 = fn1;
			name2 = fn2;
		}
	}

	if(opts.sortkey == 0) {
		r = opts.order * alpm_pkg_vercmp(name1, name2);
	} else {
		r = opts.order * alpm_pkg_vercmp(nth_column(name1), nth_column(name2));
	}

	if(opts.filemode) {
		free(fn1);
		free(fn2);
	}

	return r;
}

static char escape_char(const char *string)
{
	const size_t len = strlen(string);

	if(!string || len > 2) {
		return -1;
	}

	if(len == 1) {
		return *string;
	}

	if(*string != '\\') {
		return -1;
	}

	switch(string[1]) {
		case 't':
			return '\t';
		case 'n':
			return '\n';
		case 'v':
			return '\v';
		case '0':
			return '\0';
		default:
			return -1;
	}
}

static void usage(void)
{
	fprintf(stderr, "pacsort (pacman) v" PACKAGE_VERSION "\n\n"
			"A sort utility implementing alpm_pkg_vercmp.\n\n"
			"Usage: pacsort [options] [files...]\n\n"
			"  -f, --files             assume inputs are file paths of packages\n"
			"  -h, --help              display this help message\n"
			"  -k, --key <index>       sort input starting on specified column\n"
			"  -r, --reverse           sort in reverse order (default: oldest to newest)\n"
			"  -t, --separator <sep>   specify field separator (default: space)\n"
			"  -z, --null              lines end with null bytes, not newlines\n\n");
}

static int parse_options(int argc, char **argv)
{
	int opt;

	static const struct option opttable[] = {
		{"files",     no_argument,          0, 'f'},
		{"help",      no_argument,          0, 'h'},
		{"key",       required_argument,    0, 'k'},
		{"reverse",   no_argument,          0, 'r'},
		{"separator", required_argument,    0, 't'},
		{"null",      no_argument,          0, 'z'},
		{0, 0, 0, 0}
	};

	while((opt = getopt_long(argc, argv, "fhk:rt:z", opttable, NULL)) != -1) {
		switch(opt) {
			case 'f':
				opts.filemode = 1;
				break;
			case 'h':
				return 1;
			case 'k':
				opts.sortkey = (int)strtol(optarg, NULL, 10);
				if(opts.sortkey <= 0) {
					fprintf(stderr, "error: invalid sort key -- %s\n", optarg);
					return 1;
				}
				break;
			case 'r':
				opts.order = -1;
				break;
			case 't':
				opts.delim = escape_char(optarg);
				if(opts.delim == -1) {
					fprintf(stderr, "error: invalid field separator -- `%s'\n", optarg);
					return 1;
				}
				break;
			case 'z':
				opts.null = 1;
				break;
			default:
				return 1;
		}
	}

	return 0;
}

int main(int argc, char *argv[])
{
	struct list_t *list;
	struct buffer_t *buffer;
	size_t i;

	/* option defaults */
	opts.order = 1;
	opts.delim = DELIM;
	opts.sortkey = 0;
	opts.null = 0;

	if(parse_options(argc, argv) != 0) {
		usage();
		return 2;
	}

	list = list_new(100);
	buffer = buffer_new(BUFSIZ * 3);

	if(optind == argc) {
		if(splitfile(stdin, buffer, list) != 0) {
			fprintf(stderr, "%s: memory exhausted\n", argv[0]);
			return ENOMEM;
		}
	} else {
		while(optind < argc) {
			FILE *input = fopen(argv[optind], "r");
			if(input) {
				if(splitfile(input, buffer, list) != 0) {
					fprintf(stderr, "%s: memory exhausted\n", argv[0]);
					return ENOMEM;
				}
				fclose(input);
			} else {
				fprintf(stderr, "%s: %s: %s\n", argv[0], argv[optind], strerror(errno));
			}
			optind++;
		}
	}

	if(list->count) {
		const char linedelim = opts.null ? '\0' : '\n';
		qsort(list->list, list->count, sizeof(char *), vercmp);
		for(i = 0; i < list->count; i++) {
			printf("%s%c", list->list[i], linedelim);
		}
	}

	list_free(list);
	buffer_free(buffer);

	return 0;
}

/* vim: set ts=2 sw=2 noet: */
