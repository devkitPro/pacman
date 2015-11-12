/*
 *  sighandler.c
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

#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include <alpm.h>

#include "conf.h"
#include "sighandler.h"
#include "util.h"

/** Write function that correctly handles EINTR.
 */
static ssize_t xwrite(int fd, const void *buf, size_t count)
{
	ssize_t ret;
	do {
		ret = write(fd, buf, count);
	} while(ret == -1 && errno == EINTR);
	return ret;
}

/** Catches thrown signals. Performs necessary cleanup to ensure database is
 * in a consistent state.
 * @param signum the thrown signal
 */
static void handler(int signum)
{
	if(signum == SIGINT) {
		const char msg[] = "\nInterrupt signal received\n";
		xwrite(STDERR_FILENO, msg, ARRAYSIZE(msg) - 1);
	} else {
		const char msg[] = "\nHangup signal received\n";
		xwrite(STDERR_FILENO, msg, ARRAYSIZE(msg) - 1);
	}
	if(alpm_trans_interrupt(config->handle) == 0) {
		/* a transaction is being interrupted, don't exit pacman yet. */
		return;
	}
	/* SIGINT/SIGHUP: no committing transaction, release it now and then exit pacman */
	alpm_unlock(config->handle);
	/* output a newline to be sure we clear any line we may be on */
	xwrite(STDOUT_FILENO, "\n", 1);
	_Exit(128 + signum);
}

static void segv_handler(int signum)
{
	const char msg[] = "\nerror: segmentation fault\n"
		"Please submit a full bug report with --debug if appropriate.\n";
	xwrite(STDERR_FILENO, msg, sizeof(msg) - 1);
	_Exit(signum);
}

void install_segv_handler(void)
{
	struct sigaction new_action;
	new_action.sa_handler = segv_handler;
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = SA_RESTART;
	sigaction(SIGSEGV, &new_action, NULL);
}

static void winch_handler(int signum)
{
	(void)signum; /* suppress unused variable warnings */
	columns_cache_reset();
}

void install_winch_handler(void)
{
	struct sigaction new_action;
	new_action.sa_handler = winch_handler;
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = SA_RESTART;
	sigaction(SIGWINCH, &new_action, NULL);
}

void install_signal_handlers(void)
{
	struct sigaction new_action;
	const int signals[] = { SIGHUP, SIGINT };
	size_t i;

	/* Set signal handlers */
	/* Set up the structure to specify the new action. */
	new_action.sa_handler = handler;
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = SA_RESTART;

	/* assign our handler to any signals we care about */
	for(i = 0; i < ARRAYSIZE(signals); i++) {
		sigaction(signals[i], &new_action, NULL);
	}
}

/* vim: set noet: */
