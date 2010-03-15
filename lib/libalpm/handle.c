/*
 *  handle.c
 *
 *  Copyright (c) 2006-2010 Pacman Development Team <pacman-dev@archlinux.org>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <syslog.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

/* libalpm */
#include "handle.h"
#include "alpm_list.h"
#include "util.h"
#include "log.h"
#include "trans.h"
#include "alpm.h"

/* global var for handle (private to libalpm) */
pmhandle_t *handle = NULL;

pmhandle_t *_alpm_handle_new()
{
	pmhandle_t *handle;

	ALPM_LOG_FUNC;

	CALLOC(handle, 1, sizeof(pmhandle_t), RET_ERR(PM_ERR_MEMORY, NULL));
	handle->lckfd = -1;

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
	FREE(handle->arch);
	FREELIST(handle->dbs_sync);
	FREELIST(handle->noupgrade);
	FREELIST(handle->noextract);
	FREELIST(handle->ignorepkg);
	FREELIST(handle->ignoregrp);
	FREE(handle);
}

alpm_cb_log SYMEXPORT alpm_option_get_logcb()
{
	if (handle == NULL) {
		pm_errno = PM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->logcb;
}

alpm_cb_download SYMEXPORT alpm_option_get_dlcb()
{
	if (handle == NULL) {
		pm_errno = PM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->dlcb;
}

alpm_cb_fetch SYMEXPORT alpm_option_get_fetchcb()
{
	if (handle == NULL) {
		pm_errno = PM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->fetchcb;
}

alpm_cb_totaldl SYMEXPORT alpm_option_get_totaldlcb()
{
	if (handle == NULL) {
		pm_errno = PM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->totaldlcb;
}

const char SYMEXPORT *alpm_option_get_root()
{
	if (handle == NULL) {
		pm_errno = PM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->root;
}

const char SYMEXPORT *alpm_option_get_dbpath()
{
	if (handle == NULL) {
		pm_errno = PM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->dbpath;
}

alpm_list_t SYMEXPORT *alpm_option_get_cachedirs()
{
	if (handle == NULL) {
		pm_errno = PM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->cachedirs;
}

const char SYMEXPORT *alpm_option_get_logfile()
{
	if (handle == NULL) {
		pm_errno = PM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->logfile;
}

const char SYMEXPORT *alpm_option_get_lockfile()
{
	if (handle == NULL) {
		pm_errno = PM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->lockfile;
}

int SYMEXPORT alpm_option_get_usesyslog()
{
	if (handle == NULL) {
		pm_errno = PM_ERR_HANDLE_NULL;
		return -1;
	}
	return handle->usesyslog;
}

alpm_list_t SYMEXPORT *alpm_option_get_noupgrades()
{
	if (handle == NULL) {
		pm_errno = PM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->noupgrade;
}

alpm_list_t SYMEXPORT *alpm_option_get_noextracts()
{
	if (handle == NULL) {
		pm_errno = PM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->noextract;
}

alpm_list_t SYMEXPORT *alpm_option_get_ignorepkgs()
{
	if (handle == NULL) {
		pm_errno = PM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->ignorepkg;
}

alpm_list_t SYMEXPORT *alpm_option_get_ignoregrps()
{
	if (handle == NULL) {
		pm_errno = PM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->ignoregrp;
}

const char SYMEXPORT *alpm_option_get_arch()
{
	if (handle == NULL) {
		pm_errno = PM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->arch;
}

int SYMEXPORT alpm_option_get_usedelta()
{
	if (handle == NULL) {
		pm_errno = PM_ERR_HANDLE_NULL;
		return -1;
	}
	return handle->usedelta;
}

pmdb_t SYMEXPORT *alpm_option_get_localdb()
{
	if (handle == NULL) {
		pm_errno = PM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->db_local;
}

alpm_list_t SYMEXPORT *alpm_option_get_syncdbs()
{
	if (handle == NULL) {
		pm_errno = PM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->dbs_sync;
}

void SYMEXPORT alpm_option_set_logcb(alpm_cb_log cb)
{
	if (handle == NULL) {
		pm_errno = PM_ERR_HANDLE_NULL;
		return;
	}
	handle->logcb = cb;
}

void SYMEXPORT alpm_option_set_dlcb(alpm_cb_download cb)
{
	if (handle == NULL) {
		pm_errno = PM_ERR_HANDLE_NULL;
		return;
	}
	handle->dlcb = cb;
}

void SYMEXPORT alpm_option_set_fetchcb(alpm_cb_fetch cb)
{
	if (handle == NULL) {
		pm_errno = PM_ERR_HANDLE_NULL;
		return;
	}
	handle->fetchcb = cb;
}

void SYMEXPORT alpm_option_set_totaldlcb(alpm_cb_totaldl cb)
{
	if (handle == NULL) {
		pm_errno = PM_ERR_HANDLE_NULL;
		return;
	}
	handle->totaldlcb = cb;
}

int SYMEXPORT alpm_option_set_root(const char *root)
{
	struct stat st;
	char *realroot;
	size_t rootlen;

	ALPM_LOG_FUNC;

	if(!root) {
		pm_errno = PM_ERR_WRONG_ARGS;
		return(-1);
	}
	if(stat(root, &st) == -1 || !S_ISDIR(st.st_mode)) {
		pm_errno = PM_ERR_NOT_A_DIR;
		return(-1);
	}

	realroot = calloc(PATH_MAX+1, sizeof(char));
	if(!realpath(root, realroot)) {
		FREE(realroot);
		pm_errno = PM_ERR_NOT_A_DIR;
		return(-1);
	}

	/* verify root ends in a '/' */
	rootlen = strlen(realroot);
	if(realroot[rootlen-1] != '/') {
		rootlen += 1;
	}
	if(handle->root) {
		FREE(handle->root);
	}
	handle->root = calloc(rootlen + 1, sizeof(char));
	strncpy(handle->root, realroot, rootlen);
	handle->root[rootlen-1] = '/';
	FREE(realroot);
	_alpm_log(PM_LOG_DEBUG, "option 'root' = %s\n", handle->root);
	return(0);
}

int SYMEXPORT alpm_option_set_dbpath(const char *dbpath)
{
	struct stat st;
	size_t dbpathlen, lockfilelen;
	const char *lf = "db.lck";

	ALPM_LOG_FUNC;

	if(!dbpath) {
		pm_errno = PM_ERR_WRONG_ARGS;
		return(-1);
	}
	if(stat(dbpath, &st) == -1 || !S_ISDIR(st.st_mode)) {
		pm_errno = PM_ERR_NOT_A_DIR;
		return(-1);
	}
	/* verify dbpath ends in a '/' */
	dbpathlen = strlen(dbpath);
	if(dbpath[dbpathlen-1] != '/') {
		dbpathlen += 1;
	}
	if(handle->dbpath) {
		FREE(handle->dbpath);
	}
	handle->dbpath = calloc(dbpathlen+1, sizeof(char));
	strncpy(handle->dbpath, dbpath, dbpathlen);
	handle->dbpath[dbpathlen-1] = '/';
	_alpm_log(PM_LOG_DEBUG, "option 'dbpath' = %s\n", handle->dbpath);

	if(handle->lockfile) {
		FREE(handle->lockfile);
	}
	lockfilelen = strlen(handle->dbpath) + strlen(lf) + 1;
	handle->lockfile = calloc(lockfilelen, sizeof(char));
	snprintf(handle->lockfile, lockfilelen, "%s%s", handle->dbpath, lf);
	_alpm_log(PM_LOG_DEBUG, "option 'lockfile' = %s\n", handle->lockfile);
	return(0);
}

int SYMEXPORT alpm_option_add_cachedir(const char *cachedir)
{
	char *newcachedir;
	size_t cachedirlen;

	ALPM_LOG_FUNC;

	if(!cachedir) {
		pm_errno = PM_ERR_WRONG_ARGS;
		return(-1);
	}
	/* don't stat the cachedir yet, as it may not even be needed. we can
	 * fail later if it is needed and the path is invalid. */

	/* verify cachedir ends in a '/' */
	cachedirlen = strlen(cachedir);
	if(cachedir[cachedirlen-1] != '/') {
		cachedirlen += 1;
	}
	newcachedir = calloc(cachedirlen + 1, sizeof(char));
	strncpy(newcachedir, cachedir, cachedirlen);
	newcachedir[cachedirlen-1] = '/';
	handle->cachedirs = alpm_list_add(handle->cachedirs, newcachedir);
	_alpm_log(PM_LOG_DEBUG, "option 'cachedir' = %s\n", newcachedir);
	return(0);
}

void SYMEXPORT alpm_option_set_cachedirs(alpm_list_t *cachedirs)
{
	if(handle->cachedirs) FREELIST(handle->cachedirs);
	if(cachedirs) handle->cachedirs = cachedirs;
}

int SYMEXPORT alpm_option_remove_cachedir(const char *cachedir)
{
	char *vdata = NULL;
	char *newcachedir;
	size_t cachedirlen;
	/* verify cachedir ends in a '/' */
	cachedirlen = strlen(cachedir);
	if(cachedir[cachedirlen-1] != '/') {
		cachedirlen += 1;
	}
	newcachedir = calloc(cachedirlen + 1, sizeof(char));
	strncpy(newcachedir, cachedir, cachedirlen);
	newcachedir[cachedirlen-1] = '/';
	handle->cachedirs = alpm_list_remove_str(handle->cachedirs, newcachedir, &vdata);
	FREE(newcachedir);
	if(vdata != NULL) {
		FREE(vdata);
		return(1);
	}
	return(0);
}

int SYMEXPORT alpm_option_set_logfile(const char *logfile)
{
	char *oldlogfile = handle->logfile;

	ALPM_LOG_FUNC;

	if(!logfile) {
		pm_errno = PM_ERR_WRONG_ARGS;
		return(-1);
	}

	handle->logfile = strdup(logfile);

	/* free the old logfile path string, and close the stream so logaction
	 * will reopen a new stream on the new logfile */
	if(oldlogfile) {
		FREE(oldlogfile);
	}
	if(handle->logstream) {
		fclose(handle->logstream);
		handle->logstream = NULL;
	}
	_alpm_log(PM_LOG_DEBUG, "option 'logfile' = %s\n", handle->logfile);
	return(0);
}

void SYMEXPORT alpm_option_set_usesyslog(int usesyslog)
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

int SYMEXPORT alpm_option_remove_noupgrade(const char *pkg)
{
	char *vdata = NULL;
	handle->noupgrade = alpm_list_remove_str(handle->noupgrade, pkg, &vdata);
	if(vdata != NULL) {
		FREE(vdata);
		return(1);
	}
	return(0);
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

int SYMEXPORT alpm_option_remove_noextract(const char *pkg)
{
	char *vdata = NULL;
	handle->noextract = alpm_list_remove_str(handle->noextract, pkg, &vdata);
	if(vdata != NULL) {
		FREE(vdata);
		return(1);
	}
	return(0);
}

void SYMEXPORT alpm_option_add_ignorepkg(const char *pkg)
{
	handle->ignorepkg = alpm_list_add(handle->ignorepkg, strdup(pkg));
}

void SYMEXPORT alpm_option_set_ignorepkgs(alpm_list_t *ignorepkgs)
{
	if(handle->ignorepkg) FREELIST(handle->ignorepkg);
	if(ignorepkgs) handle->ignorepkg = ignorepkgs;
}

int SYMEXPORT alpm_option_remove_ignorepkg(const char *pkg)
{
	char *vdata = NULL;
	handle->ignorepkg = alpm_list_remove_str(handle->ignorepkg, pkg, &vdata);
	if(vdata != NULL) {
		FREE(vdata);
		return(1);
	}
	return(0);
}

void SYMEXPORT alpm_option_add_ignoregrp(const char *grp)
{
	handle->ignoregrp = alpm_list_add(handle->ignoregrp, strdup(grp));
}

void SYMEXPORT alpm_option_set_ignoregrps(alpm_list_t *ignoregrps)
{
	if(handle->ignoregrp) FREELIST(handle->ignoregrp);
	if(ignoregrps) handle->ignoregrp = ignoregrps;
}

int SYMEXPORT alpm_option_remove_ignoregrp(const char *grp)
{
	char *vdata = NULL;
	handle->ignoregrp = alpm_list_remove_str(handle->ignoregrp, grp, &vdata);
	if(vdata != NULL) {
		FREE(vdata);
		return(1);
	}
	return(0);
}

void SYMEXPORT alpm_option_set_arch(const char *arch)
{
	if(handle->arch) FREE(handle->arch);
	if(arch) handle->arch = strdup(arch);
}

void SYMEXPORT alpm_option_set_usedelta(int usedelta)
{
	handle->usedelta = usedelta;
}

/* vim: set ts=2 sw=2 noet: */
