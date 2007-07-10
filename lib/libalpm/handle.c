/*
 *  handle.c
 * 
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2005, 2006 by Miklos Vajna <vmiklos@frugalware.org>
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <syslog.h>
#include <time.h>

/* libalpm */
#include "handle.h"
#include "alpm_list.h"
#include "util.h"
#include "log.h"
#include "error.h"
#include "trans.h"
#include "alpm.h"
#include "server.h"

pmhandle_t *_alpm_handle_new()
{
	pmhandle_t *handle;

	handle = malloc(sizeof(pmhandle_t));
	if(handle == NULL) {
		_alpm_log(PM_LOG_ERROR, _("malloc failure: could not allocate %d bytes"), sizeof(pmhandle_t));
		RET_ERR(PM_ERR_MEMORY, NULL);
	}

	memset(handle, 0, sizeof(pmhandle_t));
	handle->lckfd = -1;
	handle->logstream = NULL;

	/* see if we're root or not */
	handle->uid = geteuid();
	handle->root = NULL;
	handle->dbpath = NULL;
	handle->cachedirs = NULL;
	handle->lockfile = NULL;
	handle->logfile = NULL;

	return(handle);
}

void _alpm_handle_free(pmhandle_t *handle)
{
	ALPM_LOG_FUNC;

	if(handle == NULL) {
		return;
	}

	/* close logfile */
	if(handle->logstream) {
		fclose(handle->logstream);
		handle->logstream= NULL;
	}
	if(handle->usesyslog) {
		handle->usesyslog = 0;
		closelog();
	}

	/* free memory */
	_alpm_trans_free(handle->trans);
	FREE(handle->root);
	FREE(handle->dbpath);
	FREELIST(handle->cachedirs);
	FREE(handle->logfile);
	FREE(handle->lockfile);
	FREE(handle->xfercommand);
	FREELIST(handle->dbs_sync);
	FREELIST(handle->noupgrade);
	FREELIST(handle->noextract);
	FREELIST(handle->ignorepkg);
	FREELIST(handle->holdpkg);
	FREE(handle);
}

alpm_cb_log SYMEXPORT alpm_option_get_logcb() { return (handle ? handle->logcb : NULL); }
alpm_cb_download SYMEXPORT alpm_option_get_dlcb() { return (handle ? handle->dlcb : NULL); }
const char SYMEXPORT *alpm_option_get_root() { return handle->root; }
const char SYMEXPORT *alpm_option_get_dbpath() { return handle->dbpath; }
alpm_list_t SYMEXPORT *alpm_option_get_cachedirs() { return handle->cachedirs; }
const char SYMEXPORT *alpm_option_get_logfile() { return handle->logfile; }
const char SYMEXPORT *alpm_option_get_lockfile() { return handle->lockfile; }
unsigned short SYMEXPORT alpm_option_get_usesyslog() { return handle->usesyslog; }
alpm_list_t SYMEXPORT *alpm_option_get_noupgrades() { return handle->noupgrade; }
alpm_list_t SYMEXPORT *alpm_option_get_noextracts() { return handle->noextract; }
alpm_list_t SYMEXPORT *alpm_option_get_ignorepkgs() { return handle->ignorepkg; }
alpm_list_t SYMEXPORT *alpm_option_get_holdpkgs() { return handle->holdpkg; }
time_t SYMEXPORT alpm_option_get_upgradedelay() { return handle->upgradedelay; }
const char SYMEXPORT *alpm_option_get_xfercommand() { return handle->xfercommand; }
unsigned short SYMEXPORT alpm_option_get_nopassiveftp() { return handle->nopassiveftp; }

pmdb_t SYMEXPORT *alpm_option_get_localdb() { return handle->db_local; }
alpm_list_t SYMEXPORT *alpm_option_get_syncdbs()
{
	return handle->dbs_sync;
}

void SYMEXPORT alpm_option_set_logcb(alpm_cb_log cb) { handle->logcb = cb; }

void SYMEXPORT alpm_option_set_dlcb(alpm_cb_download cb) { handle->dlcb = cb; }

void SYMEXPORT alpm_option_set_root(const char *root)
{
	ALPM_LOG_FUNC;

	if(handle->root) FREE(handle->root);
	/* According to the man page, realpath is safe to use IFF the second arg is
	 * NULL. */
	char *realroot = realpath(root, NULL);
	if(realroot) {
		root = realroot;
	} else {
		_alpm_log(PM_LOG_ERROR, _("cannot canonicalize specified root path '%s'"), root);
	}

	if(root) {
		/* verify root ends in a '/' */
		int rootlen = strlen(realroot);
		if(realroot[rootlen-1] != '/') {
			rootlen += 1;
		}
		handle->root = calloc(rootlen+1, sizeof(char));
		strncpy(handle->root, realroot, rootlen);
		handle->root[rootlen-1] = '/';
	}
	if(realroot) {
		free(realroot);
	}
	_alpm_log(PM_LOG_DEBUG, "option 'root' = %s", handle->root);
}

void SYMEXPORT alpm_option_set_dbpath(const char *dbpath)
{
	ALPM_LOG_FUNC;

	if(handle->dbpath) FREE(handle->dbpath);
	if(handle->lockfile) FREE(handle->lockfile);
	if(dbpath) {
		/* verify dbpath ends in a '/' */
		int dbpathlen = strlen(dbpath);
		if(dbpath[dbpathlen-1] != '/') {
			dbpathlen += 1;
		}
		handle->dbpath = calloc(dbpathlen+1, sizeof(char));
		strncpy(handle->dbpath, dbpath, dbpathlen);
		handle->dbpath[dbpathlen-1] = '/';
		_alpm_log(PM_LOG_DEBUG, "option 'dbpath' = %s", handle->dbpath);

		const char *lf = "db.lck";
		int lockfilelen = strlen(handle->dbpath) + strlen(lf) + 1;
		handle->lockfile = calloc(lockfilelen, sizeof(char));
		snprintf(handle->lockfile, lockfilelen, "%s%s", handle->dbpath, lf);
		_alpm_log(PM_LOG_DEBUG, "option 'lockfile' = %s", handle->lockfile);
	}

}

void SYMEXPORT alpm_option_add_cachedir(const char *cachedir)
{
	ALPM_LOG_FUNC;

	if(cachedir) {
		char *newcachedir;
		/* verify cachedir ends in a '/' */
		int cachedirlen = strlen(cachedir);
		if(cachedir[cachedirlen-1] != '/') {
			cachedirlen += 1;
		}
		newcachedir = calloc(cachedirlen + 1, sizeof(char));
		strncpy(newcachedir, cachedir, cachedirlen);
		newcachedir[cachedirlen-1] = '/';
		handle->cachedirs = alpm_list_add(handle->cachedirs, newcachedir);
		_alpm_log(PM_LOG_DEBUG, "option 'cachedir' = %s", newcachedir);
	}
}

void SYMEXPORT alpm_option_set_cachedirs(alpm_list_t *cachedirs)
{
	if(handle->cachedirs) FREELIST(handle->cachedirs);
	if(cachedirs) handle->cachedirs = cachedirs;
}

void SYMEXPORT alpm_option_set_logfile(const char *logfile)
{
	ALPM_LOG_FUNC;

	if(handle->logfile) {
		FREE(handle->logfile);
		if(handle->logstream) {
			fclose(handle->logstream);
			handle->logstream = NULL;
		}
	}
	if(logfile) {
		handle->logfile = strdup(logfile);
		handle->logstream = fopen(logfile, "a");
	}
}

void SYMEXPORT alpm_option_set_usesyslog(unsigned short usesyslog)
{
	handle->usesyslog = usesyslog;
}

void SYMEXPORT alpm_option_add_noupgrade(const char *pkg)
{
	handle->noupgrade = alpm_list_add(handle->noupgrade, strdup(pkg));
}

void SYMEXPORT alpm_option_set_noupgrades(alpm_list_t *noupgrade)
{
	if(handle->noupgrade) FREELIST(handle->noupgrade);
	if(noupgrade) handle->noupgrade = noupgrade;
}

void SYMEXPORT alpm_option_add_noextract(const char *pkg)
{
	handle->noextract = alpm_list_add(handle->noextract, strdup(pkg));
}

void SYMEXPORT alpm_option_set_noextracts(alpm_list_t *noextract)
{
	if(handle->noextract) FREELIST(handle->noextract);
	if(noextract) handle->noextract = noextract;
}

void SYMEXPORT alpm_option_add_ignorepkg(const char *pkg)
{
	handle->ignorepkg = alpm_list_add(handle->ignorepkg, strdup(pkg));
}
void alpm_option_set_ignorepkgs(alpm_list_t *ignorepkgs)
{
	if(handle->ignorepkg) FREELIST(handle->ignorepkg);
	if(ignorepkgs) handle->ignorepkg = ignorepkgs;
}

void SYMEXPORT alpm_option_add_holdpkg(const char *pkg)
{
	handle->holdpkg = alpm_list_add(handle->holdpkg, strdup(pkg));
}
void SYMEXPORT alpm_option_set_holdpkgs(alpm_list_t *holdpkgs)
{
	if(handle->holdpkg) FREELIST(handle->holdpkg);
	if(holdpkgs) handle->holdpkg = holdpkgs;
}

void SYMEXPORT alpm_option_set_upgradedelay(time_t delay)
{
	handle->upgradedelay = delay;
}

void SYMEXPORT alpm_option_set_xfercommand(const char *cmd)
{
	if(handle->xfercommand) FREE(handle->xfercommand);
	if(cmd) handle->xfercommand = strdup(cmd);
}

void SYMEXPORT alpm_option_set_nopassiveftp(unsigned short nopasv)
{
	handle->nopassiveftp = nopasv;
}

/* vim: set ts=2 sw=2 noet: */
