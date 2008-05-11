/*
 *  be_files.c
 *
 *  Copyright (c) 2006 by Christian Hamar <krics@linuxforum.hu>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h> /* intmax_t */
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <time.h>
#include <limits.h> /* PATH_MAX */
#include <locale.h> /* setlocale */

/* libarchive */
#include <archive.h>
#include <archive_entry.h>

/* libalpm */
#include "db.h"
#include "alpm_list.h"
#include "cache.h"
#include "log.h"
#include "util.h"
#include "alpm.h"
#include "handle.h"
#include "package.h"
#include "delta.h"
#include "deps.h"
#include "dload.h"


static int checkdbdir(pmdb_t *db)
{
	struct stat buf;
	const char *path = _alpm_db_path(db);

	if(stat(path, &buf) != 0) {
		_alpm_log(PM_LOG_DEBUG, "database dir '%s' does not exist, creating it\n",
				path);
		if(_alpm_makepath(path) != 0) {
			RET_ERR(PM_ERR_SYSTEM, -1);
		}
	} else if(!S_ISDIR(buf.st_mode)) {
		_alpm_log(PM_LOG_WARNING, _("removing invalid database: %s\n"), path);
		if(unlink(path) != 0 || _alpm_makepath(path) != 0) {
			RET_ERR(PM_ERR_SYSTEM, -1);
		}
	}
	return(0);
}

/* create list of directories in db */
static int dirlist_from_tar(const char *archive, alpm_list_t **dirlist)
{
	struct archive *_archive;
	struct archive_entry *entry;

	if((_archive = archive_read_new()) == NULL)
		RET_ERR(PM_ERR_LIBARCHIVE, -1);

	archive_read_support_compression_all(_archive);
	archive_read_support_format_all(_archive);

	if(archive_read_open_filename(_archive, archive,
				ARCHIVE_DEFAULT_BYTES_PER_BLOCK) != ARCHIVE_OK) {
		_alpm_log(PM_LOG_ERROR, _("could not open %s: %s\n"), archive,
				archive_error_string(_archive));
		RET_ERR(PM_ERR_PKG_OPEN, -1);
	}

	while(archive_read_next_header(_archive, &entry) == ARCHIVE_OK) {
		const struct stat *st;
		const char *entryname; /* the name of the file in the archive */

		st = archive_entry_stat(entry);
		entryname = archive_entry_pathname(entry);

		if(S_ISDIR(st->st_mode)) {
			char *name = strdup(entryname);
			*dirlist = alpm_list_add(*dirlist, name);
		}
	}
	archive_read_finish(_archive);

	*dirlist = alpm_list_msort(*dirlist, alpm_list_count(*dirlist), _alpm_str_cmp);
	return(0);
}

static int is_dir(const char *path, struct dirent *entry)
{
#ifdef DT_DIR
	return(entry->d_type == DT_DIR);
#else
	char buffer[PATH_MAX];
	snprintf(buffer, PATH_MAX, "%s/%s", path, entry->d_name);

	struct stat sbuf;
	if (!stat(buffer, &sbuf)) {
		return(S_ISDIR(sbuf.st_mode));
	}

	return(0);
#endif
}

/* create list of directories in db */
static int dirlist_from_fs(const char *syncdbpath, alpm_list_t **dirlist)
{
	DIR *dbdir;
	struct dirent *ent = NULL;

	dbdir = opendir(syncdbpath);
	if (dbdir != NULL) {
		while((ent = readdir(dbdir)) != NULL) {
			char *name = ent->d_name;
			size_t len;
			char *entry;

			if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
				continue;
			}

			if(!is_dir(syncdbpath, ent)) {
				continue;
			}

			len = strlen(name);
			MALLOC(entry, len + 2, RET_ERR(PM_ERR_MEMORY, -1));
			strcpy(entry, name);
			entry[len] = '/';
			entry[len+1] = '\0';
			*dirlist = alpm_list_add(*dirlist, entry);
		}
		closedir(dbdir);
	}

	*dirlist = alpm_list_msort(*dirlist, alpm_list_count(*dirlist), _alpm_str_cmp);
	return(0);
}

/* remove old directories from dbdir */
static int remove_olddir(const char *syncdbpath, alpm_list_t *dirlist)
{
	alpm_list_t *i;
	for (i = dirlist; i; i = i->next) {
		const char *name = i->data;
		char *dbdir;
		size_t len = strlen(syncdbpath) + strlen(name) + 2;
		MALLOC(dbdir, len, RET_ERR(PM_ERR_MEMORY, -1));
		snprintf(dbdir, len, "%s%s", syncdbpath, name);
		_alpm_log(PM_LOG_DEBUG, "removing: %s\n", dbdir);
		if(_alpm_rmrf(dbdir) != 0) {
			_alpm_log(PM_LOG_ERROR, _("could not remove database directory %s\n"), dbdir);
			free(dbdir);
			RET_ERR(PM_ERR_DB_REMOVE, -1);
		}
		free(dbdir);
	}
	return(0);
}

/** Update a package database
 *
 * An update of the package database \a db will be attempted. Unless
 * \a force is true, the update will only be performed if the remote
 * database was modified since the last update.
 *
 * A transaction is necessary for this operation, in order to obtain a
 * database lock. During this transaction the front-end will be informed
 * of the download progress of the database via the download callback.
 *
 * Example:
 * @code
 * pmdb_t *db;
 * int result;
 * db = alpm_list_getdata(alpm_option_get_syncdbs());
 * if(alpm_trans_init(0, NULL, NULL, NULL) == 0) {
 *     result = alpm_db_update(0, db);
 *     alpm_trans_release();
 *
 *     if(result > 0) {
 *	       printf("Unable to update database: %s\n", alpm_strerrorlast());
 *     } else if(result < 0) {
 *         printf("Database already up to date\n");
 *     } else {
 *         printf("Database updated\n");
 *     }
 * }
 * @endcode
 *
 * @ingroup alpm_databases
 * @note After a successful update, the \link alpm_db_get_pkgcache()
 * package cache \endlink will be invalidated
 * @param force if true, then forces the update, otherwise update only in case
 * the database isn't up to date
 * @param db pointer to the package database to update
 * @return 0 on success, > 0 on error (pm_errno is set accordingly), < 0 if up
 * to date
 */
int SYMEXPORT alpm_db_update(int force, pmdb_t *db)
{
	char *dbfile, *dbfilepath, *syncpath;
	const char *dbpath, *syncdbpath;
	alpm_list_t *newdirlist = NULL, *olddirlist = NULL;
	alpm_list_t *onlynew = NULL, *onlyold = NULL;
	size_t len;
	int ret;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	ASSERT(db != NULL && db != handle->db_local, RET_ERR(PM_ERR_WRONG_ARGS, -1));
	/* Verify we are in a transaction.  This is done _mainly_ because we need a DB
	 * lock - if we update without a db lock, we may kludge some other pacman
	 * process that _has_ a lock.
	 */
	ASSERT(handle->trans != NULL, RET_ERR(PM_ERR_TRANS_NULL, -1));
	ASSERT(handle->trans->state == STATE_INITIALIZED, RET_ERR(PM_ERR_TRANS_NOT_INITIALIZED, -1));

	if(!alpm_list_find_ptr(handle->dbs_sync, db)) {
		RET_ERR(PM_ERR_DB_NOT_FOUND, -1);
	}

	len = strlen(db->treename) + 4;
	MALLOC(dbfile, len, RET_ERR(PM_ERR_MEMORY, -1));
	sprintf(dbfile, "%s.db", db->treename);

	dbpath = alpm_option_get_dbpath();
	len = strlen(dbpath) + 6;
	MALLOC(syncpath, len, RET_ERR(PM_ERR_MEMORY, -1));
	sprintf(syncpath, "%s%s", dbpath, "sync/");

	ret = _alpm_download_single_file(dbfile, db->servers, syncpath, force);
	free(dbfile);
	free(syncpath);

	if(ret == 1) {
		/* files match, do nothing */
		pm_errno = 0;
		return(1);
	} else if(ret == -1) {
		/* pm_errno was set by the download code */
		_alpm_log(PM_LOG_DEBUG, "failed to sync db: %s\n", alpm_strerrorlast());
		return(-1);
	}

	syncdbpath = _alpm_db_path(db);

	/* form the path to the db location */
	len = strlen(dbpath) + strlen(db->treename) + 9;
	MALLOC(dbfilepath, len, RET_ERR(PM_ERR_MEMORY, -1));
	sprintf(dbfilepath, "%ssync/%s.db", dbpath, db->treename);

	if(force) {
		/* if forcing update, remove the old dir and extract the db */
		if(_alpm_rmrf(syncdbpath) != 0) {
			_alpm_log(PM_LOG_ERROR, _("could not remove database %s\n"), db->treename);
			RET_ERR(PM_ERR_DB_REMOVE, -1);
		} else {
			_alpm_log(PM_LOG_DEBUG, "database dir %s removed\n", _alpm_db_path(db));
		}
	} else {
		/* if not forcing, only remove and extract what is necessary */
		ret = dirlist_from_tar(dbfilepath, &newdirlist);
		if(ret) {
			goto cleanup;
		}
		ret = dirlist_from_fs(syncdbpath, &olddirlist);
		if(ret) {
			goto cleanup;
		}

		alpm_list_diff_sorted(olddirlist, newdirlist, _alpm_str_cmp, &onlyold, &onlynew);

		ret = remove_olddir(syncdbpath, onlyold);
		if(ret) {
			goto cleanup;
		}
	}

	/* Cache needs to be rebuilt */
	_alpm_db_free_pkgcache(db);

	checkdbdir(db);
	ret = _alpm_unpack(dbfilepath, syncdbpath, onlynew, 0);

cleanup:
	FREELIST(newdirlist);
	FREELIST(olddirlist);
	alpm_list_free(onlynew);
	alpm_list_free(onlyold);

	free(dbfilepath);

	if(ret) {
		RET_ERR(PM_ERR_SYSTEM, -1);
	}

	return(0);
}


static int splitname(const char *target, pmpkg_t *pkg)
{
	/* the format of a db entry is as follows:
	 *    package-version-rel/
	 * package name can contain hyphens, so parse from the back- go back
	 * two hyphens and we have split the version from the name.
	 */
	char *tmp, *p, *q;

	if(target == NULL || pkg == NULL) {
		return(-1);
	}
	STRDUP(tmp, target, RET_ERR(PM_ERR_MEMORY, -1));
	p = tmp + strlen(tmp);

	/* do the magic parsing- find the beginning of the version string
	 * by doing two iterations of same loop to lop off two hyphens */
	for(q = --p; *q && *q != '-'; q--);
	for(p = --q; *p && *p != '-'; p--);
	if(*p != '-' || p == tmp) {
		return(-1);
	}

	/* copy into fields and return */
	if(pkg->version) {
		FREE(pkg->version);
	}
	STRDUP(pkg->version, p+1, RET_ERR(PM_ERR_MEMORY, -1));
	/* insert a terminator at the end of the name (on hyphen)- then copy it */
	*p = '\0';
	if(pkg->name) {
		FREE(pkg->name);
	}
	STRDUP(pkg->name, tmp, RET_ERR(PM_ERR_MEMORY, -1));

	free(tmp);
	return(0);
}

int _alpm_db_populate(pmdb_t *db)
{
	int count = 0;
	struct dirent *ent = NULL;
	const char *dbpath;
	DIR *dbdir;

	ALPM_LOG_FUNC;

	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));

	dbpath = _alpm_db_path(db);
	dbdir = opendir(dbpath);
	if(dbdir == NULL) {
		return(0);
	}
	while((ent = readdir(dbdir)) != NULL) {
		const char *name = ent->d_name;
		pmpkg_t *pkg;

		if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
			continue;
		}
		if(!is_dir(dbpath, ent)) {
			continue;
		}

		pkg = _alpm_pkg_new();
		if(pkg == NULL) {
			closedir(dbdir);
			return(-1);
		}
		/* split the db entry name */
		if(splitname(name, pkg) != 0) {
			_alpm_log(PM_LOG_ERROR, _("invalid name for database entry '%s'\n"),
					name);
			_alpm_pkg_free(pkg);
			continue;
		}

		/* duplicated database entries are not allowed */
		if(_alpm_pkg_find(db->pkgcache, pkg->name)) {
			_alpm_log(PM_LOG_ERROR, _("duplicated database entry '%s'\n"), pkg->name);
			_alpm_pkg_free(pkg);
			continue;
		}

		/* explicitly read with only 'BASE' data, accessors will handle the rest */
		if(_alpm_db_read(db, pkg, INFRQ_BASE) == -1) {
			_alpm_log(PM_LOG_ERROR, _("corrupted database entry '%s'\n"), name);
			_alpm_pkg_free(pkg);
			continue;
		}
		if(db == handle->db_local) {
			pkg->origin = PKG_FROM_LOCALDB;
		}  else {
			pkg->origin = PKG_FROM_SYNCDB;
		}
		pkg->origin_data.db = db;
		/* add to the collection */
		_alpm_log(PM_LOG_FUNCTION, "adding '%s' to package cache for db '%s'\n",
				pkg->name, db->treename);
		db->pkgcache = alpm_list_add(db->pkgcache, pkg);
		count++;
	}

	closedir(dbdir);
	db->pkgcache = alpm_list_msort(db->pkgcache, count, _alpm_pkg_cmp);
	return(count);
}

/* Note: the return value must be freed by the caller */
static char *get_pkgpath(pmdb_t *db, pmpkg_t *info)
{
	size_t len;
	char *pkgpath;
	const char *dbpath;

	dbpath = _alpm_db_path(db);
	len = strlen(dbpath) + strlen(info->name) + strlen(info->version) + 3;
	MALLOC(pkgpath, len, RET_ERR(PM_ERR_MEMORY, NULL));
	sprintf(pkgpath, "%s%s-%s/", dbpath, info->name, info->version);
	return(pkgpath);
}

int _alpm_db_read(pmdb_t *db, pmpkg_t *info, pmdbinfrq_t inforeq)
{
	FILE *fp = NULL;
	char path[PATH_MAX];
	char line[1024];
	char *pkgpath = NULL;

	ALPM_LOG_FUNC;

	if(db == NULL) {
		RET_ERR(PM_ERR_DB_NULL, -1);
	}

	if(info == NULL || info->name == NULL || info->version == NULL) {
		_alpm_log(PM_LOG_DEBUG, "invalid package entry provided to _alpm_db_read, skipping\n");
		return(-1);
	}

	if(info->origin == PKG_FROM_FILE) {
		_alpm_log(PM_LOG_DEBUG, "request to read database info for a file-based package '%s', skipping...\n", info->name);
		return(-1);
	}

	/* bitmask logic here:
	 * infolevel: 00001111
	 * inforeq:   00010100
	 * & result:  00000100
	 * == to inforeq? nope, we need to load more info. */
	if((info->infolevel & inforeq) == inforeq) {
		/* already loaded this info, do nothing */
		return(0);
	}
	_alpm_log(PM_LOG_FUNCTION, "loading package data for %s : level=0x%x\n",
			info->name, inforeq);

	/* clear out 'line', to be certain - and to make valgrind happy */
	memset(line, 0, sizeof(line));

	pkgpath = get_pkgpath(db, info);

	if(access(pkgpath, F_OK)) {
		/* directory doesn't exist or can't be opened */
		_alpm_log(PM_LOG_DEBUG, "cannot find '%s-%s' in db '%s'\n",
				info->name, info->version, db->treename);
		goto error;
	}

	/* DESC */
	if(inforeq & INFRQ_DESC) {
		snprintf(path, PATH_MAX, "%sdesc", pkgpath);
		if((fp = fopen(path, "r")) == NULL) {
			_alpm_log(PM_LOG_ERROR, _("could not open file %s: %s\n"), path, strerror(errno));
			goto error;
		}
		while(!feof(fp)) {
			if(fgets(line, sizeof(line), fp) == NULL) {
				break;
			}
			_alpm_strtrim(line);
			if(strcmp(line, "%NAME%") == 0) {
				if(fgets(line, sizeof(line), fp) == NULL) {
					goto error;
				}
				if(strcmp(_alpm_strtrim(line), info->name) != 0) {
					_alpm_log(PM_LOG_ERROR, _("%s database is inconsistent: name "
								"mismatch on package %s\n"), db->treename, info->name);
				}
			} else if(strcmp(line, "%VERSION%") == 0) {
				if(fgets(line, sizeof(line), fp) == NULL) {
					goto error;
				}
				if(strcmp(_alpm_strtrim(line), info->version) != 0) {
					_alpm_log(PM_LOG_ERROR, _("%s database is inconsistent: version "
								"mismatch on package %s\n"), db->treename, info->name);
				}
			} else if(strcmp(line, "%FILENAME%") == 0) {
				if(fgets(line, sizeof(line), fp) == NULL) {
					goto error;
				}
				STRDUP(info->filename, _alpm_strtrim(line), goto error);
			} else if(strcmp(line, "%DESC%") == 0) {
				if(fgets(line, sizeof(line), fp) == NULL) {
					goto error;
				}
				STRDUP(info->desc, _alpm_strtrim(line), goto error);
			} else if(strcmp(line, "%GROUPS%") == 0) {
				while(fgets(line, sizeof(line), fp) && strlen(_alpm_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alpm_strtrim(line), goto error);
					info->groups = alpm_list_add(info->groups, linedup);
				}
			} else if(strcmp(line, "%URL%") == 0) {
				if(fgets(line, sizeof(line), fp) == NULL) {
					goto error;
				}
				STRDUP(info->url, _alpm_strtrim(line), goto error);
			} else if(strcmp(line, "%LICENSE%") == 0) {
				while(fgets(line, sizeof(line), fp) && strlen(_alpm_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alpm_strtrim(line), goto error);
					info->licenses = alpm_list_add(info->licenses, linedup);
				}
			} else if(strcmp(line, "%ARCH%") == 0) {
				if(fgets(line, sizeof(line), fp) == NULL) {
					goto error;
				}
				STRDUP(info->arch, _alpm_strtrim(line), goto error);
			} else if(strcmp(line, "%BUILDDATE%") == 0) {
				if(fgets(line, sizeof(line), fp) == NULL) {
					goto error;
				}
				_alpm_strtrim(line);

				char first = tolower((unsigned char)line[0]);
				if(first > 'a' && first < 'z') {
					struct tm tmp_tm = {0}; /* initialize to null in case of failure */
					setlocale(LC_TIME, "C");
					strptime(line, "%a %b %e %H:%M:%S %Y", &tmp_tm);
					info->builddate = mktime(&tmp_tm);
					setlocale(LC_TIME, "");
				} else {
					info->builddate = atol(line);
				}
			} else if(strcmp(line, "%INSTALLDATE%") == 0) {
				if(fgets(line, sizeof(line), fp) == NULL) {
					goto error;
				}
				_alpm_strtrim(line);

				char first = tolower((unsigned char)line[0]);
				if(first > 'a' && first < 'z') {
					struct tm tmp_tm = {0}; /* initialize to null in case of failure */
					setlocale(LC_TIME, "C");
					strptime(line, "%a %b %e %H:%M:%S %Y", &tmp_tm);
					info->installdate = mktime(&tmp_tm);
					setlocale(LC_TIME, "");
				} else {
					info->installdate = atol(line);
				}
			} else if(strcmp(line, "%PACKAGER%") == 0) {
				if(fgets(line, sizeof(line), fp) == NULL) {
					goto error;
				}
				STRDUP(info->packager, _alpm_strtrim(line), goto error);
			} else if(strcmp(line, "%REASON%") == 0) {
				if(fgets(line, sizeof(line), fp) == NULL) {
					goto error;
				}
				info->reason = (pmpkgreason_t)atol(_alpm_strtrim(line));
			} else if(strcmp(line, "%SIZE%") == 0 || strcmp(line, "%CSIZE%") == 0) {
				/* NOTE: the CSIZE and SIZE fields both share the "size" field
				 *       in the pkginfo_t struct.  This can be done b/c CSIZE
				 *       is currently only used in sync databases, and SIZE is
				 *       only used in local databases.
				 */
				if(fgets(line, sizeof(line), fp) == NULL) {
					goto error;
				}
				info->size = atol(_alpm_strtrim(line));
				/* also store this value to isize if isize is unset */
				if(info->isize == 0) {
					info->isize = info->size;
				}
			} else if(strcmp(line, "%ISIZE%") == 0) {
				/* ISIZE (installed size) tag only appears in sync repositories,
				 * not the local one. */
				if(fgets(line, sizeof(line), fp) == NULL) {
					goto error;
				}
				info->isize = atol(_alpm_strtrim(line));
			} else if(strcmp(line, "%MD5SUM%") == 0) {
				/* MD5SUM tag only appears in sync repositories,
				 * not the local one. */
				if(fgets(line, sizeof(line), fp) == NULL) {
					goto error;
				}
				STRDUP(info->md5sum, _alpm_strtrim(line), goto error);
			} else if(strcmp(line, "%REPLACES%") == 0) {
				while(fgets(line, sizeof(line), fp) && strlen(_alpm_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alpm_strtrim(line), goto error);
					info->replaces = alpm_list_add(info->replaces, linedup);
				}
			} else if(strcmp(line, "%FORCE%") == 0) {
				info->force = 1;
			}
		}
		fclose(fp);
		fp = NULL;
	}

	/* FILES */
	if(inforeq & INFRQ_FILES) {
		snprintf(path, PATH_MAX, "%sfiles", pkgpath);
		if((fp = fopen(path, "r")) == NULL) {
			_alpm_log(PM_LOG_ERROR, _("could not open file %s: %s\n"), path, strerror(errno));
			goto error;
		}
		while(fgets(line, sizeof(line), fp)) {
			_alpm_strtrim(line);
			if(strcmp(line, "%FILES%") == 0) {
				while(fgets(line, sizeof(line), fp) && strlen(_alpm_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alpm_strtrim(line), goto error);
					info->files = alpm_list_add(info->files, linedup);
				}
			} else if(strcmp(line, "%BACKUP%") == 0) {
				while(fgets(line, sizeof(line), fp) && strlen(_alpm_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alpm_strtrim(line), goto error);
					info->backup = alpm_list_add(info->backup, linedup);
				}
			}
		}
		fclose(fp);
		fp = NULL;
	}

	/* DEPENDS */
	if(inforeq & INFRQ_DEPENDS) {
		snprintf(path, PATH_MAX, "%sdepends", pkgpath);
		if((fp = fopen(path, "r")) == NULL) {
			_alpm_log(PM_LOG_ERROR, _("could not open file %s: %s\n"), path, strerror(errno));
			goto error;
		}
		while(!feof(fp)) {
			if(fgets(line, sizeof(line), fp) == NULL) {
				break;
			}
			_alpm_strtrim(line);
			if(strcmp(line, "%DEPENDS%") == 0) {
				while(fgets(line, sizeof(line), fp) && strlen(_alpm_strtrim(line))) {
					pmdepend_t *dep = _alpm_splitdep(_alpm_strtrim(line));
					info->depends = alpm_list_add(info->depends, dep);
				}
			} else if(strcmp(line, "%OPTDEPENDS%") == 0) {
				while(fgets(line, sizeof(line), fp) && strlen(_alpm_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alpm_strtrim(line), goto error);
					info->optdepends = alpm_list_add(info->optdepends, linedup);
				}
			} else if(strcmp(line, "%CONFLICTS%") == 0) {
				while(fgets(line, sizeof(line), fp) && strlen(_alpm_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alpm_strtrim(line), goto error);
					info->conflicts = alpm_list_add(info->conflicts, linedup);
				}
			} else if(strcmp(line, "%PROVIDES%") == 0) {
				while(fgets(line, sizeof(line), fp) && strlen(_alpm_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alpm_strtrim(line), goto error);
					info->provides = alpm_list_add(info->provides, linedup);
				}
			}
		}
		fclose(fp);
		fp = NULL;
	}

	/* DELTAS */
	if(inforeq & INFRQ_DELTAS) {
		snprintf(path, PATH_MAX, "%sdeltas", pkgpath);
		if((fp = fopen(path, "r"))) {
			while(!feof(fp)) {
				if(fgets(line, sizeof(line), fp) == NULL) {
					break;
				}
				_alpm_strtrim(line);
				if(strcmp(line, "%DELTAS%") == 0) {
					while(fgets(line, sizeof(line), fp) && strlen(_alpm_strtrim(line))) {
						pmdelta_t *delta = _alpm_delta_parse(line);
						if(delta) {
							info->deltas = alpm_list_add(info->deltas, delta);
						}
					}
				}
			}
			fclose(fp);
			fp = NULL;
		}
	}

	/* INSTALL */
	if(inforeq & INFRQ_SCRIPTLET) {
		snprintf(path, PATH_MAX, "%sinstall", pkgpath);
		if(access(path, F_OK) == 0) {
			info->scriptlet = 1;
		}
	}

	/* internal */
	info->infolevel |= inforeq;

	free(pkgpath);
	return(0);

error:
	free(pkgpath);
	if(fp) {
		fclose(fp);
	}
	return(-1);
}

int _alpm_db_prepare(pmdb_t *db, pmpkg_t *info)
{
	mode_t oldmask;
	int retval = 0;
	char *pkgpath = NULL;

	if(checkdbdir(db) != 0) {
		return(-1);
	}

	oldmask = umask(0000);
	pkgpath = get_pkgpath(db, info);

	if((retval = mkdir(pkgpath, 0755)) != 0) {
		_alpm_log(PM_LOG_ERROR, _("could not create directory %s: %s\n"),
				pkgpath, strerror(errno));
	}

	free(pkgpath);
	umask(oldmask);

	return(retval);
}

int _alpm_db_write(pmdb_t *db, pmpkg_t *info, pmdbinfrq_t inforeq)
{
	FILE *fp = NULL;
	char path[PATH_MAX];
	mode_t oldmask;
	alpm_list_t *lp = NULL;
	int retval = 0;
	int local = 0;
	char *pkgpath = NULL;

	ALPM_LOG_FUNC;

	if(db == NULL || info == NULL) {
		return(-1);
	}

	pkgpath = get_pkgpath(db, info);

	/* make sure we have a sane umask */
	oldmask = umask(0022);

	if(strcmp(db->treename, "local") == 0) {
		local = 1;
	}

	/* DESC */
	if(inforeq & INFRQ_DESC) {
		_alpm_log(PM_LOG_DEBUG, "writing %s-%s DESC information back to db\n",
				info->name, info->version);
		snprintf(path, PATH_MAX, "%sdesc", pkgpath);
		if((fp = fopen(path, "w")) == NULL) {
			_alpm_log(PM_LOG_ERROR, _("could not open file %s: %s\n"), path, strerror(errno));
			retval = -1;
			goto cleanup;
		}
		fprintf(fp, "%%NAME%%\n%s\n\n"
						"%%VERSION%%\n%s\n\n", info->name, info->version);
		if(info->desc) {
			fprintf(fp, "%%DESC%%\n"
							"%s\n\n", info->desc);
		}
		if(info->groups) {
			fputs("%GROUPS%\n", fp);
			for(lp = info->groups; lp; lp = lp->next) {
				fprintf(fp, "%s\n", (char *)lp->data);
			}
			fprintf(fp, "\n");
		}
		if(info->replaces) {
			fputs("%REPLACES%\n", fp);
			for(lp = info->replaces; lp; lp = lp->next) {
				fprintf(fp, "%s\n", (char *)lp->data);
			}
			fprintf(fp, "\n");
		}
		if(info->force) {
			fprintf(fp, "%%FORCE%%\n\n");
		}
		if(local) {
			if(info->url) {
				fprintf(fp, "%%URL%%\n"
								"%s\n\n", info->url);
			}
			if(info->licenses) {
				fputs("%LICENSE%\n", fp);
				for(lp = info->licenses; lp; lp = lp->next) {
					fprintf(fp, "%s\n", (char *)lp->data);
				}
				fprintf(fp, "\n");
			}
			if(info->arch) {
				fprintf(fp, "%%ARCH%%\n"
								"%s\n\n", info->arch);
			}
			if(info->builddate) {
				fprintf(fp, "%%BUILDDATE%%\n"
								"%ld\n\n", info->builddate);
			}
			if(info->installdate) {
				fprintf(fp, "%%INSTALLDATE%%\n"
								"%ld\n\n", info->installdate);
			}
			if(info->packager) {
				fprintf(fp, "%%PACKAGER%%\n"
								"%s\n\n", info->packager);
			}
			if(info->isize) {
				/* only write installed size, csize is irrelevant once installed */
				fprintf(fp, "%%SIZE%%\n"
								"%jd\n\n", (intmax_t)info->isize);
			}
			if(info->reason) {
				fprintf(fp, "%%REASON%%\n"
								"%u\n\n", info->reason);
			}
		} else {
			if(info->size) {
				fprintf(fp, "%%CSIZE%%\n"
								"%jd\n\n", (intmax_t)info->size);
			}
			if(info->isize) {
				fprintf(fp, "%%ISIZE%%\n"
								"%jd\n\n", (intmax_t)info->isize);
			}
			if(info->md5sum) {
				fprintf(fp, "%%MD5SUM%%\n"
								"%s\n\n", info->md5sum);
			}
		}
		fclose(fp);
		fp = NULL;
	}

	/* FILES */
	if(local && (inforeq & INFRQ_FILES)) {
		_alpm_log(PM_LOG_DEBUG, "writing %s-%s FILES information back to db\n",
				info->name, info->version);
		snprintf(path, PATH_MAX, "%sfiles", pkgpath);
		if((fp = fopen(path, "w")) == NULL) {
			_alpm_log(PM_LOG_ERROR, _("could not open file %s: %s\n"), path, strerror(errno));
			retval = -1;
			goto cleanup;
		}
		if(info->files) {
			fprintf(fp, "%%FILES%%\n");
			for(lp = info->files; lp; lp = lp->next) {
				fprintf(fp, "%s\n", (char *)lp->data);
			}
			fprintf(fp, "\n");
		}
		if(info->backup) {
			fprintf(fp, "%%BACKUP%%\n");
			for(lp = info->backup; lp; lp = lp->next) {
				fprintf(fp, "%s\n", (char *)lp->data);
			}
			fprintf(fp, "\n");
		}
		fclose(fp);
		fp = NULL;
	}

	/* DEPENDS */
	if(inforeq & INFRQ_DEPENDS) {
		_alpm_log(PM_LOG_DEBUG, "writing %s-%s DEPENDS information back to db\n",
			info->name, info->version);
		snprintf(path, PATH_MAX, "%sdepends", pkgpath);
		if((fp = fopen(path, "w")) == NULL) {
			_alpm_log(PM_LOG_ERROR, _("could not open file %s: %s\n"), path, strerror(errno));
			retval = -1;
			goto cleanup;
		}
		if(info->depends) {
			fputs("%DEPENDS%\n", fp);
			for(lp = info->depends; lp; lp = lp->next) {
				char *depstring = alpm_dep_compute_string(lp->data);
				fprintf(fp, "%s\n", depstring);
				free(depstring);
			}
			fprintf(fp, "\n");
		}
		if(info->optdepends) {
			fputs("%OPTDEPENDS%\n", fp);
			for(lp = info->optdepends; lp; lp = lp->next) {
				fprintf(fp, "%s\n", (char *)lp->data);
			}
			fprintf(fp, "\n");
		}
		if(info->conflicts) {
			fputs("%CONFLICTS%\n", fp);
			for(lp = info->conflicts; lp; lp = lp->next) {
				fprintf(fp, "%s\n", (char *)lp->data);
			}
			fprintf(fp, "\n");
		}
		if(info->provides) {
			fputs("%PROVIDES%\n", fp);
			for(lp = info->provides; lp; lp = lp->next) {
				fprintf(fp, "%s\n", (char *)lp->data);
			}
			fprintf(fp, "\n");
		}
		fclose(fp);
		fp = NULL;
	}

	/* INSTALL */
	/* nothing needed here (script is automatically extracted) */

cleanup:
	umask(oldmask);
	free(pkgpath);

	if(fp) {
		fclose(fp);
	}

	return(retval);
}

int _alpm_db_remove(pmdb_t *db, pmpkg_t *info)
{
	int ret = 0;
	char *pkgpath = NULL;

	ALPM_LOG_FUNC;

	if(db == NULL || info == NULL) {
		RET_ERR(PM_ERR_DB_NULL, -1);
	}

	pkgpath = get_pkgpath(db, info);

	ret = _alpm_rmrf(pkgpath);
	free(pkgpath);
	if(ret != 0) {
		ret = -1;
	}
	return(ret);
}

/* vim: set ts=2 sw=2 noet: */
