/*
 *  sandbox_fs.c
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
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>

#include "config.h"
#include "log.h"
#include "sandbox_fs.h"
#include "util.h"

#ifdef HAVE_LINUX_LANDLOCK_H
# include <linux/landlock.h>
# include <sys/prctl.h>
# include <sys/syscall.h>
#endif /* HAVE_LINUX_LANDLOCK_H */

#ifdef HAVE_LINUX_LANDLOCK_H
#ifndef landlock_create_ruleset
static inline int landlock_create_ruleset(const struct landlock_ruleset_attr *const attr,
		const size_t size, const __u32 flags)
{
	return syscall(__NR_landlock_create_ruleset, attr, size, flags);
}
#endif /* landlock_create_ruleset */

#ifndef landlock_add_rule
static inline int landlock_add_rule(const int ruleset_fd,
		const enum landlock_rule_type rule_type,
		const void *const rule_attr, const __u32 flags)
{
	return syscall(__NR_landlock_add_rule, ruleset_fd, rule_type, rule_attr, flags);
}
#endif /* landlock_add_rule */

#ifndef landlock_restrict_self
static inline int landlock_restrict_self(const int ruleset_fd, const __u32 flags)
{
	return syscall(__NR_landlock_restrict_self, ruleset_fd, flags);
}
#endif /* landlock_restrict_self */

#define _LANDLOCK_ACCESS_FS_WRITE ( \
  LANDLOCK_ACCESS_FS_WRITE_FILE | \
  LANDLOCK_ACCESS_FS_REMOVE_DIR | \
  LANDLOCK_ACCESS_FS_REMOVE_FILE | \
  LANDLOCK_ACCESS_FS_MAKE_CHAR | \
  LANDLOCK_ACCESS_FS_MAKE_DIR | \
  LANDLOCK_ACCESS_FS_MAKE_REG | \
  LANDLOCK_ACCESS_FS_MAKE_SOCK | \
  LANDLOCK_ACCESS_FS_MAKE_FIFO | \
  LANDLOCK_ACCESS_FS_MAKE_BLOCK | \
  LANDLOCK_ACCESS_FS_MAKE_SYM)

#define _LANDLOCK_ACCESS_FS_READ ( \
  LANDLOCK_ACCESS_FS_READ_FILE | \
  LANDLOCK_ACCESS_FS_READ_DIR)

#ifdef LANDLOCK_ACCESS_FS_REFER
#define _LANDLOCK_ACCESS_FS_REFER LANDLOCK_ACCESS_FS_REFER
#else
#define _LANDLOCK_ACCESS_FS_REFER 0
#endif /* LANDLOCK_ACCESS_FS_REFER */

#ifdef LANDLOCK_ACCESS_FS_TRUNCATE
#define _LANDLOCK_ACCESS_FS_TRUNCATE LANDLOCK_ACCESS_FS_TRUNCATE
#else
#define _LANDLOCK_ACCESS_FS_TRUNCATE 0
#endif /* LANDLOCK_ACCESS_FS_TRUNCATE */

#endif /* HAVE_LINUX_LANDLOCK_H */

bool _alpm_sandbox_fs_restrict_writes_to(alpm_handle_t *handle, const char *path)
{
	ASSERT(handle != NULL, return false);
	ASSERT(path != NULL, return false);

#ifdef HAVE_LINUX_LANDLOCK_H
	struct landlock_ruleset_attr ruleset_attr = {
		.handled_access_fs = \
			_LANDLOCK_ACCESS_FS_READ | \
			_LANDLOCK_ACCESS_FS_WRITE | \
			_LANDLOCK_ACCESS_FS_REFER | \
			_LANDLOCK_ACCESS_FS_TRUNCATE | \
			LANDLOCK_ACCESS_FS_EXECUTE,
	};
	struct landlock_path_beneath_attr path_beneath = {
		.allowed_access = _LANDLOCK_ACCESS_FS_READ,
	};
	int abi = 0;
	int result = 0;
	int ruleset_fd;

	abi = landlock_create_ruleset(NULL, 0, LANDLOCK_CREATE_RULESET_VERSION);
	if(abi < 0) {
		/* landlock is not supported/enabled in the kernel */
		_alpm_log(handle, ALPM_LOG_ERROR, _("restricting filesystem access failed because landlock is not supported by the kernel!\n"));
		return true;
	}
#ifdef LANDLOCK_ACCESS_FS_REFER
	if(abi < 2) {
		_alpm_log(handle, ALPM_LOG_DEBUG, _("landlock ABI < 2, LANDLOCK_ACCESS_FS_REFER is not supported\n"));
		ruleset_attr.handled_access_fs &= ~LANDLOCK_ACCESS_FS_REFER;
	}
#endif /* LANDLOCK_ACCESS_FS_REFER */
#ifdef LANDLOCK_ACCESS_FS_TRUNCATE
	if(abi < 3) {
		_alpm_log(handle, ALPM_LOG_DEBUG, _("landlock ABI < 3, LANDLOCK_ACCESS_FS_TRUNCATE is not supported\n"));
		ruleset_attr.handled_access_fs &= ~LANDLOCK_ACCESS_FS_TRUNCATE;
	}
#endif /* LANDLOCK_ACCESS_FS_TRUNCATE */

	ruleset_fd = landlock_create_ruleset(&ruleset_attr, sizeof(ruleset_attr), 0);
	if(ruleset_fd < 0) {
		_alpm_log(handle, ALPM_LOG_ERROR, _("restricting filesystem access failed because the landlock ruleset could not be created!\n"));
		return false;
	}

	/* allow / as read-only */
	path_beneath.parent_fd = open("/", O_PATH | O_CLOEXEC | O_DIRECTORY);
	path_beneath.allowed_access = _LANDLOCK_ACCESS_FS_READ;

	if(landlock_add_rule(ruleset_fd, LANDLOCK_RULE_PATH_BENEATH, &path_beneath, 0) != 0) {
		_alpm_log(handle, ALPM_LOG_ERROR, _("restricting filesystem access failed because the landlock rule for / could not be added!\n"));
		close(path_beneath.parent_fd);
		close(ruleset_fd);
		return false;
	}

	close(path_beneath.parent_fd);

	/* allow read-write access to the directory passed as parameter */
	path_beneath.parent_fd = open(path, O_PATH | O_CLOEXEC | O_DIRECTORY);
	path_beneath.allowed_access = _LANDLOCK_ACCESS_FS_READ | _LANDLOCK_ACCESS_FS_WRITE | _LANDLOCK_ACCESS_FS_TRUNCATE;

	if(!landlock_add_rule(ruleset_fd, LANDLOCK_RULE_PATH_BENEATH, &path_beneath, 0) != 0) {
		if(landlock_restrict_self(ruleset_fd, 0)) {
		_alpm_log(handle, ALPM_LOG_ERROR, _("restricting filesystem access failed because the landlock ruleset could not be applied!\n"));
		result = errno;
		}
	} else {
		result = errno;
		_alpm_log(handle, ALPM_LOG_ERROR, _("restricting filesystem access failed because the landlock rule for the temporary download directory could not be added!\n"));
	}

	close(path_beneath.parent_fd);
	close(ruleset_fd);
	if(result == 0) {
		_alpm_log(handle, ALPM_LOG_DEBUG, _("filesystem access has been restricted to %s, landlock ABI is %d\n"), path, abi);
		return true;
        }
	return false;
#else /* HAVE_LINUX_LANDLOCK_H */
	return true;
#endif /* HAVE_LINUX_LANDLOCK_H */
}
