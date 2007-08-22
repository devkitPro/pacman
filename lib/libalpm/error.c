/*
 *  error.c
 * 
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2005 by Christian Hamar <krics@linuxforum.hu>
 *  Copyright (c) 2006 by Miklos Vajna <vmiklos@frugalware.org>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
 *  USA.
 */

#include "config.h"

/* libalpm */
#include "error.h"
#include "util.h"
#include "alpm.h"

/* TODO does this really need a file all on its own? */
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
		/* Configuration */
		case PM_ERR_OPT_LOGFILE:
		case PM_ERR_OPT_DBPATH:
		case PM_ERR_OPT_LOCALDB:
		case PM_ERR_OPT_SYNCDB:
		case PM_ERR_OPT_USESYSLOG:
			return _("could not set parameter");
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
		case PM_ERR_TRANS_COMMITING:
			return _("could not commit transaction");
		case PM_ERR_TRANS_DOWNLOADING:
			return _("could not download all files");
		/* Packages */
		case PM_ERR_PKG_NOT_FOUND:
			return _("could not find or read package");
		case PM_ERR_PKG_INVALID:
			return _("invalid or corrupted package");
		case PM_ERR_PKG_OPEN:
			return _("cannot open package file");
		case PM_ERR_PKG_LOAD:
			return _("cannot load package data");
		case PM_ERR_PKG_INSTALLED:
			return _("package already installed");
		case PM_ERR_PKG_CANT_FRESH:
			return _("package not installed or lesser version");
		case PM_ERR_PKG_CANT_REMOVE:
			return _("cannot remove all files for package");
		case PM_ERR_PKG_INVALID_NAME:
			return _("package name is not valid");
		case PM_ERR_PKG_CORRUPTED:
			return _("corrupted package");
		case PM_ERR_PKG_REPO_NOT_FOUND:
			return _("no such repository");
		/* Groups */
		case PM_ERR_GRP_NOT_FOUND:
			return _("group not found");
		/* Dependencies */
		case PM_ERR_UNSATISFIED_DEPS:
			return _("could not satisfy dependencies");
		case PM_ERR_CONFLICTING_DEPS:
			return _("conflicting dependencies");
		case PM_ERR_FILE_CONFLICTS:
			return _("conflicting files");
		/* Miscellaenous */
		case PM_ERR_USER_ABORT:
			return _("user aborted the operation");
		case PM_ERR_INTERNAL_ERROR:
			return _("internal error");
		case PM_ERR_LIBARCHIVE_ERROR:
			return _("libarchive error");
		case PM_ERR_PKG_HOLD:
			/* TODO wow this is not descriptive at all... what does this mean? */
			return _("not confirmed");
		case PM_ERR_INVALID_REGEX:
			return _("invalid regular expression");
		/* Downloading */
		case PM_ERR_CONNECT_FAILED:
			return _("connection to remote host failed");
		/* Unknown error! */
		default:
			return _("unexpected error");
	}
}

/* vim: set ts=2 sw=2 noet: */
