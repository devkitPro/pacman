/*
 *  vercmp.c
 *
 *  Copyright (c) 2002-2005 by Judd Vinet <jvinet@zeroflux.org>
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

#include <stdio.h> /* printf */
#include <string.h> /* strncpy */

#include <alpm.h>

#define BASENAME "vercmp"

#define MAX_LEN 255

static void usage()
{
	fprintf(stderr, "usage: %s <ver1> <ver2>\n\n", BASENAME);
	fprintf(stderr, "return values:\n");
	fprintf(stderr, "  < 0 : if ver1 < ver2\n");
	fprintf(stderr, "    0 : if ver1 == ver2\n");
	fprintf(stderr, "  > 0 : if ver1 > ver2\n");
}

int main(int argc, char *argv[])
{
	char s1[MAX_LEN] = "";
	char s2[MAX_LEN] = "";
	int ret;

	if(argc == 1) {
		usage();
		return(2);
	}
	if(argc > 1 &&
			(strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0
			 || strcmp(argv[1], "--usage") == 0)) {
		usage();
		return(0);
	}
	if(argc > 1) {
		strncpy(s1, argv[1], MAX_LEN);
		s1[MAX_LEN -1] = '\0';
	}
	if(argc > 2) {
		strncpy(s2, argv[2], MAX_LEN);
		s2[MAX_LEN -1] = '\0';
	} else {
		printf("0\n");
		return(0);
	}

	ret = alpm_pkg_vercmp(s1, s2);
	printf("%d\n", ret);
	return(ret);
}
