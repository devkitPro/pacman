/*
 *  pacsort.c - a sort utility implementing alpm_pkg_vercmp
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

#include <errno.h>
#include <fnmatch.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <alpm.h>
#include "util-common.h"

#define DELIM ' '
#define INVALD_ESCAPE_CHAR ((char)-1)

#ifndef MIN
#define MIN(a, b)      \
	__extension__({           \
		__typeof__(a) _a = (a); \
		__typeof__(b) _b = (b); \
		_a < _b ? _a : _b;           \
	})
#endif

struct buffer_t {
	char *mem;
	size_t len;
	size_t maxlen;
};

struct list_t {
	void **list;
	size_t count;
	size_t maxcount;
};

struct input_t {
	char *data;
	int is_file;

	const char *pkgname;
	size_t pkgname_len;

	const char *pkgver;
	size_t pkgver_len;
};

static struct options_t {
	int order;
	int sortkey;
	int null;
	int filemode;
	int help;
	char delim;
} opts;

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

	free(buf->mem);
	free(buf);
}

static int buffer_grow(struct buffer_t *buffer)
{
	size_t newsz = buffer->maxlen * 2.5;
	char* new_mem = realloc(buffer->mem, newsz * sizeof(char));
	if(!new_mem) {
		return 1;
	}
	buffer->mem = new_mem;
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
	void **new_list = realloc(list->list, newsz * sizeof(char *));
	if(!new_list) {
		return 1;
	}

	list->list = new_list;
	list->maxcount = newsz;

	return 0;
}

static int list_add(struct list_t *list, void *obj)
{
	if(!list || !obj) {
		return 1;
	}

	if(list->count + 1 >= list->maxcount) {
		if(list_grow(list) != 0) {
			return 1;
		}
	}

	list->list[list->count] = obj;
	list->count++;

	return 0;
}

static void list_free(struct list_t *list, void (*freefn)(void *))
{
	size_t i;

	if(!list) {
		return;
	}

	if(list->list) {
		for(i = 0; i < list->count; i++) {
			freefn(list->list[i]);
		}
		free(list->list);
	}
	free(list);
}

static void input_free(void *p)
{
	struct input_t *in = p;

	if(in == NULL) {
		return;
	}

	free(in->data);
	free(in);
}

static struct input_t *input_new(const char *path, int pathlen)
{
	const char *pkgver_end;
	const char *slash;
	struct input_t *in;

	in = calloc(1, sizeof(struct input_t));
	if(in == NULL) {
		return NULL;
	}

	in->data = strndup(path, pathlen);
	if(in->data == NULL) {
		free(in);
		return NULL;
	}

	in->is_file = fnmatch("*-*.pkg.tar.?z", in->data, 0) == 0;
	if(!in->is_file) {
		return in;
	}

	/* for files, we parse the pkgname and pkgrel from the full filename. */

	slash = strrchr(in->data, '/');
	if(slash == NULL) {
		in->pkgname = in->data;
	} else {
		in->pkgname = slash + 1;
	}

	pkgver_end = strrchr(in->pkgname, '-');

	/* read backwards through pkgrel */
	for(in->pkgver = pkgver_end - 1;
			in->pkgver > in->pkgname && *in->pkgver != '-';
			--in->pkgver)
		;
	/* read backwards through pkgver */
	for(--in->pkgver;
			in->pkgver > in->pkgname && *in->pkgver != '-';
			--in->pkgver)
		;
	++in->pkgver;

	in->pkgname_len = in->pkgver - in->pkgname - 1;
	in->pkgver_len = pkgver_end - in->pkgver;

	return in;
}

static char *explode(struct buffer_t *buffer, struct list_t *list)
{
	char *ptr, *end;
	const char linedelim = opts.null ? '\0' : '\n';
	struct input_t *meta;

	ptr = buffer->mem;
	while((end = memchr(ptr, linedelim, &buffer->mem[buffer->len] - ptr))) {
		*end = '\0';
		meta = input_new(ptr, end - ptr);
		if(meta == NULL || list_add(list, meta) != 0) {
			input_free(meta);
			return NULL;
		}
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
		struct input_t *meta = input_new(buffer->mem, buffer->len + 1);
		if(meta == NULL || list_add(list, meta) != 0) {
			input_free(meta);
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
	for(col = 0; ptr && col < opts.sortkey; col++) {
		prev = ptr;
		ptr = strchr(ptr, opts.delim);
		if(ptr) {
			ptr++;
		}
	}

	return prev;
}

static int compare_versions(const char *v1, const char *v2)
{
	if(opts.sortkey == 0) {
		return opts.order * alpm_pkg_vercmp(v1, v2);
	} else {
		return opts.order * alpm_pkg_vercmp(nth_column(v1), nth_column(v2));
	}
}

static int compare_files(const struct input_t *meta1, const struct input_t *meta2)
{
	int cmp;
	char *verbuf;
	const char *v1, *v2;

	/* sort first by package name */
	cmp = memcmp(meta1->pkgname, meta2->pkgname,
			MIN(meta1->pkgname_len, meta2->pkgname_len));

	/* 1) package names differ, sort by package name */
	if(cmp != 0) {
		return opts.order * cmp;
	}

	/* 2) prefixes are the same but length differs, sort by length */
	if(meta1->pkgname_len != meta2->pkgname_len) {
		return opts.order * (meta1->pkgname_len - meta2->pkgname_len);
	}

	/* allocate once with enough space for both pkgver */
	verbuf = calloc(1, meta1->pkgver_len + 1 + meta2->pkgver_len + 1);
	memcpy(verbuf, meta1->pkgver, meta1->pkgver_len);
	memcpy(&verbuf[meta1->pkgver_len + 1], meta2->pkgver, meta2->pkgver_len);

	/* 3) sort by package version */
	v1 = verbuf;
	v2 = verbuf + meta1->pkgver_len + 1;
	cmp = compare_versions(v1, v2);
	free(verbuf);

	return cmp;
}

static int vercmp(const void *p1, const void *p2)
{
	const struct input_t *meta1, *meta2;

	meta1 = *(struct input_t **)p1;
	meta2 = *(struct input_t **)p2;

	if(opts.filemode && meta1->is_file && meta2->is_file) {
		return compare_files(meta1, meta2);
	} else {
		return compare_versions(meta1->data, meta2->data);
	}
}

static char escape_char(const char *string)
{
	if(!string) {
		return INVALD_ESCAPE_CHAR;
	}

	const size_t len = strlen(string);

	if(len > 2) {
		return INVALD_ESCAPE_CHAR;
	}

	if(len == 1) {
		return *string;
	}

	if(*string != '\\') {
		return INVALD_ESCAPE_CHAR;
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
			return INVALD_ESCAPE_CHAR;
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
			"  -z, --null              lines end with null bytes, not newlines\n\n"
			"pacsort writes the sorted concatenation of all files, to standard output.\n"
			"Files should contain a list of inputs to sort.\n\n"
			"Standard input is read when no files are given.\n\n");
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
				opts.help = 1;
				return 0;
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
				if(opts.delim == INVALD_ESCAPE_CHAR) {
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
	int ret = 0;

	/* option defaults */
	opts.order = 1;
	opts.delim = DELIM;
	opts.sortkey = 0;
	opts.null = 0;

	if(parse_options(argc, argv) != 0) {
		usage();
		return 2;
	}

	if(opts.help) {
		usage();
		return 0;
	}

	list = list_new(100);
	buffer = buffer_new(BUFSIZ * 3);

	if(optind == argc) {
		if(splitfile(stdin, buffer, list) != 0) {
			fprintf(stderr, "%s: memory exhausted\n", argv[0]);
			ret = ENOMEM;
			goto cleanup;
		}
	} else {
		while(optind < argc) {
			FILE *input = fopen(argv[optind], "r");
			if(input) {
				if(splitfile(input, buffer, list) != 0) {
					fprintf(stderr, "%s: memory exhausted\n", argv[0]);
					fclose(input);
					ret = ENOMEM;
					goto cleanup;
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
		qsort(list->list, list->count, sizeof(void *), vercmp);
		for(i = 0; i < list->count; i++) {
			const struct input_t *in = list->list[i];
			printf("%s%c", in->data, linedelim);
		}
	}

cleanup:
	list_free(list, input_free);
	buffer_free(buffer);

	return ret;
}

/* vim: set noet: */
