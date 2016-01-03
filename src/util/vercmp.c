/*
 *  vercmp.c - Compare package version numbers using pacman's version
 *      comparison logic
 *
 *  Copyright (c) 2006-2016 Pacman Development Team <pacman-dev@archlinux.org>
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

#include <stdlib.h>
#include <stdio.h> /* printf */
#include <string.h>

/* forward declaration, comes from version.o in libalpm source that is linked
 * in directly so we don't have any library deps */
int alpm_pkg_vercmp(const char *a, const char *b);

static void usage(void)
{
	fprintf(stderr, "vercmp (pacman) v" PACKAGE_VERSION "\n\n"
		"Compare package version numbers using pacman's version comparison logic.\n\n"
		"Usage: vercmp <ver1> <ver2>\n\n"
		"Output values:\n"
		"  < 0 : if ver1 < ver2\n"
		"    0 : if ver1 == ver2\n"
		"  > 0 : if ver1 > ver2\n");
}

int main(int argc, char *argv[])
{
	const char *s1 = "";
	const char *s2 = "";
	int ret;

	if(argc == 1) {
		usage();
		return 2;
	}
	if(argc > 1 &&
			(strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0
			 || strcmp(argv[1], "--usage") == 0)) {
		usage();
		return 0;
	}
	if(argc > 2) {
		s2 = argv[2];
	}
	if(argc > 1) {
		s1 = argv[1];
	}

	ret = alpm_pkg_vercmp(s1, s2);
	printf("%d\n", ret);
	return EXIT_SUCCESS;
}
