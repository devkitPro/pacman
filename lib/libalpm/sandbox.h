/*
 *  sandbox.h
 *
 *  Copyright (c) 2021-2022 Pacman Development Team <pacman-dev@lists.archlinux.org>
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

#ifndef ALPM_SANDBOX_H
#define ALPM_SANDBOX_H

#include <stdbool.h>


/* The type of callbacks that can happen during a sandboxed operation */
typedef enum {
	ALPM_SANDBOX_CB_LOG,
	ALPM_SANDBOX_CB_DOWNLOAD
} _alpm_sandbox_callback_t;

typedef struct {
	int callback_pipe;
} _alpm_sandbox_callback_context;


/* Sandbox callbacks */

__attribute__((format(printf, 3, 0)))
void _alpm_sandbox_cb_log(void *ctx, alpm_loglevel_t level, const char *fmt, va_list args);

void _alpm_sandbox_cb_dl(void *ctx, const char *filename, alpm_download_event_type_t event, void *data);


/* Functions to capture sandbox callbacks and convert them to alpm callbacks */

bool _alpm_sandbox_process_cb_log(alpm_handle_t *handle, int callback_pipe);
bool _alpm_sandbox_process_cb_download(alpm_handle_t *handle, int callback_pipe);


#endif /* ALPM_SANDBOX_H */
