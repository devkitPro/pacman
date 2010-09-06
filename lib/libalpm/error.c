/*
 *  error.c
 *
 *  Copyright (c) 2006-2010 Pacman Development Team <pacman-dev@archlinux.org>
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

#include "config.h"

/* TODO: needed for the libfetch stuff, unfortunately- we should kill it */
#include <stdio.h>
#include <limits.h>
/* the following two are needed on BSD for libfetch */
#if defined(HAVE_SYS_SYSLIMITS_H)
#include <sys/syslimits.h> /* PATH_MAX */
#endif
#if defined(HAVE_SYS_PARAM_H)
#include <sys/param.h> /* MAXHOSTNAMELEN */
#endif

#ifdef HAVE_LIBFETCH
#include <fetch.h> /* fetchLastErrString */
#endif

/* libalpm */
#include "util.h"
#include "alpm.h"

const char SYMEXPORT *alpm_strerrorlast(void)
{
	return alpm_strerror(pm_errno);
}

const char SYMEXPORT *alpm_strerror(int err)
{
	switch(err) {
		/* System */
		case PM_ERR_MEMORY:
			return _("out of memory!");
		case PM_ERR_SYSTEM:
			return _("unexpected system error");
		case PM_ERR_BADPERMS:
			return _("insufficient privileges");
		case PM_ERR_NOT_A_FILE:
			return _("could not find or read file");
		case PM_ERR_NOT_A_DIR:
			return _("could not find or read directory");
		case PM_ERR_WRONG_ARGS:
			return _("wrong or NULL argument passed");
		/* Interface */
		case PM_ERR_HANDLE_NULL:
			return _("library not initialized");
		case PM_ERR_HANDLE_NOT_NULL:
			return _("library already initialized");
		case PM_ERR_HANDLE_LOCK:
			return _("unable to lock database");
		/* Databases */
		case PM_ERR_DB_OPEN:
			return _("could not open database");
		case PM_ERR_DB_CREATE:
			return _("could not create database");
		case PM_ERR_DB_NULL:
			return _("database not initialized");
		case PM_ERR_DB_NOT_NULL:
			return _("database already registered");
		case PM_ERR_DB_NOT_FOUND:
			return _("could not find database");
		case PM_ERR_DB_WRITE:
			return _("could not update database");
		case PM_ERR_DB_REMOVE:
			return _("could not remove database entry");
		/* Servers */
		case PM_ERR_SERVER_BAD_URL:
			return _("invalid url for server");
		case PM_ERR_SERVER_NONE:
			return _("no servers configured for repository");
		/* Transactions */
		case PM_ERR_TRANS_NOT_NULL:
			return _("transaction already initialized");
		case PM_ERR_TRANS_NULL:
			return _("transaction not initialized");
		case PM_ERR_TRANS_DUP_TARGET:
			return _("duplicate target");
		case PM_ERR_TRANS_NOT_INITIALIZED:
			return _("transaction not initialized");
		case PM_ERR_TRANS_NOT_PREPARED:
			return _("transaction not prepared");
		case PM_ERR_TRANS_ABORT:
			return _("transaction aborted");
		case PM_ERR_TRANS_TYPE:
			return _("operation not compatible with the transaction type");
		case PM_ERR_TRANS_NOT_LOCKED:
			return _("transaction commit attempt when database is not locked");
		/* Packages */
		case PM_ERR_PKG_NOT_FOUND:
			return _("could not find or read package");
		case PM_ERR_PKG_IGNORED:
			return _("operation cancelled due to ignorepkg");
		case PM_ERR_PKG_INVALID:
			return _("invalid or corrupted package");
		case PM_ERR_PKG_OPEN:
			return _("cannot open package file");
		case PM_ERR_PKG_CANT_REMOVE:
			return _("cannot remove all files for package");
		case PM_ERR_PKG_INVALID_NAME:
			return _("package filename is not valid");
		case PM_ERR_PKG_INVALID_ARCH:
			return _("package architecture is not valid");
		case PM_ERR_PKG_REPO_NOT_FOUND:
			return _("could not find repository for target");
		/* Deltas */
		case PM_ERR_DLT_INVALID:
			return _("invalid or corrupted delta");
		case PM_ERR_DLT_PATCHFAILED:
			return _("delta patch failed");
		/* Dependencies */
		case PM_ERR_UNSATISFIED_DEPS:
			return _("could not satisfy dependencies");
		case PM_ERR_CONFLICTING_DEPS:
			return _("conflicting dependencies");
		case PM_ERR_FILE_CONFLICTS:
			return _("conflicting files");
		/* Miscellaenous */
		case PM_ERR_RETRIEVE:
			return _("failed to retrieve some files");
		case PM_ERR_INVALID_REGEX:
			return _("invalid regular expression");
		/* Errors from external libraries- our own wrapper error */
		case PM_ERR_LIBARCHIVE:
			/* it would be nice to use archive_error_string() here, but that
			 * requires the archive struct, so we can't. Just use a generic
			 * error string instead. */
			return _("libarchive error");
		case PM_ERR_LIBFETCH:
#ifdef HAVE_LIBFETCH
			return fetchLastErrString;
#else
			/* obviously shouldn't get here... */
			return _("download library error");
#endif
		case PM_ERR_EXTERNAL_DOWNLOAD:
			return _("error invoking external downloader");
		/* Unknown error! */
		default:
			return _("unexpected error");
	}
}

/* vim: set ts=2 sw=2 noet: */
