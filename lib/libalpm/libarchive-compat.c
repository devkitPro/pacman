/*
 * libarchive-compat.c
 *
 *  Copyright (c) 2013 Pacman Development Team <pacman-dev@archlinux.org>
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

#include <stdint.h>

#include "libarchive-compat.h"

int _alpm_archive_read_free(struct archive *archive) {
#if ARCHIVE_VERSION_NUMBER >= 3000000
	return archive_read_free(archive);
#else
	return archive_read_finish(archive);
#endif
}

int64_t _alpm_archive_compressed_ftell(struct archive *archive) {
#if ARCHIVE_VERSION_NUMBER >= 3000000
	return archive_filter_bytes(archive, -1);
#else
	return archive_position_compressed(archive);
#endif
}

int _alpm_archive_read_open_file(struct archive *archive,
		const char *filename, size_t block_size) {
#if ARCHIVE_VERSION_NUMBER >= 3000000
	return archive_read_open_filename(archive, filename, block_size);
#else
	return archive_read_open_file(archive, filename, block_size);
#endif
}

int _alpm_archive_filter_code(struct archive *archive) {
#if ARCHIVE_VERSION_NUMBER >= 3000000
	return archive_filter_code(archive, 0);
#else
	return archive_compression(archive);
#endif
}

int _alpm_archive_read_support_filter_all(struct archive *archive) {
#if ARCHIVE_VERSION_NUMBER >= 3000000
	return archive_read_support_filter_all(archive);
#else
	return archive_read_support_compression_all(archive);
#endif
}

/* vim: set ts=2 sw=2 noet: */
