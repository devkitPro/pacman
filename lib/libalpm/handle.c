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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <syslog.h>
#include <sys/stat.h>
#include <fcntl.h>

/* libalpm */
#include "handle.h"
#include "alpm_list.h"
#include "util.h"
#include "log.h"
#include "trans.h"
#include "alpm.h"

pmhandle_t *_alpm_handle_new()
{
	pmhandle_t *handle;

	CALLOC(handle, 1, sizeof(pmhandle_t), return NULL);

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
	FREE(handle->gpgdir);
	FREELIST(handle->dbs_sync);
	FREELIST(handle->noupgrade);
	FREELIST(handle->noextract);
	FREELIST(handle->ignorepkg);
	FREELIST(handle->ignoregrp);
	FREE(handle);
}

/** Lock the database */
int _alpm_handle_lock(pmhandle_t *handle)
{
	int fd;
	char *dir, *ptr;

	ASSERT(handle->lockfile != NULL, return -1);
	ASSERT(handle->lckstream == NULL, return 0);

	/* create the dir of the lockfile first */
	dir = strdup(handle->lockfile);
	ptr = strrchr(dir, '/');
	if(ptr) {
		*ptr = '\0';
	}
	if(_alpm_makepath(dir)) {
		FREE(dir);
		return -1;
	}
	FREE(dir);

	do {
		fd = open(handle->lockfile, O_WRONLY | O_CREAT | O_EXCL, 0000);
	} while(fd == -1 && errno == EINTR);
	if(fd > 0) {
		FILE *f = fdopen(fd, "w");
		fprintf(f, "%ld\n", (long)getpid());
		fflush(f);
		fsync(fd);
		handle->lckstream = f;
		return 0;
	}
	return -1;
}

/** Remove a lock file */
int _alpm_handle_unlock(pmhandle_t *handle)
{
	ASSERT(handle->lockfile != NULL, return -1);
	ASSERT(handle->lckstream != NULL, return 0);

	if(handle->lckstream != NULL) {
		fclose(handle->lckstream);
		handle->lckstream = NULL;
	}
	if(unlink(handle->lockfile) && errno != ENOENT) {
		return -1;
	}
	return 0;
}


alpm_cb_log SYMEXPORT alpm_option_get_logcb(pmhandle_t *handle)
{
	CHECK_HANDLE(handle, return NULL);
	return handle->logcb;
}

alpm_cb_download SYMEXPORT alpm_option_get_dlcb(pmhandle_t *handle)
{
	CHECK_HANDLE(handle, return NULL);
	return handle->dlcb;
}

alpm_cb_fetch SYMEXPORT alpm_option_get_fetchcb(pmhandle_t *handle)
{
	CHECK_HANDLE(handle, return NULL);
	return handle->fetchcb;
}

alpm_cb_totaldl SYMEXPORT alpm_option_get_totaldlcb(pmhandle_t *handle)
{
	CHECK_HANDLE(handle, return NULL);
	return handle->totaldlcb;
}

const char SYMEXPORT *alpm_option_get_root(pmhandle_t *handle)
{
	CHECK_HANDLE(handle, return NULL);
	return handle->root;
}

const char SYMEXPORT *alpm_option_get_dbpath(pmhandle_t *handle)
{
	CHECK_HANDLE(handle, return NULL);
	return handle->dbpath;
}

alpm_list_t SYMEXPORT *alpm_option_get_cachedirs(pmhandle_t *handle)
{
	CHECK_HANDLE(handle, return NULL);
	return handle->cachedirs;
}

const char SYMEXPORT *alpm_option_get_logfile(pmhandle_t *handle)
{
	CHECK_HANDLE(handle, return NULL);
	return handle->logfile;
}

const char SYMEXPORT *alpm_option_get_lockfile(pmhandle_t *handle)
{
	CHECK_HANDLE(handle, return NULL);
	return handle->lockfile;
}

const char SYMEXPORT *alpm_option_get_gpgdir(pmhandle_t *handle)
{
	CHECK_HANDLE(handle, return NULL);
	return handle->gpgdir;
}

int SYMEXPORT alpm_option_get_usesyslog(pmhandle_t *handle)
{
	CHECK_HANDLE(handle, return -1);
	return handle->usesyslog;
}

alpm_list_t SYMEXPORT *alpm_option_get_noupgrades(pmhandle_t *handle)
{
	CHECK_HANDLE(handle, return NULL);
	return handle->noupgrade;
}

alpm_list_t SYMEXPORT *alpm_option_get_noextracts(pmhandle_t *handle)
{
	CHECK_HANDLE(handle, return NULL);
	return handle->noextract;
}

alpm_list_t SYMEXPORT *alpm_option_get_ignorepkgs(pmhandle_t *handle)
{
	CHECK_HANDLE(handle, return NULL);
	return handle->ignorepkg;
}

alpm_list_t SYMEXPORT *alpm_option_get_ignoregrps(pmhandle_t *handle)
{
	CHECK_HANDLE(handle, return NULL);
	return handle->ignoregrp;
}

const char SYMEXPORT *alpm_option_get_arch(pmhandle_t *handle)
{
	CHECK_HANDLE(handle, return NULL);
	return handle->arch;
}

int SYMEXPORT alpm_option_get_usedelta(pmhandle_t *handle)
{
	CHECK_HANDLE(handle, return -1);
	return handle->usedelta;
}

int SYMEXPORT alpm_option_get_checkspace(pmhandle_t *handle)
{
	CHECK_HANDLE(handle, return -1);
	return handle->checkspace;
}

pmdb_t SYMEXPORT *alpm_option_get_localdb(pmhandle_t *handle)
{
	CHECK_HANDLE(handle, return NULL);
	return handle->db_local;
}

alpm_list_t SYMEXPORT *alpm_option_get_syncdbs(pmhandle_t *handle)
{
	CHECK_HANDLE(handle, return NULL);
	return handle->dbs_sync;
}

int SYMEXPORT alpm_option_set_logcb(pmhandle_t *handle, alpm_cb_log cb)
{
	CHECK_HANDLE(handle, return -1);
	handle->logcb = cb;
	return 0;
}

int SYMEXPORT alpm_option_set_dlcb(pmhandle_t *handle, alpm_cb_download cb)
{
	CHECK_HANDLE(handle, return -1);
	handle->dlcb = cb;
	return 0;
}

int SYMEXPORT alpm_option_set_fetchcb(pmhandle_t *handle, alpm_cb_fetch cb)
{
	CHECK_HANDLE(handle, return -1);
	handle->fetchcb = cb;
	return 0;
}

int SYMEXPORT alpm_option_set_totaldlcb(pmhandle_t *handle, alpm_cb_totaldl cb)
{
	CHECK_HANDLE(handle, return -1);
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
	CALLOC(new_path, len + 1, sizeof(char), return NULL);
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
		CALLOC(real, PATH_MAX, sizeof(char), return PM_ERR_MEMORY);
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
	if(!*storage) {
		return PM_ERR_MEMORY;
	}
	free(real);
	return 0;
}

int SYMEXPORT alpm_option_add_cachedir(pmhandle_t *handle, const char *cachedir)
{
	char *newcachedir;

	CHECK_HANDLE(handle, return -1);
	ASSERT(cachedir != NULL, RET_ERR(handle, PM_ERR_WRONG_ARGS, -1));
	/* don't stat the cachedir yet, as it may not even be needed. we can
	 * fail later if it is needed and the path is invalid. */

	newcachedir = canonicalize_path(cachedir);
	if(!newcachedir) {
		RET_ERR(handle, PM_ERR_MEMORY, -1);
	}
	handle->cachedirs = alpm_list_add(handle->cachedirs, newcachedir);
	_alpm_log(handle, PM_LOG_DEBUG, "option 'cachedir' = %s\n", newcachedir);
	return 0;
}

int SYMEXPORT alpm_option_set_cachedirs(pmhandle_t *handle, alpm_list_t *cachedirs)
{
	alpm_list_t *i;
	CHECK_HANDLE(handle, return -1);
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
	CHECK_HANDLE(handle, return -1);
	ASSERT(cachedir != NULL, RET_ERR(handle, PM_ERR_WRONG_ARGS, -1));

	newcachedir = canonicalize_path(cachedir);
	if(!newcachedir) {
		RET_ERR(handle, PM_ERR_MEMORY, -1);
	}
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

	CHECK_HANDLE(handle, return -1);
	if(!logfile) {
		handle->pm_errno = PM_ERR_WRONG_ARGS;
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
	_alpm_log(handle, PM_LOG_DEBUG, "option 'logfile' = %s\n", handle->logfile);
	return 0;
}

int SYMEXPORT alpm_option_set_gpgdir(pmhandle_t *handle, const char *gpgdir)
{
	CHECK_HANDLE(handle, return -1);
	if(!gpgdir) {
		handle->pm_errno = PM_ERR_WRONG_ARGS;
		return -1;
	}

	if(handle->gpgdir) {
		FREE(handle->gpgdir);
	}
	handle->gpgdir = strdup(gpgdir);

	_alpm_log(handle, PM_LOG_DEBUG, "option 'gpgdir' = %s\n", handle->gpgdir);
	return 0;
}

int SYMEXPORT alpm_option_set_usesyslog(pmhandle_t *handle, int usesyslog)
{
	CHECK_HANDLE(handle, return -1);
	handle->usesyslog = usesyslog;
	return 0;
}

int SYMEXPORT alpm_option_add_noupgrade(pmhandle_t *handle, const char *pkg)
{
	CHECK_HANDLE(handle, return -1);
	handle->noupgrade = alpm_list_add(handle->noupgrade, strdup(pkg));
	return 0;
}

int SYMEXPORT alpm_option_set_noupgrades(pmhandle_t *handle, alpm_list_t *noupgrade)
{
	CHECK_HANDLE(handle, return -1);
	if(handle->noupgrade) FREELIST(handle->noupgrade);
	handle->noupgrade = alpm_list_strdup(noupgrade);
	return 0;
}

int SYMEXPORT alpm_option_remove_noupgrade(pmhandle_t *handle, const char *pkg)
{
	char *vdata = NULL;
	CHECK_HANDLE(handle, return -1);
	handle->noupgrade = alpm_list_remove_str(handle->noupgrade, pkg, &vdata);
	if(vdata != NULL) {
		FREE(vdata);
		return 1;
	}
	return 0;
}

int SYMEXPORT alpm_option_add_noextract(pmhandle_t *handle, const char *pkg)
{
	CHECK_HANDLE(handle, return -1);
	handle->noextract = alpm_list_add(handle->noextract, strdup(pkg));
	return 0;
}

int SYMEXPORT alpm_option_set_noextracts(pmhandle_t *handle, alpm_list_t *noextract)
{
	CHECK_HANDLE(handle, return -1);
	if(handle->noextract) FREELIST(handle->noextract);
	handle->noextract = alpm_list_strdup(noextract);
	return 0;
}

int SYMEXPORT alpm_option_remove_noextract(pmhandle_t *handle, const char *pkg)
{
	char *vdata = NULL;
	CHECK_HANDLE(handle, return -1);
	handle->noextract = alpm_list_remove_str(handle->noextract, pkg, &vdata);
	if(vdata != NULL) {
		FREE(vdata);
		return 1;
	}
	return 0;
}

int SYMEXPORT alpm_option_add_ignorepkg(pmhandle_t *handle, const char *pkg)
{
	CHECK_HANDLE(handle, return -1);
	handle->ignorepkg = alpm_list_add(handle->ignorepkg, strdup(pkg));
	return 0;
}

int SYMEXPORT alpm_option_set_ignorepkgs(pmhandle_t *handle, alpm_list_t *ignorepkgs)
{
	CHECK_HANDLE(handle, return -1);
	if(handle->ignorepkg) FREELIST(handle->ignorepkg);
	handle->ignorepkg = alpm_list_strdup(ignorepkgs);
	return 0;
}

int SYMEXPORT alpm_option_remove_ignorepkg(pmhandle_t *handle, const char *pkg)
{
	char *vdata = NULL;
	CHECK_HANDLE(handle, return -1);
	handle->ignorepkg = alpm_list_remove_str(handle->ignorepkg, pkg, &vdata);
	if(vdata != NULL) {
		FREE(vdata);
		return 1;
	}
	return 0;
}

int SYMEXPORT alpm_option_add_ignoregrp(pmhandle_t *handle, const char *grp)
{
	CHECK_HANDLE(handle, return -1);
	handle->ignoregrp = alpm_list_add(handle->ignoregrp, strdup(grp));
	return 0;
}

int SYMEXPORT alpm_option_set_ignoregrps(pmhandle_t *handle, alpm_list_t *ignoregrps)
{
	CHECK_HANDLE(handle, return -1);
	if(handle->ignoregrp) FREELIST(handle->ignoregrp);
	handle->ignoregrp = alpm_list_strdup(ignoregrps);
	return 0;
}

int SYMEXPORT alpm_option_remove_ignoregrp(pmhandle_t *handle, const char *grp)
{
	char *vdata = NULL;
	CHECK_HANDLE(handle, return -1);
	handle->ignoregrp = alpm_list_remove_str(handle->ignoregrp, grp, &vdata);
	if(vdata != NULL) {
		FREE(vdata);
		return 1;
	}
	return 0;
}

int SYMEXPORT alpm_option_set_arch(pmhandle_t *handle, const char *arch)
{
	CHECK_HANDLE(handle, return -1);
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
	CHECK_HANDLE(handle, return -1);
	handle->usedelta = usedelta;
	return 0;
}

int SYMEXPORT alpm_option_set_checkspace(pmhandle_t *handle, int checkspace)
{
	CHECK_HANDLE(handle, return -1);
	handle->checkspace = checkspace;
	return 0;
}

int SYMEXPORT alpm_option_set_default_sigverify(pmhandle_t *handle, pgp_verify_t level)
{
	CHECK_HANDLE(handle, return -1);
	ASSERT(level != PM_PGP_VERIFY_UNKNOWN, RET_ERR(handle, PM_ERR_WRONG_ARGS, -1));
	handle->sigverify = level;
	return 0;
}

pgp_verify_t SYMEXPORT alpm_option_get_default_sigverify(pmhandle_t *handle)
{
	CHECK_HANDLE(handle, return PM_PGP_VERIFY_UNKNOWN);
	return handle->sigverify;
}

/* vim: set ts=2 sw=2 noet: */
