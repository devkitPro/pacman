/*
 *  be_sync.c : backend for sync databases
 *
 *  Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
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

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

/* libarchive */
#include <archive.h>
#include <archive_entry.h>

/* libalpm */
#include "util.h"
#include "log.h"
#include "alpm.h"
#include "alpm_list.h"
#include "package.h"
#include "handle.h"
#include "delta.h"
#include "deps.h"
#include "dload.h"

static char *get_sync_dir(pmhandle_t *handle)
{
	const char *dbpath = alpm_option_get_dbpath(handle);
	size_t len = strlen(dbpath) + 6;
	char *syncpath;
	struct stat buf;

	MALLOC(syncpath, len, RET_ERR(handle, PM_ERR_MEMORY, NULL));
	sprintf(syncpath, "%s%s", dbpath, "sync/");

	if(stat(syncpath, &buf) != 0) {
		_alpm_log(handle, PM_LOG_DEBUG, "database dir '%s' does not exist, creating it\n",
				syncpath);
		if(_alpm_makepath(syncpath) != 0) {
			free(syncpath);
			RET_ERR(handle, PM_ERR_SYSTEM, NULL);
		}
	} else if(!S_ISDIR(buf.st_mode)) {
		_alpm_log(handle, PM_LOG_WARNING, _("removing invalid file: %s\n"), syncpath);
		if(unlink(syncpath) != 0 || _alpm_makepath(syncpath) != 0) {
			free(syncpath);
			RET_ERR(handle, PM_ERR_SYSTEM, NULL);
		}
	}

	return syncpath;
}

static int sync_db_validate(pmdb_t *db)
{
	pgp_verify_t check_sig;

	if(db->status & DB_STATUS_VALID) {
		return 0;
	}

	/* this takes into account the default verification level if UNKNOWN
	 * was assigned to this db */
	check_sig = _alpm_db_get_sigverify_level(db);

	if(check_sig != PM_PGP_VERIFY_NEVER) {
		int ret;
		const char *dbpath = _alpm_db_path(db);
		if(!dbpath) {
			/* pm_errno set in _alpm_db_path() */
			return -1;
		}

		/* we can skip any validation if the database doesn't exist */
		if(access(dbpath, R_OK) != 0 && errno == ENOENT) {
			goto valid;
			return 0;
		}

		_alpm_log(db->handle, PM_LOG_DEBUG, "checking signature for %s\n",
				db->treename);
		ret = _alpm_gpgme_checksig(db->handle, dbpath, NULL);
		if((check_sig == PM_PGP_VERIFY_ALWAYS && ret != 0) ||
				(check_sig == PM_PGP_VERIFY_OPTIONAL && ret == 1)) {
			RET_ERR(db->handle, PM_ERR_SIG_INVALID, -1);
		}
	}

valid:
	db->status |= DB_STATUS_VALID;
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
 * alpm_list_t *syncs = alpm_option_get_syncdbs();
 * for(i = syncs; i; i = alpm_list_next(i)) {
 *     pmdb_t *db = alpm_list_getdata(i);
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
int SYMEXPORT alpm_db_update(int force, pmdb_t *db)
{
	char *syncpath;
	alpm_list_t *i;
	int ret = -1;
	mode_t oldmask;
	pmhandle_t *handle;
	pgp_verify_t check_sig;

	/* Sanity checks */
	ASSERT(db != NULL, return -1);
	handle = db->handle;
	handle->pm_errno = 0;
	ASSERT(db != handle->db_local, RET_ERR(handle, PM_ERR_WRONG_ARGS, -1));
	ASSERT(db->servers != NULL, RET_ERR(handle, PM_ERR_SERVER_NONE, -1));

	syncpath = get_sync_dir(handle);
	if(!syncpath) {
		return -1;
	}

	/* make sure we have a sane umask */
	oldmask = umask(0022);

	check_sig = _alpm_db_get_sigverify_level(db);

	/* attempt to grab a lock */
	if(_alpm_handle_lock(handle)) {
		RET_ERR(handle, PM_ERR_HANDLE_LOCK, -1);
	}

	for(i = db->servers; i; i = i->next) {
		const char *server = i->data;
		char *fileurl;
		size_t len;
		int sig_ret = 0;

		/* print server + filename into a buffer (leave space for .sig) */
		len = strlen(server) + strlen(db->treename) + 9;
		CALLOC(fileurl, len, sizeof(char), RET_ERR(handle, PM_ERR_MEMORY, -1));
		snprintf(fileurl, len, "%s/%s.db", server, db->treename);

		ret = _alpm_download(handle, fileurl, syncpath, force, 0, 0);

		if(ret == 0 && (check_sig == PM_PGP_VERIFY_ALWAYS ||
					check_sig == PM_PGP_VERIFY_OPTIONAL)) {
			/* an existing sig file is no good at this point */
			char *sigpath = _alpm_db_sig_path(db);
			if(!sigpath) {
				ret = -1;
				break;
			}
			unlink(sigpath);
			free(sigpath);

			int errors_ok = (check_sig == PM_PGP_VERIFY_OPTIONAL);
			/* if we downloaded a DB, we want the .sig from the same server */
			snprintf(fileurl, len, "%s/%s.db.sig", server, db->treename);

			sig_ret = _alpm_download(handle, fileurl, syncpath, 1, 0, errors_ok);
			/* errors_ok suppresses error messages, but not the return code */
			sig_ret = errors_ok ? 0 : sig_ret;
		}

		FREE(fileurl);
		if(ret != -1 && sig_ret != -1) {
			break;
		}
	}

	if(ret == 1) {
		/* files match, do nothing */
		handle->pm_errno = 0;
		goto cleanup;
	} else if(ret == -1) {
		/* pm_errno was set by the download code */
		_alpm_log(handle, PM_LOG_DEBUG, "failed to sync db: %s\n",
				alpm_strerror(handle->pm_errno));
		goto cleanup;
	}

	/* Cache needs to be rebuilt */
	_alpm_db_free_pkgcache(db);

	db->status &= ~DB_STATUS_VALID;
	if(sync_db_validate(db)) {
		/* pm_errno should be set */
		ret = -1;
	}

cleanup:

	if(_alpm_handle_unlock(handle)) {
		_alpm_log(handle, PM_LOG_WARNING, _("could not remove lock file %s\n"),
				alpm_option_get_lockfile(handle));
	}
	free(syncpath);
	umask(oldmask);
	return ret;
}

/* Forward decl so I don't reorganize the whole file right now */
static int sync_db_read(pmdb_t *db, struct archive *archive,
		struct archive_entry *entry, pmpkg_t **likely_pkg);

static pmpkg_t *load_pkg_for_entry(pmdb_t *db, const char *entryname,
		const char **entry_filename, pmpkg_t *likely_pkg)
{
	char *pkgname = NULL, *pkgver = NULL;
	unsigned long pkgname_hash;
	pmpkg_t *pkg;

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
		_alpm_log(db->handle, PM_LOG_ERROR,
				_("invalid name for database entry '%s'\n"), entryname);
		return NULL;
	}

	if(likely_pkg && strcmp(likely_pkg->name, pkgname) == 0) {
		pkg = likely_pkg;
	} else {
		pkg = _alpm_pkghash_find(db->pkgcache, pkgname);
	}
	if(pkg == NULL) {
		pkg = _alpm_pkg_new();
		if(pkg == NULL) {
			RET_ERR(db->handle, PM_ERR_MEMORY, NULL);
		}

		pkg->name = pkgname;
		pkg->version = pkgver;
		pkg->name_hash = pkgname_hash;

		pkg->origin = PKG_FROM_SYNCDB;
		pkg->origin_data.db = db;
		pkg->ops = &default_pkg_ops;
		pkg->handle = db->handle;

		/* add to the collection */
		_alpm_log(db->handle, PM_LOG_FUNCTION, "adding '%s' to package cache for db '%s'\n",
				pkg->name, db->treename);
		db->pkgcache = _alpm_pkghash_add(db->pkgcache, pkg);
	} else {
		free(pkgname);
		free(pkgver);
	}

	return pkg;
}

/*
 * This is the data table used to generate the estimating function below.
 * "Weighted Avg" means averaging the bottom table values; thus each repo, big
 * or small, will have equal influence.  "Unweighted Avg" means averaging the
 * sums of the top table columns, thus each package has equal influence.  The
 * final values are calculated by (surprise) averaging the averages, because
 * why the hell not.
 *
 * Database   Pkgs  tar      bz2     gz      xz
 * community  2096  5294080  256391  421227  301296
 * core        180   460800   25257   36850   29356
 * extra      2606  6635520  294647  470818  339392
 * multilib    126   327680   16120   23261   18732
 * testing      76   204800   10902   14348   12100
 *
 * Bytes Per Package
 * community  2096  2525.80  122.32  200.97  143.75
 * core        180  2560.00  140.32  204.72  163.09
 * extra      2606  2546.25  113.06  180.67  130.23
 * multilib    126  2600.63  127.94  184.61  148.67
 * testing      76  2694.74  143.45  188.79  159.21

 * Weighted Avg     2585.48  129.42  191.95  148.99
 * Unweighted Avg   2543.39  118.74  190.16  137.93
 * Average of Avgs  2564.44  124.08  191.06  143.46
 */
static size_t estimate_package_count(struct stat *st, struct archive *archive)
{
	unsigned int per_package;

	switch(archive_compression(archive)) {
		case ARCHIVE_COMPRESSION_NONE:
			per_package = 2564;
			break;
		case ARCHIVE_COMPRESSION_GZIP:
			per_package = 191;
			break;
		case ARCHIVE_COMPRESSION_BZIP2:
			per_package = 124;
			break;
		case ARCHIVE_COMPRESSION_COMPRESS:
			per_package = 193;
			break;
		case ARCHIVE_COMPRESSION_LZMA:
		case ARCHIVE_COMPRESSION_XZ:
			per_package = 143;
			break;
#ifdef ARCHIVE_COMPRESSION_UU
		case ARCHIVE_COMPRESSION_UU:
			per_package = 3543;
			break;
#endif
		default:
			/* assume it is at least somewhat compressed */
			per_package = 200;
	}
	return (size_t)((st->st_size / per_package) + 1);
}

static int sync_db_populate(pmdb_t *db)
{
	const char *dbpath;
	size_t est_count;
	int count = 0;
	struct stat buf;
	struct archive *archive;
	struct archive_entry *entry;
	pmpkg_t *pkg = NULL;

	if((archive = archive_read_new()) == NULL) {
		RET_ERR(db->handle, PM_ERR_LIBARCHIVE, -1);
	}

	archive_read_support_compression_all(archive);
	archive_read_support_format_all(archive);

	dbpath = _alpm_db_path(db);
	if(!dbpath) {
		/* pm_errno set in _alpm_db_path() */
		return -1;
	}

	_alpm_log(db->handle, PM_LOG_DEBUG, "opening database archive %s\n", dbpath);

	if(archive_read_open_filename(archive, dbpath,
				ARCHIVE_DEFAULT_BYTES_PER_BLOCK) != ARCHIVE_OK) {
		_alpm_log(db->handle, PM_LOG_ERROR, _("could not open file %s: %s\n"), dbpath,
				archive_error_string(archive));
		archive_read_finish(archive);
		RET_ERR(db->handle, PM_ERR_DB_OPEN, -1);
	}
	if(stat(dbpath, &buf) != 0) {
		RET_ERR(db->handle, PM_ERR_DB_OPEN, -1);
	}
	est_count = estimate_package_count(&buf, archive);

	/* initialize hash at 66% full */
	db->pkgcache = _alpm_pkghash_create(est_count * 3 / 2);
	if(db->pkgcache == NULL) {
		RET_ERR(db->handle, PM_ERR_MEMORY, -1);
	}

	while(archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
		const struct stat *st;

		st = archive_entry_stat(entry);

		if(S_ISDIR(st->st_mode)) {
			continue;
		} else {
			/* we have desc, depends or deltas - parse it */
			if(sync_db_read(db, archive, entry, &pkg) != 0) {
				_alpm_log(db->handle, PM_LOG_ERROR,
						_("could not parse package description file '%s' from db '%s'\n"),
						archive_entry_pathname(entry), db->treename);
				continue;
			}
		}
	}

	count = alpm_list_count(db->pkgcache->list);

	if(count > 0) {
		db->pkgcache->list = alpm_list_msort(db->pkgcache->list, (size_t)count, _alpm_pkg_cmp);
	}
	archive_read_finish(archive);
	_alpm_log(db->handle, PM_LOG_DEBUG, "added %d packages to package cache for db '%s'\n",
			count, db->treename);

	return count;
}

#define READ_NEXT(s) do { \
	if(_alpm_archive_fgets(archive, &buf) != ARCHIVE_OK) goto error; \
	s = _alpm_strtrim(buf.line); \
} while(0)

#define READ_AND_STORE(f) do { \
	READ_NEXT(line); \
	STRDUP(f, line, goto error); \
} while(0)

#define READ_AND_STORE_ALL(f) do { \
	char *linedup; \
	READ_NEXT(line); \
	if(strlen(line) == 0) break; \
	STRDUP(linedup, line, goto error); \
	f = alpm_list_add(f, linedup); \
} while(1) /* note the while(1) and not (0) */

static int sync_db_read(pmdb_t *db, struct archive *archive,
		struct archive_entry *entry, pmpkg_t **likely_pkg)
{
	const char *entryname, *filename;
	pmpkg_t *pkg;
	struct archive_read_buffer buf;

	entryname = archive_entry_pathname(entry);
	if(entryname == NULL) {
		_alpm_log(db->handle, PM_LOG_DEBUG,
				"invalid archive entry provided to _alpm_sync_db_read, skipping\n");
		return -1;
	}

	_alpm_log(db->handle, PM_LOG_FUNCTION, "loading package data from archive entry %s\n",
			entryname);

	memset(&buf, 0, sizeof(buf));
	/* 512K for a line length seems reasonable */
	buf.max_line_size = 512 * 1024;

	pkg = load_pkg_for_entry(db, entryname, &filename, *likely_pkg);

	if(pkg == NULL) {
		_alpm_log(db->handle, PM_LOG_DEBUG,
				"entry %s could not be loaded into %s sync database",
				entryname, db->treename);
		return -1;
	}

	if(strcmp(filename, "desc") == 0 || strcmp(filename, "depends") == 0
			|| strcmp(filename, "deltas") == 0) {
		int ret;
		while((ret = _alpm_archive_fgets(archive, &buf)) == ARCHIVE_OK) {
			char *line = _alpm_strtrim(buf.line);

			if(strcmp(line, "%NAME%") == 0) {
				READ_NEXT(line);
				if(strcmp(line, pkg->name) != 0) {
					_alpm_log(db->handle, PM_LOG_ERROR, _("%s database is inconsistent: name "
								"mismatch on package %s\n"), db->treename, pkg->name);
				}
			} else if(strcmp(line, "%VERSION%") == 0) {
				READ_NEXT(line);
				if(strcmp(line, pkg->version) != 0) {
					_alpm_log(db->handle, PM_LOG_ERROR, _("%s database is inconsistent: version "
								"mismatch on package %s\n"), db->treename, pkg->name);
				}
			} else if(strcmp(line, "%FILENAME%") == 0) {
				READ_AND_STORE(pkg->filename);
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
				READ_NEXT(line);
				pkg->builddate = _alpm_parsedate(line);
			} else if(strcmp(line, "%PACKAGER%") == 0) {
				READ_AND_STORE(pkg->packager);
			} else if(strcmp(line, "%CSIZE%") == 0) {
				/* Note: the CSIZE and SIZE fields both share the "size" field in the
				 * pkginfo_t struct. This can be done b/c CSIZE is currently only used
				 * in sync databases, and SIZE is only used in local databases.
				 */
				READ_NEXT(line);
				pkg->size = atol(line);
				/* also store this value to isize if isize is unset */
				if(pkg->isize == 0) {
					pkg->isize = pkg->size;
				}
			} else if(strcmp(line, "%ISIZE%") == 0) {
				READ_NEXT(line);
				pkg->isize = atol(line);
			} else if(strcmp(line, "%MD5SUM%") == 0) {
				READ_AND_STORE(pkg->md5sum);
			} else if(strcmp(line, "%SHA256SUM%") == 0) {
				/* we don't do anything with this value right now */
				READ_NEXT(line);
			} else if(strcmp(line, "%PGPSIG%") == 0) {
				READ_AND_STORE(pkg->base64_sig);
			} else if(strcmp(line, "%REPLACES%") == 0) {
				READ_AND_STORE_ALL(pkg->replaces);
			} else if(strcmp(line, "%DEPENDS%") == 0) {
				/* Different than the rest because of the _alpm_splitdep call. */
				while(1) {
					READ_NEXT(line);
					if(strlen(line) == 0) break;
					pkg->depends = alpm_list_add(pkg->depends, _alpm_splitdep(line));
				}
			} else if(strcmp(line, "%OPTDEPENDS%") == 0) {
				READ_AND_STORE_ALL(pkg->optdepends);
			} else if(strcmp(line, "%CONFLICTS%") == 0) {
				READ_AND_STORE_ALL(pkg->conflicts);
			} else if(strcmp(line, "%PROVIDES%") == 0) {
				READ_AND_STORE_ALL(pkg->provides);
			} else if(strcmp(line, "%DELTAS%") == 0) {
				/* Different than the rest because of the _alpm_delta_parse call. */
				while(1) {
					READ_NEXT(line);
					if(strlen(line) == 0) break;
					pkg->deltas = alpm_list_add(pkg->deltas, _alpm_delta_parse(line));
				}
			}
		}
		if(ret != ARCHIVE_EOF) {
			goto error;
		}
		*likely_pkg = pkg;
	} else if(strcmp(filename, "files") == 0) {
		/* currently do nothing with this file */
	} else {
		/* unknown database file */
		_alpm_log(db->handle, PM_LOG_DEBUG, "unknown database file: %s\n", filename);
	}

	return 0;

error:
	_alpm_log(db->handle, PM_LOG_DEBUG, "error parsing database file: %s\n", filename);
	return -1;
}

struct db_operations sync_db_ops = {
	.populate         = sync_db_populate,
	.unregister       = _alpm_db_unregister,
};

pmdb_t *_alpm_db_register_sync(pmhandle_t *handle, const char *treename,
		pgp_verify_t level)
{
	pmdb_t *db;

	_alpm_log(handle, PM_LOG_DEBUG, "registering sync database '%s'\n", treename);

	db = _alpm_db_new(treename, 0);
	if(db == NULL) {
		RET_ERR(handle, PM_ERR_DB_CREATE, NULL);
	}
	db->ops = &sync_db_ops;
	db->handle = handle;
	db->pgp_verify = level;

	if(sync_db_validate(db)) {
		_alpm_db_free(db);
		return NULL;
	}

	handle->dbs_sync = alpm_list_add(handle->dbs_sync, db);
	return db;
}


/* vim: set ts=2 sw=2 noet: */
