/*
 *  handle.c
 *
 *  Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
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
#include <sys/stat.h>

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

	CALLOC(handle, 1, sizeof(pmhandle_t), RET_ERR(PM_ERR_MEMORY, NULL));

	handle->sigverify = PM_PGP_VERIFY_OPTIONAL;

	return handle;
}

void _alpm_handle_free(pmhandle_t *handle)
{
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

#ifdef HAVE_LIBCURL
	/* release curl handle */
	curl_easy_cleanup(handle->curl);
#endif

	/* free memory */
	_alpm_trans_free(handle->trans);
	FREE(handle->root);
	FREE(handle->dbpath);
	FREELIST(handle->cachedirs);
	FREE(handle->logfile);
	FREE(handle->lockfile);
	FREE(handle->arch);
	FREE(handle->signaturedir);
	FREELIST(handle->dbs_sync);
	FREELIST(handle->noupgrade);
	FREELIST(handle->noextract);
	FREELIST(handle->ignorepkg);
	FREELIST(handle->ignoregrp);
	FREE(handle);

}

alpm_cb_log SYMEXPORT alpm_option_get_logcb(pmhandle_t *handle)
{
	ASSERT(handle != NULL, return NULL);
	return handle->logcb;
}

alpm_cb_download SYMEXPORT alpm_option_get_dlcb(pmhandle_t *handle)
{
	ASSERT(handle != NULL, return NULL);
	return handle->dlcb;
}

alpm_cb_fetch SYMEXPORT alpm_option_get_fetchcb(pmhandle_t *handle)
{
	ASSERT(handle != NULL, return NULL);
	return handle->fetchcb;
}

alpm_cb_totaldl SYMEXPORT alpm_option_get_totaldlcb(pmhandle_t *handle)
{
	ASSERT(handle != NULL, return NULL);
	return handle->totaldlcb;
}

const char SYMEXPORT *alpm_option_get_root(pmhandle_t *handle)
{
	ASSERT(handle != NULL, return NULL);
	return handle->root;
}

const char SYMEXPORT *alpm_option_get_dbpath(pmhandle_t *handle)
{
	ASSERT(handle != NULL, return NULL);
	return handle->dbpath;
}

alpm_list_t SYMEXPORT *alpm_option_get_cachedirs(pmhandle_t *handle)
{
	ASSERT(handle != NULL, return NULL);
	return handle->cachedirs;
}

const char SYMEXPORT *alpm_option_get_logfile(pmhandle_t *handle)
{
	ASSERT(handle != NULL, return NULL);
	return handle->logfile;
}

const char SYMEXPORT *alpm_option_get_lockfile(pmhandle_t *handle)
{
	ASSERT(handle != NULL, return NULL);
	return handle->lockfile;
}

const char SYMEXPORT *alpm_option_get_signaturedir(pmhandle_t *handle)
{
	ASSERT(handle != NULL, return NULL);
	return handle->signaturedir;
}

int SYMEXPORT alpm_option_get_usesyslog(pmhandle_t *handle)
{
	ASSERT(handle != NULL, return -1);
	return handle->usesyslog;
}

alpm_list_t SYMEXPORT *alpm_option_get_noupgrades(pmhandle_t *handle)
{
	ASSERT(handle != NULL, return NULL);
	return handle->noupgrade;
}

alpm_list_t SYMEXPORT *alpm_option_get_noextracts(pmhandle_t *handle)
{
	ASSERT(handle != NULL, return NULL);
	return handle->noextract;
}

alpm_list_t SYMEXPORT *alpm_option_get_ignorepkgs(pmhandle_t *handle)
{
	ASSERT(handle != NULL, return NULL);
	return handle->ignorepkg;
}

alpm_list_t SYMEXPORT *alpm_option_get_ignoregrps(pmhandle_t *handle)
{
	ASSERT(handle != NULL, return NULL);
	return handle->ignoregrp;
}

const char SYMEXPORT *alpm_option_get_arch(pmhandle_t *handle)
{
	ASSERT(handle != NULL, return NULL);
	return handle->arch;
}

int SYMEXPORT alpm_option_get_usedelta(pmhandle_t *handle)
{
	ASSERT(handle != NULL, return -1);
	return handle->usedelta;
}

int SYMEXPORT alpm_option_get_checkspace(pmhandle_t *handle)
{
	ASSERT(handle != NULL, return -1);
	return handle->checkspace;
}

pmdb_t SYMEXPORT *alpm_option_get_localdb(pmhandle_t *handle)
{
	ASSERT(handle != NULL, return NULL);
	return handle->db_local;
}

alpm_list_t SYMEXPORT *alpm_option_get_syncdbs(pmhandle_t *handle)
{
	ASSERT(handle != NULL, return NULL);
	return handle->dbs_sync;
}

int SYMEXPORT alpm_option_set_logcb(pmhandle_t *handle, alpm_cb_log cb)
{
	ASSERT(handle != NULL, return -1);
	handle->logcb = cb;
	return 0;
}

int SYMEXPORT alpm_option_set_dlcb(pmhandle_t *handle, alpm_cb_download cb)
{
	ASSERT(handle != NULL, return -1);
	handle->dlcb = cb;
	return 0;
}

int SYMEXPORT alpm_option_set_fetchcb(pmhandle_t *handle, alpm_cb_fetch cb)
{
	ASSERT(handle != NULL, return -1);
	handle->fetchcb = cb;
	return 0;
}

int SYMEXPORT alpm_option_set_totaldlcb(pmhandle_t *handle, alpm_cb_totaldl cb)
{
	ASSERT(handle != NULL, return -1);
	handle->totaldlcb = cb;
	return 0;
}

static char *canonicalize_path(const char *path) {
	char *new_path;
	size_t len;

	/* verify path ends in a '/' */
	len = strlen(path);
	if(path[len - 1] != '/') {
		len += 1;
	}
	new_path = calloc(len + 1, sizeof(char));
	strncpy(new_path, path, len);
	new_path[len - 1] = '/';
	return new_path;
}

enum _pmerrno_t _alpm_set_directory_option(const char *value,
		char **storage, int must_exist)
 {
	struct stat st;
	char *real = NULL;
	const char *path;

	path = value;
	if(!path) {
		return PM_ERR_WRONG_ARGS;
	}
	if(must_exist) {
		if(stat(path, &st) == -1 || !S_ISDIR(st.st_mode)) {
			return PM_ERR_NOT_A_DIR;
		}
		real = calloc(PATH_MAX, sizeof(char));
		if(!realpath(path, real)) {
			free(real);
			return PM_ERR_NOT_A_DIR;
		}
		path = real;
	}

	if(*storage) {
		FREE(*storage);
	}
	*storage = canonicalize_path(path);
	free(real);
	return 0;
}

int SYMEXPORT alpm_option_add_cachedir(pmhandle_t *handle, const char *cachedir)
{
	char *newcachedir;

	ASSERT(handle != NULL, return -1);
	if(!cachedir) {
		pm_errno = PM_ERR_WRONG_ARGS;
		return -1;
	}
	/* don't stat the cachedir yet, as it may not even be needed. we can
	 * fail later if it is needed and the path is invalid. */

	newcachedir = canonicalize_path(cachedir);
	handle->cachedirs = alpm_list_add(handle->cachedirs, newcachedir);
	_alpm_log(PM_LOG_DEBUG, "backend option 'cachedir' = %s\n", newcachedir);
	return 0;
}

int SYMEXPORT alpm_option_set_cachedirs(pmhandle_t *handle, alpm_list_t *cachedirs)
{
	alpm_list_t *i;
	ASSERT(handle != NULL, return -1);
	if(handle->cachedirs) {
		FREELIST(handle->cachedirs);
	}
	for(i = cachedirs; i; i = i->next) {
		int ret = alpm_option_add_cachedir(handle, i->data);
		if(ret) {
			return ret;
		}
	}
	return 0;
}

int SYMEXPORT alpm_option_remove_cachedir(pmhandle_t *handle, const char *cachedir)
{
	char *vdata = NULL;
	char *newcachedir;
	size_t cachedirlen;
	ASSERT(handle != NULL, return -1);
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
		return 1;
	}
	return 0;
}

int SYMEXPORT alpm_option_set_logfile(pmhandle_t *handle, const char *logfile)
{
	char *oldlogfile = handle->logfile;

	ASSERT(handle != NULL, return -1);
	if(!logfile) {
		pm_errno = PM_ERR_WRONG_ARGS;
		return -1;
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
	return 0;
}

int SYMEXPORT alpm_option_set_signaturedir(pmhandle_t *handle, const char *signaturedir)
{
	ASSERT(handle != NULL, return -1);
	if(!signaturedir) {
		pm_errno = PM_ERR_WRONG_ARGS;
		return -1;
	}

	if(handle->signaturedir) {
		FREE(handle->signaturedir);
	}
	handle->signaturedir = strdup(signaturedir);

	_alpm_log(PM_LOG_DEBUG, "option 'signaturedir' = %s\n", handle->signaturedir);
	return 0;
}

int SYMEXPORT alpm_option_set_usesyslog(pmhandle_t *handle, int usesyslog)
{
	ASSERT(handle != NULL, return -1);
	handle->usesyslog = usesyslog;
	return 0;
}

int SYMEXPORT alpm_option_add_noupgrade(pmhandle_t *handle, const char *pkg)
{
	ASSERT(handle != NULL, return -1);
	handle->noupgrade = alpm_list_add(handle->noupgrade, strdup(pkg));
	return 0;
}

int SYMEXPORT alpm_option_set_noupgrades(pmhandle_t *handle, alpm_list_t *noupgrade)
{
	ASSERT(handle != NULL, return -1);
	if(handle->noupgrade) FREELIST(handle->noupgrade);
	handle->noupgrade = alpm_list_strdup(noupgrade);
	return 0;
}

int SYMEXPORT alpm_option_remove_noupgrade(pmhandle_t *handle, const char *pkg)
{
	char *vdata = NULL;
	ASSERT(handle != NULL, return -1);
	handle->noupgrade = alpm_list_remove_str(handle->noupgrade, pkg, &vdata);
	if(vdata != NULL) {
		FREE(vdata);
		return 1;
	}
	return 0;
}

int SYMEXPORT alpm_option_add_noextract(pmhandle_t *handle, const char *pkg)
{
	ASSERT(handle != NULL, return -1);
	handle->noextract = alpm_list_add(handle->noextract, strdup(pkg));
	return 0;
}

int SYMEXPORT alpm_option_set_noextracts(pmhandle_t *handle, alpm_list_t *noextract)
{
	ASSERT(handle != NULL, return -1);
	if(handle->noextract) FREELIST(handle->noextract);
	handle->noextract = alpm_list_strdup(noextract);
	return 0;
}

int SYMEXPORT alpm_option_remove_noextract(pmhandle_t *handle, const char *pkg)
{
	char *vdata = NULL;
	ASSERT(handle != NULL, return -1);
	handle->noextract = alpm_list_remove_str(handle->noextract, pkg, &vdata);
	if(vdata != NULL) {
		FREE(vdata);
		return 1;
	}
	return 0;
}

int SYMEXPORT alpm_option_add_ignorepkg(pmhandle_t *handle, const char *pkg)
{
	ASSERT(handle != NULL, return -1);
	handle->ignorepkg = alpm_list_add(handle->ignorepkg, strdup(pkg));
	return 0;
}

int SYMEXPORT alpm_option_set_ignorepkgs(pmhandle_t *handle, alpm_list_t *ignorepkgs)
{
	ASSERT(handle != NULL, return -1);
	if(handle->ignorepkg) FREELIST(handle->ignorepkg);
	handle->ignorepkg = alpm_list_strdup(ignorepkgs);
	return 0;
}

int SYMEXPORT alpm_option_remove_ignorepkg(pmhandle_t *handle, const char *pkg)
{
	char *vdata = NULL;
	ASSERT(handle != NULL, return -1);
	handle->ignorepkg = alpm_list_remove_str(handle->ignorepkg, pkg, &vdata);
	if(vdata != NULL) {
		FREE(vdata);
		return 1;
	}
	return 0;
}

int SYMEXPORT alpm_option_add_ignoregrp(pmhandle_t *handle, const char *grp)
{
	ASSERT(handle != NULL, return -1);
	handle->ignoregrp = alpm_list_add(handle->ignoregrp, strdup(grp));
	return 0;
}

int SYMEXPORT alpm_option_set_ignoregrps(pmhandle_t *handle, alpm_list_t *ignoregrps)
{
	ASSERT(handle != NULL, return -1);
	if(handle->ignoregrp) FREELIST(handle->ignoregrp);
	handle->ignoregrp = alpm_list_strdup(ignoregrps);
	return 0;
}

int SYMEXPORT alpm_option_remove_ignoregrp(pmhandle_t *handle, const char *grp)
{
	char *vdata = NULL;
	ASSERT(handle != NULL, return -1);
	handle->ignoregrp = alpm_list_remove_str(handle->ignoregrp, grp, &vdata);
	if(vdata != NULL) {
		FREE(vdata);
		return 1;
	}
	return 0;
}

int SYMEXPORT alpm_option_set_arch(pmhandle_t *handle, const char *arch)
{
	ASSERT(handle != NULL, return -1);
	if(handle->arch) FREE(handle->arch);
	if(arch) {
		handle->arch = strdup(arch);
	} else {
		handle->arch = NULL;
	}
	return 0;
}

int SYMEXPORT alpm_option_set_usedelta(pmhandle_t *handle, int usedelta)
{
	ASSERT(handle != NULL, return -1);
	handle->usedelta = usedelta;
	return 0;
}

int SYMEXPORT alpm_option_set_checkspace(pmhandle_t *handle, int checkspace)
{
	ASSERT(handle != NULL, return -1);
	handle->checkspace = checkspace;
	return 0;
}

int SYMEXPORT alpm_option_set_default_sigverify(pmhandle_t *handle, pgp_verify_t level)
{
	ASSERT(handle != NULL, return -1);
	ASSERT(level != PM_PGP_VERIFY_UNKNOWN, RET_ERR(PM_ERR_WRONG_ARGS, -1));
	handle->sigverify = level;
	return 0;
}

pgp_verify_t SYMEXPORT alpm_option_get_default_sigverify(pmhandle_t *handle)
{
	ASSERT(handle != NULL, return PM_PGP_VERIFY_UNKNOWN);
	return handle->sigverify;
}

/* vim: set ts=2 sw=2 noet: */
