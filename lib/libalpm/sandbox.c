/*
 *  sandbox.c
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

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include "alpm.h"
#include "util.h"

int SYMEXPORT alpm_sandbox_setup_child(const char* sandboxuser)
{
	struct passwd const *pw = NULL;

	ASSERT(sandboxuser != NULL, return -1);
	ASSERT(getuid() == 0, return -1);
	ASSERT((pw = getpwnam(sandboxuser)), return -1);
	ASSERT(setgid(pw->pw_gid) == 0, return -1);
	ASSERT(setgroups(0, NULL) == 0, return -1);
	ASSERT(setuid(pw->pw_uid) == 0, return -1);

	return 0;
}

