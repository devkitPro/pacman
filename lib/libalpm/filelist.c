/*
 *  filelist.c
 *
 *  Copyright (c) 2012 Pacman Development Team <pacman-dev@archlinux.org>
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

#include <limits.h>
#include <string.h>
#include <sys/stat.h>

/* libalpm */
#include "filelist.h"
#include "util.h"

/** Helper function for comparing strings when sorting */
static int _alpm_filelist_strcmp(const void *s1, const void *s2)
{
	return strcmp(*(char **)s1, *(char **)s2);
}

/**
 * @brief Resolves a symlink and its children.
 *
 * @attention Pre-condition: files must be sorted!
 *
 * @param files filelist to resolve
 * @param i index in files to start processing
 * @param path absolute path for the symlink being resolved
 * @param root_len length of the root portion of path
 * @param resolving is file \i in \files a symlink that needs to be resolved
 *
 * @return the index of the last file resolved
 */
size_t _alpm_filelist_resolve_link(
		alpm_filelist_t *files, size_t i, char *path, size_t root_len, int resolving)
{
	char *causal_dir = NULL; /* symlink being resolved */
	char *filename_r = NULL; /* resolved filename */
	size_t causal_dir_len = 0, causal_dir_r_len = 0;

	if(resolving) {
		/* deal with the symlink being resolved */
		MALLOC(filename_r, PATH_MAX, goto error);
		causal_dir = files->files[i].name;
		causal_dir_len = strlen(causal_dir);
		if(realpath(path, filename_r) == NULL) {
			STRDUP(files->resolved_path[i], causal_dir, goto error);
			FREE(filename_r);
			return i;
		}
		causal_dir_r_len = strlen(filename_r + root_len) + 1;
		if(causal_dir_r_len >= PATH_MAX) {
			STRDUP(files->resolved_path[i], causal_dir, goto error);
			FREE(filename_r);
			return i;
		}
		/* remove root_r from filename_r */
		memmove(filename_r, filename_r + root_len, causal_dir_r_len);
		filename_r[causal_dir_r_len - 1] = '/';
		filename_r[causal_dir_r_len] = '\0';
		STRDUP(files->resolved_path[i], filename_r, goto error);
		i++;
	}

	for(; i < files->count; i++) {
		char *filename = files->files[i].name;
		size_t filename_len = strlen(filename);
		size_t filename_r_len = filename_len;
		struct stat sbuf;
		int exists;

		if(resolving) {
			if(filename_len < causal_dir_len || strncmp(filename, causal_dir, causal_dir_len) != 0) {
				/* not inside causal_dir anymore */
				break;
			}

			filename_r_len = filename_len + causal_dir_r_len - causal_dir_len;
			if(filename_r_len >= PATH_MAX) {
				/* resolved path is too long */
				STRDUP(files->resolved_path[i], filename, goto error);
				continue;
			}

			strcpy(filename_r + causal_dir_r_len, filename + causal_dir_len);
		} else {
			filename_r = filename;
		}

		/* deal with files and paths too long to resolve*/
		if(filename[filename_len - 1] != '/' || root_len + filename_r_len >= PATH_MAX) {
			STRDUP(files->resolved_path[i], filename_r, goto error);
			continue;
		}

		/* construct absolute path and stat() */
		strcpy(path + root_len, filename_r);
		exists = !_alpm_lstat(path, &sbuf);

		/* deal with symlinks */
		if(exists && S_ISLNK(sbuf.st_mode)) {
			i = _alpm_filelist_resolve_link(files, i, path, root_len, 1);
			continue;
		}

		/* deal with normal directories */
		STRDUP(files->resolved_path[i], filename_r, goto error);

		/* deal with children of non-existent directories to reduce lstat() calls */
		if (!exists) {
			for(i++; i < files->count; i++) {
				char *f = files->files[i].name;;
				size_t f_len = strlen(f);
				size_t f_r_len;

				if(f_len < filename_len || strncmp(f, filename, filename_len) != 0) {
					/* not inside the non-existent dir anymore */
					break;
				}

				f_r_len = f_len + causal_dir_r_len - causal_dir_len;
				if(resolving && f_r_len <= PATH_MAX) {
					strcpy(filename_r + causal_dir_r_len, f + causal_dir_len);
					STRDUP(files->resolved_path[i], filename_r, goto error);
				} else {
					STRDUP(files->resolved_path[i], f, goto error);
				}
			}
			i--;
		}
	}

	if(resolving) {
		FREE(filename_r);
	}

	return i-1;

error:
	if(resolving) {
		FREE(filename_r);
	}
	/* out of memory, not much point in going on */
	return files->count;
}

/**
 * @brief Takes a file list and resolves all directory paths according to the
 * filesystem
 *
 * @attention Pre-condition: files must be sorted!
 *
 * @note A symlink and directory at the same path in two difference packages
 * causes a conflict so the filepath can not change as packages get installed
 *
 * @param handle the context handle
 * @param files list of files to resolve
 */
void _alpm_filelist_resolve(alpm_handle_t *handle, alpm_filelist_t *files)
{
	char path[PATH_MAX];
	size_t root_len;

	if(!files || files->resolved_path) {
		return;
	}

	CALLOC(files->resolved_path, files->count, sizeof(char *), return);

	/* not much point in going on if we can't even resolve root */
	if(realpath(handle->root, path) == NULL){
		return;
	}
	root_len = strlen(path) + 1;
	if(root_len >= PATH_MAX) {
		return;
	}
	path[root_len - 1] = '/';
	path[root_len] = '\0';

	_alpm_filelist_resolve_link(files, 0, path, root_len, 0);

	qsort(files->resolved_path, files->count, sizeof(char *),
			_alpm_filelist_strcmp);

	return;
}


/* Returns the difference of the provided two lists of files.
 * Pre-condition: both lists are sorted!
 * When done, free the list but NOT the contained data.
 */
alpm_list_t *_alpm_filelist_difference(alpm_filelist_t *filesA,
		alpm_filelist_t *filesB)
{
	alpm_list_t *ret = NULL;
	size_t ctrA = 0, ctrB = 0;

	while(ctrA < filesA->count && ctrB < filesB->count) {
		alpm_file_t *fileA = filesA->files + ctrA;
		alpm_file_t *fileB = filesB->files + ctrB;
		const char *strA = fileA->name;
		const char *strB = fileB->name;
		/* skip directories, we don't care about them */
		if(strA[strlen(strA)-1] == '/') {
			ctrA++;
		} else if(strB[strlen(strB)-1] == '/') {
			ctrB++;
		} else {
			int cmp = strcmp(strA, strB);
			if(cmp < 0) {
				/* item only in filesA, qualifies as a difference */
				ret = alpm_list_add(ret, fileA);
				ctrA++;
			} else if(cmp > 0) {
				ctrB++;
			} else {
				ctrA++;
				ctrB++;
			}
		}
	}

	/* ensure we have completely emptied pA */
	while(ctrA < filesA->count) {
		alpm_file_t *fileA = filesA->files + ctrA;
		const char *strA = fileA->name;
		/* skip directories */
		if(strA[strlen(strA)-1] != '/') {
			ret = alpm_list_add(ret, fileA);
		}
		ctrA++;
	}

	return ret;
}

/* Returns the intersection of the provided two lists of files.
 * Pre-condition: both lists are sorted!
 * When done, free the list but NOT the contained data.
 */
alpm_list_t *_alpm_filelist_intersection(alpm_filelist_t *filesA,
		alpm_filelist_t *filesB)
{
	alpm_list_t *ret = NULL;
	size_t ctrA = 0, ctrB = 0;

	while(ctrA < filesA->count && ctrB < filesB->count) {
		int cmp, isdirA, isdirB;
		char *strA, *strB;

		alpm_file_t *fileA = filesA->files + ctrA;
		alpm_file_t *fileB = filesB->files + ctrB;

		isdirA = 0;
		strA = fileA->name;
		if(strA[strlen(strA)-1] == '/') {
			isdirA = 1;
			strA = strndup(fileA->name, strlen(strA)-1);
		}

		isdirB = 0;
		strB = fileB->name;
		if(strB[strlen(strB)-1] == '/') {
			isdirB = 1;
			strB = strndup(fileB->name, strlen(strB)-1);
		}

		cmp = strcmp(strA, strB);
		if(cmp < 0) {
			ctrA++;
		} else if(cmp > 0) {
			ctrB++;
		} else {
			/* TODO: this creates conflicts between a symlink to a directory in
			 * one package and a real directory in the other. For example,
			 * lib -> /usr/lib in pkg1 and /lib in pkg2.  This would be allowed
			 * when installing one package at a time _provided_ pkg1 is installed
			 * first. This will need adjusted if the order of package install can
			 * be guaranteed to install the symlink first */

			/* when not directories, item in both qualifies as an intersect */
			if(! (isdirA && isdirB)) {
				ret = alpm_list_add(ret, fileA);
			}
			ctrA++;
			ctrB++;
		}

		if(isdirA) {
			free(strA);
		}
		if(isdirB) {
			free(strB);
		}
	}

	return ret;
}

/* Helper function for comparing files list entries
 */
int _alpm_files_cmp(const void *f1, const void *f2)
{
	const alpm_file_t *file1 = f1;
	const alpm_file_t *file2 = f2;
	return strcmp(file1->name, file2->name);
}


alpm_file_t *alpm_filelist_contains(alpm_filelist_t *filelist,
		const char *path)
{
	alpm_file_t key;

	if(!filelist) {
		return NULL;
	}

	key.name = (char *)path;

	return bsearch(&key, filelist->files, filelist->count,
			sizeof(alpm_file_t), _alpm_files_cmp);
}

/* vim: set ts=2 sw=2 noet: */
