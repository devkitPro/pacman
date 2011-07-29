/*
 *  be_package.c : backend for packages
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* libarchive */
#include <archive.h>
#include <archive_entry.h>

/* libalpm */
#include "alpm_list.h"
#include "alpm.h"
#include "util.h"
#include "log.h"
#include "handle.h"
#include "package.h"
#include "deps.h" /* _alpm_splitdep */

/**
 * Open a package changelog for reading. Similar to fopen in functionality,
 * except that the returned 'file stream' is from an archive.
 * @param pkg the package (file) to read the changelog
 * @return a 'file stream' to the package changelog
 */
static void *_package_changelog_open(alpm_pkg_t *pkg)
{
	ASSERT(pkg != NULL, return NULL);

	struct archive *archive = NULL;
	struct archive_entry *entry;
	const char *pkgfile = pkg->origin_data.file;

	if((archive = archive_read_new()) == NULL) {
		RET_ERR(pkg->handle, ALPM_ERR_LIBARCHIVE, NULL);
	}

	archive_read_support_compression_all(archive);
	archive_read_support_format_all(archive);

	if(archive_read_open_filename(archive, pkgfile,
				ARCHIVE_DEFAULT_BYTES_PER_BLOCK) != ARCHIVE_OK) {
		RET_ERR(pkg->handle, ALPM_ERR_PKG_OPEN, NULL);
	}

	while(archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
		const char *entry_name = archive_entry_pathname(entry);

		if(strcmp(entry_name, ".CHANGELOG") == 0) {
			return archive;
		}
	}
	/* we didn't find a changelog */
	archive_read_finish(archive);
	errno = ENOENT;

	return NULL;
}

/**
 * Read data from an open changelog 'file stream'. Similar to fread in
 * functionality, this function takes a buffer and amount of data to read.
 * @param ptr a buffer to fill with raw changelog data
 * @param size the size of the buffer
 * @param pkg the package that the changelog is being read from
 * @param fp a 'file stream' to the package changelog
 * @return the number of characters read, or 0 if there is no more data
 */
static size_t _package_changelog_read(void *ptr, size_t size,
		const alpm_pkg_t UNUSED *pkg, const void *fp)
{
	ssize_t sret = archive_read_data((struct archive *)fp, ptr, size);
	/* Report error (negative values) */
	if(sret < 0) {
		RET_ERR(pkg->handle, ALPM_ERR_LIBARCHIVE, 0);
	} else {
		return (size_t)sret;
	}
}

/**
 * Close a package changelog for reading. Similar to fclose in functionality,
 * except that the 'file stream' is from an archive.
 * @param pkg the package (file) that the changelog was read from
 * @param fp a 'file stream' to the package changelog
 * @return whether closing the package changelog stream was successful
 */
static int _package_changelog_close(const alpm_pkg_t UNUSED *pkg, void *fp)
{
	return archive_read_finish((struct archive *)fp);
}

/** Package file operations struct accessor. We implement this as a method
 * rather than a static struct as in be_files because we want to reuse the
 * majority of the default_pkg_ops struct and add only a few operations of
 * our own on top.
 */
static struct pkg_operations *get_file_pkg_ops(void)
{
	static struct pkg_operations file_pkg_ops;
	static int file_pkg_ops_initialized = 0;
	if(!file_pkg_ops_initialized) {
		file_pkg_ops = default_pkg_ops;
		file_pkg_ops.changelog_open  = _package_changelog_open;
		file_pkg_ops.changelog_read  = _package_changelog_read;
		file_pkg_ops.changelog_close = _package_changelog_close;
		file_pkg_ops_initialized = 1;
	}
	return &file_pkg_ops;
}

/**
 * Parses the package description file for a package into a alpm_pkg_t struct.
 * @param archive the archive to read from, pointed at the .PKGINFO entry
 * @param newpkg an empty alpm_pkg_t struct to fill with package info
 *
 * @return 0 on success, -1 on error
 */
static int parse_descfile(alpm_handle_t *handle, struct archive *a, alpm_pkg_t *newpkg)
{
	char *ptr = NULL;
	char *key = NULL;
	int ret, linenum = 0;
	struct archive_read_buffer buf;

	memset(&buf, 0, sizeof(buf));
	/* 512K for a line length seems reasonable */
	buf.max_line_size = 512 * 1024;

	/* loop until we reach EOF or other error */
	while((ret = _alpm_archive_fgets(a, &buf)) == ARCHIVE_OK) {
		size_t len = _alpm_strip_newline(buf.line);

		linenum++;
		if(len == 0 || buf.line[0] == '#') {
			continue;
		}
		ptr = buf.line;
		key = strsep(&ptr, "=");
		if(key == NULL || ptr == NULL) {
			_alpm_log(handle, ALPM_LOG_DEBUG, "%s: syntax error in description file line %d\n",
								newpkg->name ? newpkg->name : "error", linenum);
		} else {
			key = _alpm_strtrim(key);
			while(*ptr == ' ') ptr++;
			ptr = _alpm_strtrim(ptr);
			if(strcmp(key, "pkgname") == 0) {
				STRDUP(newpkg->name, ptr, return -1);
				newpkg->name_hash = _alpm_hash_sdbm(newpkg->name);
			} else if(strcmp(key, "pkgbase") == 0) {
				/* not used atm */
			} else if(strcmp(key, "pkgver") == 0) {
				STRDUP(newpkg->version, ptr, return -1);
			} else if(strcmp(key, "pkgdesc") == 0) {
				STRDUP(newpkg->desc, ptr, return -1);
			} else if(strcmp(key, "group") == 0) {
				newpkg->groups = alpm_list_add(newpkg->groups, strdup(ptr));
			} else if(strcmp(key, "url") == 0) {
				STRDUP(newpkg->url, ptr, return -1);
			} else if(strcmp(key, "license") == 0) {
				newpkg->licenses = alpm_list_add(newpkg->licenses, strdup(ptr));
			} else if(strcmp(key, "builddate") == 0) {
				newpkg->builddate = _alpm_parsedate(ptr);
			} else if(strcmp(key, "packager") == 0) {
				STRDUP(newpkg->packager, ptr, return -1);
			} else if(strcmp(key, "arch") == 0) {
				STRDUP(newpkg->arch, ptr, return -1);
			} else if(strcmp(key, "size") == 0) {
				/* size in the raw package is uncompressed (installed) size */
				newpkg->isize = atol(ptr);
			} else if(strcmp(key, "depend") == 0) {
				alpm_depend_t *dep = _alpm_splitdep(ptr);
				newpkg->depends = alpm_list_add(newpkg->depends, dep);
			} else if(strcmp(key, "optdepend") == 0) {
				newpkg->optdepends = alpm_list_add(newpkg->optdepends, strdup(ptr));
			} else if(strcmp(key, "conflict") == 0) {
				newpkg->conflicts = alpm_list_add(newpkg->conflicts, strdup(ptr));
			} else if(strcmp(key, "replaces") == 0) {
				newpkg->replaces = alpm_list_add(newpkg->replaces, strdup(ptr));
			} else if(strcmp(key, "provides") == 0) {
				newpkg->provides = alpm_list_add(newpkg->provides, strdup(ptr));
			} else if(strcmp(key, "backup") == 0) {
				alpm_backup_t *backup;
				CALLOC(backup, 1, sizeof(alpm_backup_t), return -1);
				STRDUP(backup->name, ptr, return -1);
				newpkg->backup = alpm_list_add(newpkg->backup, backup);
			} else if(strcmp(key, "force") == 0) {
				/* deprecated, skip it */
			} else if(strcmp(key, "makepkgopt") == 0) {
				/* not used atm */
			} else {
				_alpm_log(handle, ALPM_LOG_DEBUG, "%s: unknown key '%s' in description file line %d\n",
									newpkg->name ? newpkg->name : "error", key, linenum);
			}
		}
	}
	if(ret != ARCHIVE_EOF) {
		_alpm_log(handle, ALPM_LOG_DEBUG, "error parsing package descfile\n");
		return -1;
	}

	return 0;
}

static void files_merge(alpm_file_t a[], alpm_file_t b[], alpm_file_t c[],
		size_t m, size_t n)
{
	size_t i = 0, j = 0, k = 0;
	while(i < m && j < n) {
		if(strcmp(a[i].name, b[j].name) < 0) {
			c[k++] = a[i++];
		} else {
			c[k++] = b[j++];
		}
	}
	while(i < m) {
		c[k++] = a[i++];
	}
	while(j < n) {
		c[k++] = b[j++];
	}
}

static alpm_file_t *files_msort(alpm_file_t *files, size_t n)
{
	alpm_file_t *work;
	size_t blocksize = 1;

	CALLOC(work, n, sizeof(alpm_file_t), return NULL);

	for(blocksize = 1; blocksize < n; blocksize *= 2) {
		size_t i, max_extent = 0;
		for(i = 0; i < n - blocksize; i += 2 * blocksize) {
			/* this limits our actual merge to the length of the array, since we will
			 * not likely be a perfect power of two. */
			size_t right_blocksize = blocksize;
			if(i + blocksize * 2 > n) {
				right_blocksize = n - i - blocksize;
			}
			files_merge(files + i, files + i + blocksize, work + i,
					blocksize, right_blocksize);
			max_extent = i + blocksize + right_blocksize;
		}
		/* ensure we only copy what we actually touched on this merge pass,
		 * no more, no less */
		memcpy(files, work, max_extent * sizeof(alpm_file_t));
	}
	free(work);
	return files;
}

/**
 * Load a package and create the corresponding alpm_pkg_t struct.
 * @param handle the context handle
 * @param pkgfile path to the package file
 * @param full whether to stop the load after metadata is read or continue
 *             through the full archive
 * @return An information filled alpm_pkg_t struct
 */
alpm_pkg_t *_alpm_pkg_load_internal(alpm_handle_t *handle, const char *pkgfile,
		int full, const char *md5sum, const char *base64_sig,
		alpm_siglevel_t level)
{
	int ret;
	int config = 0;
	struct archive *archive;
	struct archive_entry *entry;
	alpm_pkg_t *newpkg = NULL;
	struct stat st;
	size_t files_count = 0, files_size = 0;
	alpm_file_t *files = NULL;

	if(pkgfile == NULL || strlen(pkgfile) == 0) {
		RET_ERR(handle, ALPM_ERR_WRONG_ARGS, NULL);
	}

	/* attempt to stat the package file, ensure it exists */
	if(stat(pkgfile, &st) == 0) {
		newpkg = _alpm_pkg_new();
		if(newpkg == NULL) {
			RET_ERR(handle, ALPM_ERR_MEMORY, NULL);
		}
		newpkg->filename = strdup(pkgfile);
		newpkg->size = st.st_size;
	} else {
		/* couldn't stat the pkgfile, return an error */
		RET_ERR(handle, ALPM_ERR_PKG_OPEN, NULL);
	}

	/* first steps- validate the package file */
	_alpm_log(handle, ALPM_LOG_DEBUG, "md5sum: %s\n", md5sum);
	if(md5sum) {
		_alpm_log(handle, ALPM_LOG_DEBUG, "checking md5sum for %s\n", pkgfile);
		if(_alpm_test_md5sum(pkgfile, md5sum) != 0) {
			alpm_pkg_free(newpkg);
			RET_ERR(handle, ALPM_ERR_PKG_INVALID, NULL);
		}
	}

	_alpm_log(handle, ALPM_LOG_DEBUG, "base64_sig: %s\n", base64_sig);
	if(level & ALPM_SIG_PACKAGE &&
			_alpm_check_pgp_helper(handle, pkgfile, base64_sig,
				level & ALPM_SIG_PACKAGE_OPTIONAL, level & ALPM_SIG_PACKAGE_MARGINAL_OK,
				level & ALPM_SIG_PACKAGE_UNKNOWN_OK, ALPM_ERR_PKG_INVALID_SIG)) {
		_alpm_pkg_free(newpkg);
		return NULL;
	}

	/* next- try to create an archive object to read in the package */
	if((archive = archive_read_new()) == NULL) {
		alpm_pkg_free(newpkg);
		RET_ERR(handle, ALPM_ERR_LIBARCHIVE, NULL);
	}

	archive_read_support_compression_all(archive);
	archive_read_support_format_all(archive);

	if(archive_read_open_filename(archive, pkgfile,
				ARCHIVE_DEFAULT_BYTES_PER_BLOCK) != ARCHIVE_OK) {
		alpm_pkg_free(newpkg);
		RET_ERR(handle, ALPM_ERR_PKG_OPEN, NULL);
	}

	_alpm_log(handle, ALPM_LOG_DEBUG, "starting package load for %s\n", pkgfile);

	/* If full is false, only read through the archive until we find our needed
	 * metadata. If it is true, read through the entire archive, which serves
	 * as a verfication of integrity and allows us to create the filelist. */
	while((ret = archive_read_next_header(archive, &entry)) == ARCHIVE_OK) {
		const char *entry_name = archive_entry_pathname(entry);

		if(strcmp(entry_name, ".PKGINFO") == 0) {
			/* parse the info file */
			if(parse_descfile(handle, archive, newpkg) != 0) {
				_alpm_log(handle, ALPM_LOG_ERROR, _("could not parse package description file in %s\n"),
						pkgfile);
				goto pkg_invalid;
			}
			if(newpkg->name == NULL || strlen(newpkg->name) == 0) {
				_alpm_log(handle, ALPM_LOG_ERROR, _("missing package name in %s\n"), pkgfile);
				goto pkg_invalid;
			}
			if(newpkg->version == NULL || strlen(newpkg->version) == 0) {
				_alpm_log(handle, ALPM_LOG_ERROR, _("missing package version in %s\n"), pkgfile);
				goto pkg_invalid;
			}
			config = 1;
			continue;
		} else if(strcmp(entry_name,  ".INSTALL") == 0) {
			newpkg->scriptlet = 1;
		} else if(*entry_name == '.') {
			/* for now, ignore all files starting with '.' that haven't
			 * already been handled (for future possibilities) */
		} else if(full) {
			/* Keep track of all files for filelist generation */
			if(files_count >= files_size) {
				size_t old_size = files_size;
				if(files_size == 0) {
					files_size = 4;
				} else {
					files_size *= 2;
				}
				files = realloc(files, sizeof(alpm_file_t) * files_size);
				if(!files) {
					ALLOC_FAIL(sizeof(alpm_file_t) * files_size);
					goto error;
				}
				/* ensure all new memory is zeroed out, in both the initial
				 * allocation and later reallocs */
				memset(files + old_size, 0,
						sizeof(alpm_file_t) * (files_size - old_size));
			}
			STRDUP(files[files_count].name, entry_name, goto error);
			files[files_count].size = archive_entry_size(entry);
			files[files_count].mode = archive_entry_mode(entry);
			files_count++;
		}

		if(archive_read_data_skip(archive)) {
			_alpm_log(handle, ALPM_LOG_ERROR, _("error while reading package %s: %s\n"),
					pkgfile, archive_error_string(archive));
			handle->pm_errno = ALPM_ERR_LIBARCHIVE;
			goto error;
		}

		/* if we are not doing a full read, see if we have all we need */
		if(!full && config) {
			break;
		}
	}

	if(ret != ARCHIVE_EOF && ret != ARCHIVE_OK) { /* An error occured */
		_alpm_log(handle, ALPM_LOG_ERROR, _("error while reading package %s: %s\n"),
				pkgfile, archive_error_string(archive));
		handle->pm_errno = ALPM_ERR_LIBARCHIVE;
		goto error;
	}

	if(!config) {
		_alpm_log(handle, ALPM_LOG_ERROR, _("missing package metadata in %s\n"), pkgfile);
		goto pkg_invalid;
	}

	archive_read_finish(archive);

	/* internal fields for package struct */
	newpkg->origin = PKG_FROM_FILE;
	newpkg->origin_data.file = strdup(pkgfile);
	newpkg->ops = get_file_pkg_ops();
	newpkg->handle = handle;

	if(full) {
		/* attempt to hand back any memory we don't need */
		files = realloc(files, sizeof(alpm_file_t) * files_count);
		/* "checking for conflicts" requires a sorted list, ensure that here */
		_alpm_log(handle, ALPM_LOG_DEBUG, "sorting package filelist for %s\n", pkgfile);
		newpkg->files.files = files_msort(files, files_count);
		newpkg->files.count = files_count;
		newpkg->infolevel = INFRQ_ALL;
	} else {
		newpkg->infolevel = INFRQ_BASE | INFRQ_DESC;
	}

	return newpkg;

pkg_invalid:
	handle->pm_errno = ALPM_ERR_PKG_INVALID;
error:
	_alpm_pkg_free(newpkg);
	archive_read_finish(archive);

	return NULL;
}

int SYMEXPORT alpm_pkg_load(alpm_handle_t *handle, const char *filename, int full,
		alpm_siglevel_t level, alpm_pkg_t **pkg)
{
	CHECK_HANDLE(handle, return -1);
	ASSERT(pkg != NULL, RET_ERR(handle, ALPM_ERR_WRONG_ARGS, -1));

	*pkg = _alpm_pkg_load_internal(handle, filename, full, NULL, NULL, level);
	if(*pkg == NULL) {
		/* pm_errno is set by pkg_load */
		return -1;
	}

	return 0;
}

/* vim: set ts=2 sw=2 noet: */
