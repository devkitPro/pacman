/*
 *  sandbox_syscalls.c
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
#include <stddef.h>

#include "config.h"
#include "log.h"
#include "sandbox_syscalls.h"
#include "util.h"

#ifdef HAVE_LIBSECCOMP
# include <seccomp.h>
#endif /* HAVE_LIBSECCOMP */

bool _alpm_sandbox_syscalls_filter(alpm_handle_t *handle)
{
	int ret = 0;
#ifdef HAVE_LIBSECCOMP
	/* see https://docs.docker.com/engine/security/seccomp/ for inspiration,
	   as well as systemd's src/shared/seccomp-util.c */
	const char *denied_syscalls[] = {
		/* kernel modules */
		"delete_module",
		"finit_module",
		"init_module",
		/* mount */
		"chroot",
		"fsconfig",
		"fsmount",
		"fsopen",
		"fspick",
		"mount",
		"mount_setattr",
		"move_mount",
		"open_tree",
		"pivot_root",
		"umount",
		"umount2",
		/* keyring */
		"add_key",
		"keyctl",
		"request_key",
		/* CPU emulation */
		"modify_ldt",
		"subpage_prot",
		"switch_endian",
		"vm86",
		"vm86old",
		/* debug */
		"kcmp",
		"lookup_dcookie",
		"perf_event_open",
		"pidfd_getfd",
		"ptrace",
		"rtas",
		"sys_debug_setcontext",
		/* set clock */
		"adjtimex",
		"clock_adjtime",
		"clock_adjtime64",
		"clock_settime",
		"clock_settime64",
		"settimeofday",
		/* raw IO */
		"ioperm",
		"iopl",
		"pciconfig_iobase",
		"pciconfig_read",
		"pciconfig_write",
		/* kexec */
		"kexec_file_load",
		"kexec_load",
		/* reboot */
		"reboot",
		/* privileged */
		"acct",
		"bpf",
		"capset",
		"chroot",
		"fanotify_init",
		"fanotify_mark",
		"nfsservctl",
		"open_by_handle_at",
		"pivot_root",
		"personality",
		/* obsolete */
		"_sysctl",
		"afs_syscall",
		"bdflush",
		"break",
		"create_module",
		"ftime",
		"get_kernel_syms",
		"getpmsg",
		"gtty",
		"idle",
		"lock",
		"mpx",
		"prof",
		"profil",
		"putpmsg",
		"query_module",
		"security",
		"sgetmask",
		"ssetmask",
		"stime",
		"stty",
		"sysfs",
		"tuxcall",
		"ulimit",
		"uselib",
		"ustat",
		"vserver",
		/* swap */
		"swapon",
		"swapoff",
	};
	/* allow all syscalls that are not listed */
	size_t idx;
	scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ALLOW);
	int restrictedSyscallsCount = 0;
	if(ctx == NULL) {
		return false;
	}

	for(idx = 0; idx < sizeof(denied_syscalls) / sizeof(*denied_syscalls); idx++) {
		int syscall = seccomp_syscall_resolve_name(denied_syscalls[idx]);
		if(syscall != __NR_SCMP_ERROR) {
			if(seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), syscall, 0) != 0) {
				_alpm_log(handle, ALPM_LOG_ERROR, _("error restricting syscall %s via seccomp!\n"), denied_syscalls[idx]);
			}
			else {
				restrictedSyscallsCount++;
			}
		}
	}

	if(seccomp_load(ctx) != 0) {
		ret = errno;
		_alpm_log(handle, ALPM_LOG_ERROR, _("error restricting syscalls via seccomp: %d!\n"), ret);
	}
	else {
		_alpm_log(handle, ALPM_LOG_DEBUG, _("successfully restricted %d syscalls via seccomp\n"), restrictedSyscallsCount);
	}

	seccomp_release(ctx);
#endif /* HAVE_LIBSECCOMP */
	return ret == 0;
}
