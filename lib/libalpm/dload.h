/*
 *  dload.h
 *
 *  Copyright (c) 2006-2024 Pacman Development Team <pacman-dev@lists.archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
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
#ifndef ALPM_DLOAD_H
#define ALPM_DLOAD_H

#include "alpm_list.h"
#include "alpm.h"

struct dload_payload {
	alpm_handle_t *handle;
	const char *tempfile_openmode;
	/* name of the remote file */
	char *remote_name;
	/* temporary file name, to which the payload is downloaded */
	char *tempfile_name;
	/* name to which the downloaded file will be renamed */
	char *destfile_name;
	/* client has to provide either
	 *  1) fileurl - full URL to the file
	 *  2) pair of (servers, filepath), in this case ALPM iterates over the
	 *     server list and tries to download "$server/$filepath"
	 */
	char *fileurl;
	char *filepath; /* download URL path */
	alpm_list_t *cache_servers;
	alpm_list_t *servers;
	long respcode;
	/* the mtime of the existing version of this file, if there is one */
	long mtime_existing_file;
	off_t initial_size;
	off_t max_size;
	off_t prevprogress;
	int force;
	int allow_resume;
	int errors_ok;
	int unlink_on_fail;
	int download_signature; /* specifies if an accompanion *.sig file need to be downloaded*/
	int signature_optional; /* *.sig file is optional */
#ifdef HAVE_LIBCURL
	CURL *curl;
	char error_buffer[CURL_ERROR_SIZE];
	int signature; /* specifies if this payload is for a signature file */
	int request_errors_ok; /* per-request errors-ok */
#endif
	FILE *localf; /* temp download file */
};

void _alpm_dload_payload_reset(struct dload_payload *payload);

int _alpm_download(alpm_handle_t *handle,
		alpm_list_t *payloads /* struct dload_payload */,
		const char *localpath,
		const char *temporary_localpath);

#endif /* ALPM_DLOAD_H */
