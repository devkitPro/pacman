/*
 *  signing.h
 *
 *  Copyright (c) 2008-2011 Pacman Development Team <pacman-dev@archlinux.org>
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
#ifndef _ALPM_SIGNING_H
#define _ALPM_SIGNING_H

#include "alpm.h"

struct __pmpgpsig_t {
	/* we will either store the encoded data or the raw data-
	 * this way we can decode on an as-needed basis since most
	 * operations won't require the overhead of base64 decodes
	 * on all packages in a sync repository. */
	char *encdata;
	size_t rawlen;
	unsigned char *rawdata;
};

int _alpm_gpgme_checksig(const char *path, const pmpgpsig_t *sig);
int _alpm_load_signature(const char *sigfile, pmpgpsig_t *pgpsig);

#endif /* _ALPM_SIGNING_H */

/* vim: set ts=2 sw=2 noet: */
