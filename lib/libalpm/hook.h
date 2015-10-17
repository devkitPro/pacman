/*
 *  hook.h
 *
 *  Copyright (c) 2015 Pacman Development Team <pacman-dev@archlinux.org>
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

#ifndef _ALPM_HOOK_H
#define _ALPM_HOOK_H

#include "alpm.h"

enum _alpm_hook_when_t {
	ALPM_HOOK_PRE_TRANSACTION = 1,
	ALPM_HOOK_POST_TRANSACTION
};

int _alpm_hook_run(alpm_handle_t *handle, enum _alpm_hook_when_t when);

#endif /* _ALPM_HOOK_H */

/* vim: set noet: */
