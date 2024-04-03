/*
 *  sandbox_fs.h
 *
 *  Copyright (c) 2021-2024 Pacman Development Team <pacman-dev@lists.archlinux.org>
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
#ifndef ALPM_SANDBOX_FS_H
#define ALPM_SANDBOX_FS_H

#include <stdbool.h>
#include "alpm.h"

bool _alpm_sandbox_fs_restrict_writes_to(alpm_handle_t *handle, const char *path);

#endif /* ALPM_SANDBOX_FS_H */
