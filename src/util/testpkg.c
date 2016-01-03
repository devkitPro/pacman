/*
 *  testpkg.c : Test a pacman package for validity
 *
 *  Copyright (c) 2007-2016 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2007 by Aaron Griffin <aaronmgriffin@gmail.com>
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

#include <stdio.h> /* printf */
#include <stdarg.h> /* va_list */

#include <alpm.h>

__attribute__((format(printf, 2, 0)))
static void output_cb(alpm_loglevel_t level, const char *fmt, va_list args)
{
	if(fmt[0] == '\0') {
		return;
	}
	switch(level) {
		case ALPM_LOG_ERROR: printf("error: "); break;
		case ALPM_LOG_WARNING: printf("warning: "); break;
		default: return; /* skip other messages */
	}
	vprintf(fmt, args);
}

int main(int argc, char *argv[])
{
	int retval = 1; /* default = false */
	alpm_handle_t *handle;
	alpm_errno_t err;
	alpm_pkg_t *pkg = NULL;
	const alpm_siglevel_t level = ALPM_SIG_PACKAGE | ALPM_SIG_PACKAGE_OPTIONAL;

	if(argc != 2) {
		fprintf(stderr, "testpkg (pacman) v" PACKAGE_VERSION "\n\n"
			"Test a pacman package for validity.\n\n"
			"Usage: testpkg <package file>\n");
		return 1;
	}

	handle = alpm_initialize(ROOTDIR, DBPATH, &err);
	if(!handle) {
		fprintf(stderr, "cannot initialize alpm: %s\n", alpm_strerror(err));
		return 1;
	}

	/* let us get log messages from libalpm */
	alpm_option_set_logcb(handle, output_cb);

	/* set gpgdir to default */
	alpm_option_set_gpgdir(handle, GPGDIR);

	if(alpm_pkg_load(handle, argv[1], 1, level, &pkg) == -1
			|| pkg == NULL) {
		err = alpm_errno(handle);
		switch(err) {
			case ALPM_ERR_PKG_NOT_FOUND:
				printf("Cannot find the given file.\n");
				break;
			case ALPM_ERR_PKG_OPEN:
				printf("Cannot open the given file.\n");
				break;
			case ALPM_ERR_LIBARCHIVE:
			case ALPM_ERR_PKG_INVALID:
				printf("Package is invalid.\n");
				break;
			default:
				printf("libalpm error: %s\n", alpm_strerror(err));
				break;
		}
		retval = 1;
	} else {
		alpm_pkg_free(pkg);
		printf("Package is valid.\n");
		retval = 0;
	}

	if(alpm_release(handle) == -1) {
		fprintf(stderr, "error releasing alpm\n");
	}

	return retval;
}
