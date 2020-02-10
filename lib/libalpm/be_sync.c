/*
 *  be_sync.c : backend for sync databases
 *
 *  Copyright (c) 2006-2020 Pacman Development Team <pacman-dev@archlinux.org>
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

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>

/* libarchive */
#include <archive.h>
#include <archive_entry.h>

/* libalpm */
#include "util.h"
#include "log.h"
#include "libarchive-compat.h"
#include "alpm.h"
#include "alpm_list.h"
#include "package.h"
#include "handle.h"
#include "deps.h"
#include "dload.h"
#include "filelist.h"

static char *get_sync_dir(alpm_handle_t *handle)
{
	size_t len = strlen(handle->dbpath) + 6;
	char *syncpath;
	struct stat buf;

	MALLOC(syncpath, len, RET_ERR(handle, ALPM_ERR_MEMORY, NULL));
	sprintf(syncpath, "%s%s", handle->dbpath, "sync/");

	if(stat(syncpath, &buf) != 0) {
		_alpm_log(handle, ALPM_LOG_DEBUG, "database dir '%s' does not exist, creating it\n",
				syncpath);
		if(_alpm_makepath(syncpath) != 0) {
			free(syncpath);
			RET_ERR(handle, ALPM_ERR_SYSTEM, NULL);
		}
	} else if(!S_ISDIR(buf.st_mode)) {
		_alpm_log(handle, ALPM_LOG_WARNING, _("removing invalid file: %s\n"), syncpath);
		if(unlink(syncpath) != 0 || _alpm_makepath(syncpath) != 0) {
			free(syncpath);
			RET_ERR(handle, ALPM_ERR_SYSTEM, NULL);
		}
	}

	return syncpath;
}

static int sync_db_validate(alpm_db_t *db)
{
	int siglevel;
	const char *dbpath;

	if(db->status & DB_STATUS_VALID || db->status & DB_STATUS_MISSING) {
		return 0;
	}
	if(db->status & DB_STATUS_INVALID) {
		db->handle->pm_errno = ALPM_ERR_DB_INVALID_SIG;
		return -1;
	}

	dbpath = _alpm_db_path(db);
	if(!dbpath) {
		/* pm_errno set in _alpm_db_path() */
		return -1;
	}

	/* we can skip any validation if the database doesn't exist */
	if(_alpm_access(db->handle, NULL, dbpath, R_OK) != 0 && errno == ENOENT) {
		alpm_event_database_missing_t event = {
			.type = ALPM_EVENT_DATABASE_MISSING,
			.dbname = db->treename
		};
		db->status &= ~DB_STATUS_EXISTS;
		db->status |= DB_STATUS_MISSING;
		EVENT(db->handle, &event);
		goto valid;
	}
	db->status |= DB_STATUS_EXISTS;
	db->status &= ~DB_STATUS_MISSING;

	/* this takes into account the default verification level if UNKNOWN
	 * was assigned to this db */
	siglevel = alpm_db_get_siglevel(db);

	if(siglevel & ALPM_SIG_DATABASE) {
		int retry, ret;
		do {
			retry = 0;
			alpm_siglist_t *siglist;
			ret = _alpm_check_pgp_helper(db->handle, dbpath, NULL,
					siglevel & ALPM_SIG_DATABASE_OPTIONAL, siglevel & ALPM_SIG_DATABASE_MARGINAL_OK,
					siglevel & ALPM_SIG_DATABASE_UNKNOWN_OK, &siglist);
			if(ret) {
				retry = _alpm_process_siglist(db->handle, db->treename, siglist,
						siglevel & ALPM_SIG_DATABASE_OPTIONAL, siglevel & ALPM_SIG_DATABASE_MARGINAL_OK,
						siglevel & ALPM_SIG_DATABASE_UNKNOWN_OK);
			}
			alpm_siglist_cleanup(siglist);
			free(siglist);
		} while(retry);

		if(ret) {
			db->status &= ~DB_STATUS_VALID;
			db->status |= DB_STATUS_INVALID;
			db->handle->pm_errno = ALPM_ERR_DB_INVALID_SIG;
			return 1;
		}
	}

valid:
	db->status |= DB_STATUS_VALID;
	db->status &= ~DB_STATUS_INVALID;
	return 0;
}

/** Update a package database
 *
 * An update of the package database \a db will be attempted. Unless
 * \a force is true, the update will only be performed if the remote
 * database was modified since the last update.
 *
 * This operation requires a database lock, and will return an applicable error
 * if the lock could not be obtained.
 *
 * Example:
 * @code
 * alpm_list_t *syncs = alpm_get_syncdbs();
 * for(i = syncs; i; i = alpm_list_next(i)) {
 *     alpm_db_t *db = alpm_list_getdata(i);
 *     result = alpm_db_update(0, db);
 *
 *     if(result < 0) {
 *	       printf("Unable to update database: %s\n", alpm_strerrorlast());
 *     } else if(result == 1) {
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
 * @return 0 on success, -1 on error (pm_errno is set accordingly), 1 if up to
 * to date
 */
int SYMEXPORT alpm_db_update(int force, alpm_db_t *db)
{
	char *syncpath;
	const char *dbext;
	alpm_list_t *i;
	int updated = 0;
	int ret = -1;
	mode_t oldmask;
	alpm_handle_t *handle;
	int siglevel;

	/* Sanity checks */
	ASSERT(db != NULL, return -1);
	handle = db->handle;
	handle->pm_errno = ALPM_ERR_OK;
	ASSERT(db != handle->db_local, RET_ERR(handle, ALPM_ERR_WRONG_ARGS, -1));
	ASSERT(db->servers != NULL, RET_ERR(handle, ALPM_ERR_SERVER_NONE, -1));

	if(!(db->usage & ALPM_DB_USAGE_SYNC)) {
		return 0;
	}

	syncpath = get_sync_dir(handle);
	if(!syncpath) {
		return -1;
	}

	/* force update of invalid databases to fix potential mismatched database/signature */
	if(db->status & DB_STATUS_INVALID) {
		force = 1;
	}

	/* make sure we have a sane umask */
	oldmask = umask(0022);

	siglevel = alpm_db_get_siglevel(db);

	/* attempt to grab a lock */
	if(_alpm_handle_lock(handle)) {
		free(syncpath);
		umask(oldmask);
		RET_ERR(handle, ALPM_ERR_HANDLE_LOCK, -1);
	}

	dbext = db->handle->dbext;

	for(i = db->servers; i; i = i->next) {
		const char *server = i->data, *final_db_url = NULL;
		struct dload_payload payload;
		size_t len;
		int sig_ret = 0;

		memset(&payload, 0, sizeof(struct dload_payload));

		/* set hard upper limit of 128MiB */
		payload.max_size = 128 * 1024 * 1024;

		/* print server + filename into a buffer */
		len = strlen(server) + strlen(db->treename) + strlen(dbext) + 2;
		MALLOC(payload.fileurl, len,
			{
				free(syncpath);
				umask(oldmask);
				RET_ERR(handle, ALPM_ERR_MEMORY, -1);
			}
		);
		snprintf(payload.fileurl, len, "%s/%s%s", server, db->treename, dbext);
		payload.handle = handle;
		payload.force = force;
		payload.unlink_on_fail = 1;

		ret = _alpm_download(&payload, syncpath, NULL, &final_db_url);
		_alpm_dload_payload_reset(&payload);
		updated = (updated || ret == 0);

		if(ret != -1 && updated && (siglevel & ALPM_SIG_DATABASE)) {
			/* an existing sig file is no good at this point */
			char *sigpath = _alpm_sigpath(handle, _alpm_db_path(db));
			if(!sigpath) {
				ret = -1;
				break;
			}
			unlink(sigpath);
			free(sigpath);


			/* check if the final URL from internal downloader looks reasonable */
			if(final_db_url != NULL) {
				if(strlen(final_db_url) < 3
						|| strcmp(final_db_url + strlen(final_db_url) - strlen(dbext),
								dbext) != 0) {
					final_db_url = NULL;
				}
			}

			/* if we downloaded a DB, we want the .sig from the same server */
			if(final_db_url != NULL) {
				/* print final_db_url into a buffer (leave space for .sig) */
				len = strlen(final_db_url) + 5;
			} else {
				/* print server + filename into a buffer (leave space for separator and .sig) */
				len = strlen(server) + strlen(db->treename) + strlen(dbext) + 6;
			}

			MALLOC(payload.fileurl, len,
				{
					free(syncpath);
					umask(oldmask);
					RET_ERR(handle, ALPM_ERR_MEMORY, -1);
				}
			);

			if(final_db_url != NULL) {
				snprintf(payload.fileurl, len, "%s.sig", final_db_url);
			} else {
				snprintf(payload.fileurl, len, "%s/%s%s.sig", server, db->treename, dbext);
			}

			payload.handle = handle;
			payload.force = 1;
			payload.errors_ok = (siglevel & ALPM_SIG_DATABASE_OPTIONAL);

			/* set hard upper limit of 16KiB */
			payload.max_size = 16 * 1024;

			sig_ret = _alpm_download(&payload, syncpath, NULL, NULL);
			/* errors_ok suppresses error messages, but not the return code */
			sig_ret = payload.errors_ok ? 0 : sig_ret;
			_alpm_dload_payload_reset(&payload);
		}

		if(ret != -1 && sig_ret != -1) {
			break;
		}
	}

	if(updated) {
		/* Cache needs to be rebuilt */
		_alpm_db_free_pkgcache(db);

		/* clear all status flags regarding validity/existence */
		db->status &= ~DB_STATUS_VALID;
		db->status &= ~DB_STATUS_INVALID;
		db->status &= ~DB_STATUS_EXISTS;
		db->status &= ~DB_STATUS_MISSING;

		/* if the download failed skip validation to preserve the download error */
		if(ret != -1 && sync_db_validate(db) != 0) {
			/* pm_errno should be set */
			ret = -1;
		}
	}

	if(ret == -1) {
		/* pm_errno was set by the download code */
		_alpm_log(handle, ALPM_LOG_DEBUG, "failed to sync db: %s\n",
				alpm_strerror(handle->pm_errno));
	} else {
		handle->pm_errno = ALPM_ERR_OK;
	}

	_alpm_handle_unlock(handle);
	free(syncpath);
	umask(oldmask);
	return ret;
}

/* Forward decl so I don't reorganize the whole file right now */
static int sync_db_read(alpm_db_t *db, struct archive *archive,
		struct archive_entry *entry, alpm_pkg_t **likely_pkg);

static int _sync_get_validation(alpm_pkg_t *pkg)
{
	if(pkg->validation) {
		return pkg->validation;
	}

	if(pkg->md5sum) {
		pkg->validation |= ALPM_PKG_VALIDATION_MD5SUM;
	}
	if(pkg->sha256sum) {
		pkg->validation |= ALPM_PKG_VALIDATION_SHA256SUM;
	}
	if(pkg->base64_sig) {
		pkg->validation |= ALPM_PKG_VALIDATION_SIGNATURE;
	}

	if(!pkg->validation) {
		pkg->validation |= ALPM_PKG_VALIDATION_NONE;
	}

	return pkg->validation;
}

static alpm_pkg_t *load_pkg_for_entry(alpm_db_t *db, const char *entryname,
		const char **entry_filename, alpm_pkg_t *likely_pkg)
{
	char *pkgname = NULL, *pkgver = NULL;
	unsigned long pkgname_hash;
	alpm_pkg_t *pkg;

	/* get package and db file names */
	if(entry_filename) {
		char *fname = strrchr(entryname, '/');
		if(fname) {
			*entry_filename = fname + 1;
		} else {
			*entry_filename = NULL;
		}
	}
	if(_alpm_splitname(entryname, &pkgname, &pkgver, &pkgname_hash) != 0) {
		_alpm_log(db->handle, ALPM_LOG_ERROR,
				_("invalid name for database entry '%s'\n"), entryname);
		return NULL;
	}

	if(likely_pkg && pkgname_hash == likely_pkg->name_hash
			&& strcmp(likely_pkg->name, pkgname) == 0) {
		pkg = likely_pkg;
	} else {
		pkg = _alpm_pkghash_find(db->pkgcache, pkgname);
	}
	if(pkg == NULL) {
		pkg = _alpm_pkg_new();
		if(pkg == NULL) {
			RET_ERR(db->handle, ALPM_ERR_MEMORY, NULL);
		}

		pkg->name = pkgname;
		pkg->version = pkgver;
		pkg->name_hash = pkgname_hash;

		pkg->origin = ALPM_PKG_FROM_SYNCDB;
		pkg->origin_data.db = db;
		pkg->ops = &default_pkg_ops;
		pkg->ops->get_validation = _sync_get_validation;
		pkg->handle = db->handle;

		/* add to the collection */
		_alpm_log(db->handle, ALPM_LOG_FUNCTION, "adding '%s' to package cache for db '%s'\n",
				pkg->name, db->treename);
		if(_alpm_pkghash_add(&db->pkgcache, pkg) == NULL) {
			_alpm_pkg_free(pkg);
			RET_ERR(db->handle, ALPM_ERR_MEMORY, NULL);
		}
	} else {
		free(pkgname);
		free(pkgver);
	}

	return pkg;
}

/* This function doesn't work as well as one might think, as size of database
 * entries varies considerably. Adding signatures nearly doubles the size of a
 * single entry. These  current values are heavily influenced by Arch Linux;
 * databases with a single signature per package. */
static size_t estimate_package_count(struct stat *st, struct archive *archive)
{
	int per_package;

	switch(_alpm_archive_filter_code(archive)) {
		case ARCHIVE_COMPRESSION_NONE:
			per_package = 3015;
			break;
		case ARCHIVE_COMPRESSION_GZIP:
		case ARCHIVE_COMPRESSION_COMPRESS:
			per_package = 464;
			break;
		case ARCHIVE_COMPRESSION_BZIP2:
			per_package = 394;
			break;
		case ARCHIVE_COMPRESSION_LZMA:
		case ARCHIVE_COMPRESSION_XZ:
			per_package = 400;
			break;
#ifdef ARCHIVE_COMPRESSION_UU
		case ARCHIVE_COMPRESSION_UU:
			per_package = 3015 * 4 / 3;
			break;
#endif
		default:
			/* assume it is at least somewhat compressed */
			per_package = 500;
	}

	return (size_t)((st->st_size / per_package) + 1);
}

static int sync_db_populate(alpm_db_t *db)
{
	const char *dbpath;
	size_t est_count, count;
	int fd;
	int ret = 0;
	int archive_ret;
	struct stat buf;
	struct archive *archive;
	struct archive_entry *entry;
	alpm_pkg_t *pkg = NULL;

	if(db->status & DB_STATUS_INVALID) {
		RET_ERR(db->handle, ALPM_ERR_DB_INVALID, -1);
	}
	if(db->status & DB_STATUS_MISSING) {
		RET_ERR(db->handle, ALPM_ERR_DB_NOT_FOUND, -1);
	}
	dbpath = _alpm_db_path(db);
	if(!dbpath) {
		/* pm_errno set in _alpm_db_path() */
		return -1;
	}

	fd = _alpm_open_archive(db->handle, dbpath, &buf,
			&archive, ALPM_ERR_DB_OPEN);
	if(fd < 0) {
		db->status &= ~DB_STATUS_VALID;
		db->status |= DB_STATUS_INVALID;
		return -1;
	}
	est_count = estimate_package_count(&buf, archive);

	/* currently only .files dbs contain file lists - make flexible when required*/
	if(strcmp(db->handle->dbext, ".files") == 0) {
		/* files databases are about four times larger on average */
		est_count /= 4;
	}

	db->pkgcache = _alpm_pkghash_create(est_count);
	if(db->pkgcache == NULL) {
		db->handle->pm_errno = ALPM_ERR_MEMORY;
		ret = -1;
		goto cleanup;
	}

	while((archive_ret = archive_read_next_header(archive, &entry)) == ARCHIVE_OK) {
		mode_t mode = archive_entry_mode(entry);
		if(!S_ISDIR(mode)) {
			/* we have desc or depends - parse it */
			if(sync_db_read(db, archive, entry, &pkg) != 0) {
				_alpm_log(db->handle, ALPM_LOG_ERROR,
						_("could not parse package description file '%s' from db '%s'\n"),
						archive_entry_pathname(entry), db->treename);
				ret = -1;
			}
		}
	}
	if(archive_ret != ARCHIVE_EOF) {
		_alpm_log(db->handle, ALPM_LOG_ERROR, _("could not read db '%s' (%s)\n"),
				db->treename, archive_error_string(archive));
		_alpm_db_free_pkgcache(db);
		db->handle->pm_errno = ALPM_ERR_LIBARCHIVE;
		ret = -1;
		goto cleanup;
	}

	count = alpm_list_count(db->pkgcache->list);
	if(count > 0) {
		db->pkgcache->list = alpm_list_msort(db->pkgcache->list,
				count, _alpm_pkg_cmp);
	}
	_alpm_log(db->handle, ALPM_LOG_DEBUG,
			"added %zu packages to package cache for db '%s'\n",
			count, db->treename);

cleanup:
	_alpm_archive_read_free(archive);
	if(fd >= 0) {
		close(fd);
	}
	return ret;
}

/* This function validates %FILENAME%. filename must be between 3 and
 * PATH_MAX characters and cannot be contain a path */
static int _alpm_validate_filename(alpm_db_t *db, const char *pkgname,
		const char *filename)
{
	size_t len = strlen(filename);

	if(filename[0] == '.') {
		errno = EINVAL;
		_alpm_log(db->handle, ALPM_LOG_ERROR, _("%s database is inconsistent: filename "
					"of package %s is illegal\n"), db->treename, pkgname);
		return -1;
	} else if(memchr(filename, '/', len) != NULL) {
		errno = EINVAL;
		_alpm_log(db->handle, ALPM_LOG_ERROR, _("%s database is inconsistent: filename "
					"of package %s is illegal\n"), db->treename, pkgname);
		return -1;
	} else if(len > PATH_MAX) {
		errno = EINVAL;
		_alpm_log(db->handle, ALPM_LOG_ERROR, _("%s database is inconsistent: filename "
					"of package %s is too long\n"), db->treename, pkgname);
		return -1;
	}

	return 0;
}

#define READ_NEXT() do { \
	if(_alpm_archive_fgets(archive, &buf) != ARCHIVE_OK) goto error; \
	line = buf.line; \
	_alpm_strip_newline(line, buf.real_line_size); \
} while(0)

#define READ_AND_STORE(f) do { \
	READ_NEXT(); \
	STRDUP(f, line, goto error); \
} while(0)

#define READ_AND_STORE_ALL(f) do { \
	char *linedup; \
	if(_alpm_archive_fgets(archive, &buf) != ARCHIVE_OK) goto error; \
	if(_alpm_strip_newline(buf.line, buf.real_line_size) == 0) break; \
	STRDUP(linedup, buf.line, goto error); \
	f = alpm_list_add(f, linedup); \
} while(1) /* note the while(1) and not (0) */

#define READ_AND_SPLITDEP(f) do { \
	if(_alpm_archive_fgets(archive, &buf) != ARCHIVE_OK) goto error; \
	if(_alpm_strip_newline(buf.line, buf.real_line_size) == 0) break; \
	f = alpm_list_add(f, alpm_dep_from_string(line)); \
} while(1) /* note the while(1) and not (0) */

static int sync_db_read(alpm_db_t *db, struct archive *archive,
		struct archive_entry *entry, alpm_pkg_t **likely_pkg)
{
	const char *entryname, *filename;
	alpm_pkg_t *pkg;
	struct archive_read_buffer buf;

	entryname = archive_entry_pathname(entry);
	if(entryname == NULL) {
		_alpm_log(db->handle, ALPM_LOG_DEBUG,
				"invalid archive entry provided to _alpm_sync_db_read, skipping\n");
		return -1;
	}

	_alpm_log(db->handle, ALPM_LOG_FUNCTION, "loading package data from archive entry %s\n",
			entryname);

	memset(&buf, 0, sizeof(buf));
	/* 512K for a line length seems reasonable */
	buf.max_line_size = 512 * 1024;

	pkg = load_pkg_for_entry(db, entryname, &filename, *likely_pkg);

	if(pkg == NULL) {
		_alpm_log(db->handle, ALPM_LOG_DEBUG,
				"entry %s could not be loaded into %s sync database\n",
				entryname, db->treename);
		return -1;
	}

	if(filename == NULL) {
		/* A file exists outside of a subdirectory. This isn't a read error, so return
		 * success and try to continue on. */
		_alpm_log(db->handle, ALPM_LOG_WARNING, _("unknown database file: %s\n"),
				entryname);
		return 0;
	}

	if(strcmp(filename, "desc") == 0 || strcmp(filename, "depends") == 0
			|| strcmp(filename, "files") == 0) {
		int ret;
		while((ret = _alpm_archive_fgets(archive, &buf)) == ARCHIVE_OK) {
			char *line = buf.line;
			if(_alpm_strip_newline(line, buf.real_line_size) == 0) {
				/* length of stripped line was zero */
				continue;
			}

			if(strcmp(line, "%NAME%") == 0) {
				READ_NEXT();
				if(strcmp(line, pkg->name) != 0) {
					_alpm_log(db->handle, ALPM_LOG_ERROR, _("%s database is inconsistent: name "
								"mismatch on package %s\n"), db->treename, pkg->name);
				}
			} else if(strcmp(line, "%VERSION%") == 0) {
				READ_NEXT();
				if(strcmp(line, pkg->version) != 0) {
					_alpm_log(db->handle, ALPM_LOG_ERROR, _("%s database is inconsistent: version "
								"mismatch on package %s\n"), db->treename, pkg->name);
				}
			} else if(strcmp(line, "%FILENAME%") == 0) {
				READ_AND_STORE(pkg->filename);
				if(_alpm_validate_filename(db, pkg->name, pkg->filename) < 0) {
					return -1;
				}
			} else if(strcmp(line, "%BASE%") == 0) {
				READ_AND_STORE(pkg->base);
			} else if(strcmp(line, "%DESC%") == 0) {
				READ_AND_STORE(pkg->desc);
			} else if(strcmp(line, "%GROUPS%") == 0) {
				READ_AND_STORE_ALL(pkg->groups);
			} else if(strcmp(line, "%URL%") == 0) {
				READ_AND_STORE(pkg->url);
			} else if(strcmp(line, "%LICENSE%") == 0) {
				READ_AND_STORE_ALL(pkg->licenses);
			} else if(strcmp(line, "%ARCH%") == 0) {
				READ_AND_STORE(pkg->arch);
			} else if(strcmp(line, "%BUILDDATE%") == 0) {
				READ_NEXT();
				pkg->builddate = _alpm_parsedate(line);
			} else if(strcmp(line, "%PACKAGER%") == 0) {
				READ_AND_STORE(pkg->packager);
			} else if(strcmp(line, "%CSIZE%") == 0) {
				READ_NEXT();
				pkg->size = _alpm_strtoofft(line);
			} else if(strcmp(line, "%ISIZE%") == 0) {
				READ_NEXT();
				pkg->isize = _alpm_strtoofft(line);
			} else if(strcmp(line, "%MD5SUM%") == 0) {
				READ_AND_STORE(pkg->md5sum);
			} else if(strcmp(line, "%SHA256SUM%") == 0) {
				READ_AND_STORE(pkg->sha256sum);
			} else if(strcmp(line, "%PGPSIG%") == 0) {
				READ_AND_STORE(pkg->base64_sig);
			} else if(strcmp(line, "%REPLACES%") == 0) {
				READ_AND_SPLITDEP(pkg->replaces);
			} else if(strcmp(line, "%DEPENDS%") == 0) {
				READ_AND_SPLITDEP(pkg->depends);
			} else if(strcmp(line, "%OPTDEPENDS%") == 0) {
				READ_AND_SPLITDEP(pkg->optdepends);
			} else if(strcmp(line, "%MAKEDEPENDS%") == 0) {
				READ_AND_SPLITDEP(pkg->makedepends);
			} else if(strcmp(line, "%CHECKDEPENDS%") == 0) {
				READ_AND_SPLITDEP(pkg->checkdepends);
			} else if(strcmp(line, "%CONFLICTS%") == 0) {
				READ_AND_SPLITDEP(pkg->conflicts);
			} else if(strcmp(line, "%PROVIDES%") == 0) {
				READ_AND_SPLITDEP(pkg->provides);
			} else if(strcmp(line, "%FILES%") == 0) {
				/* TODO: this could lazy load if there is future demand */
				size_t files_count = 0, files_size = 0;
				alpm_file_t *files = NULL;

				while(1) {
					if(_alpm_archive_fgets(archive, &buf) != ARCHIVE_OK) {
						goto error;
					}
					line = buf.line;
					if(_alpm_strip_newline(line, buf.real_line_size) == 0) {
						break;
					}

					if(!_alpm_greedy_grow((void **)&files, &files_size,
								(files_count ? (files_count + 1) * sizeof(alpm_file_t) : 8 * sizeof(alpm_file_t)))) {
						goto error;
					}
					STRDUP(files[files_count].name, line, goto error);
					files_count++;
				}
				/* attempt to hand back any memory we don't need */
				if(files_count > 0) {
					files = realloc(files, sizeof(alpm_file_t) * files_count);
				} else {
					FREE(files);
				}
				pkg->files.count = files_count;
				pkg->files.files = files;
				_alpm_filelist_sort(&pkg->files);
			}
		}
		if(ret != ARCHIVE_EOF) {
			goto error;
		}
		*likely_pkg = pkg;
	} else {
		/* unknown database file */
		_alpm_log(db->handle, ALPM_LOG_DEBUG, "unknown database file: %s\n", filename);
	}

	return 0;

error:
	_alpm_log(db->handle, ALPM_LOG_DEBUG, "error parsing database file: %s\n", filename);
	return -1;
}

struct db_operations sync_db_ops = {
	.validate         = sync_db_validate,
	.populate         = sync_db_populate,
	.unregister       = _alpm_db_unregister,
};

alpm_db_t *_alpm_db_register_sync(alpm_handle_t *handle, const char *treename,
		int level)
{
	alpm_db_t *db;

	_alpm_log(handle, ALPM_LOG_DEBUG, "registering sync database '%s'\n", treename);

#ifndef HAVE_LIBGPGME
	if(level != 0 && level != ALPM_SIG_USE_DEFAULT) {
		RET_ERR(handle, ALPM_ERR_MISSING_CAPABILITY_SIGNATURES, NULL);
	}
#endif

	db = _alpm_db_new(treename, 0);
	if(db == NULL) {
		RET_ERR(handle, ALPM_ERR_DB_CREATE, NULL);
	}
	db->ops = &sync_db_ops;
	db->handle = handle;
	db->siglevel = level;

	sync_db_validate(db);

	handle->dbs_sync = alpm_list_add(handle->dbs_sync, db);
	return db;
}
