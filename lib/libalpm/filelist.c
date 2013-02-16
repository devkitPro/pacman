/*
 *  filelist.c
 *
 *  Copyright (c) 2012-2013 Pacman Development Team <pacman-dev@archlinux.org>
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

/* TODO make sure callers check the return value so we can bail on errors.
 * For now we soldier on as best we can, skipping paths that are too long to
 * resolve and using the original filenames on memory errors.  */
/**
 * @brief Resolves a symlink and its children.
 *
 * @attention Pre-condition: files must be sorted!
 *
 * @param files filelist to resolve
 * @param i pointer to the index in files to start processing, will point to
 * the last file processed on return
 * @param path absolute path for the symlink being resolved
 * @param root_len length of the root portion of path
 * @param resolving is file \i in \files a symlink that needs to be resolved
 *
 * @return 0 on success, -1 on error
 */
int _alpm_filelist_resolve_link(alpm_filelist_t *files, size_t *i,
		char *path, size_t root_len, int resolving)
{
	char *causal_dir = NULL; /* symlink being resolved */
	char *filename_r = NULL; /* resolved filename */
	size_t causal_dir_len = 0, causal_dir_r_len = 0;

	if(resolving) {
		/* deal with the symlink being resolved */
		MALLOC(filename_r, PATH_MAX, goto error);
		causal_dir = files->files[*i].name;
		causal_dir_len = strlen(causal_dir);
		if(realpath(path, filename_r) == NULL) {
			files->resolved_path[*i] = causal_dir;
			FREE(filename_r);
			return -1;
		}
		causal_dir_r_len = strlen(filename_r + root_len) + 1;
		if(causal_dir_r_len >= PATH_MAX) {
			files->resolved_path[*i] = causal_dir;
			FREE(filename_r);
			return -1;
		}
		/* remove root_r from filename_r */
		memmove(filename_r, filename_r + root_len, causal_dir_r_len);
		filename_r[causal_dir_r_len - 1] = '/';
		filename_r[causal_dir_r_len] = '\0';
		STRDUP(files->resolved_path[*i], filename_r, goto error);
		(*i)++;
	}

	for(; *i < files->count; (*i)++) {
		char *filename = files->files[*i].name;
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
				files->resolved_path[*i] = filename;
				continue;
			}

			strcpy(filename_r + causal_dir_r_len, filename + causal_dir_len);
		}

		/* deal with files and paths too long to resolve*/
		if(filename[filename_len - 1] != '/' || root_len + filename_r_len >= PATH_MAX) {
			if(resolving) {
				STRDUP(files->resolved_path[*i], filename_r, goto error);
			} else {
				files->resolved_path[*i] = filename;
			}
			continue;
		}

		/* construct absolute path and stat() */
		strcpy(path + root_len, resolving ? filename_r : filename);
		exists = !_alpm_lstat(path, &sbuf);

		/* deal with symlinks */
		if(exists && S_ISLNK(sbuf.st_mode)) {
			_alpm_filelist_resolve_link(files, i, path, root_len, 1);
			continue;
		}

		/* deal with normal directories */
		if(resolving) {
			STRDUP(files->resolved_path[*i], filename_r, goto error);
		} else {
			files->resolved_path[*i] = filename;
		}

		/* deal with children of non-existent directories to reduce lstat() calls */
		if(!exists) {
			for((*i)++; *i < files->count; (*i)++) {
				char *f = files->files[*i].name;
				size_t f_len = strlen(f);
				size_t f_r_len;

				if(f_len < filename_len || strncmp(f, filename, filename_len) != 0) {
					/* not inside the non-existent dir anymore */
					break;
				}

				f_r_len = f_len + causal_dir_r_len - causal_dir_len;
				if(resolving && f_r_len <= PATH_MAX) {
					strcpy(filename_r + causal_dir_r_len, f + causal_dir_len);
					STRDUP(files->resolved_path[*i], filename_r, goto error);
				} else {
					files->resolved_path[*i] = f;
				}
			}
			(*i)--;
		}
	}
	(*i)--;

	FREE(filename_r);

	return 0;

error:
	FREE(filename_r);
	/* out of memory, set remaining files to their original names */
	for(; *i < files->count; (*i)++) {
		files->resolved_path[*i] = files->files[*i].name;
	}
	(*i)--;
	return -1;
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
 *
 * @return 0 on success, -1 on error
 */
int _alpm_filelist_resolve(alpm_handle_t *handle, alpm_filelist_t *files)
{
	char path[PATH_MAX];
	size_t root_len, i = 0;
	int ret = 0;

	if(!files || files->resolved_path) {
		return 0;
	}

	CALLOC(files->resolved_path, files->count, sizeof(char *), return -1);

	/* not much point in going on if we can't even resolve root */
	if(realpath(handle->root, path) == NULL){
		return -1;
	}
	root_len = strlen(path);
	if(root_len + 1 >= PATH_MAX) {
		return -1;
	}
	/* append '/' if root is not "/" */
	if(path[root_len - 1] != '/') {
		path[root_len] = '/';
		root_len++;
		path[root_len] = '\0';
	}

	ret = _alpm_filelist_resolve_link(files, &i, path, root_len, 0);

	qsort(files->resolved_path, files->count, sizeof(char *),
			_alpm_filelist_strcmp);

	return ret;
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
		char *strA = filesA->resolved_path[ctrA];
		char *strB = filesB->resolved_path[ctrB];

		int cmp = strcmp(strA, strB);
		if(cmp < 0) {
			/* item only in filesA, qualifies as a difference */
			ret = alpm_list_add(ret, strA);
			ctrA++;
		} else if(cmp > 0) {
			ctrB++;
		} else {
			ctrA++;
			ctrB++;
		}
	}

	/* ensure we have completely emptied pA */
	while(ctrA < filesA->count) {
		ret = alpm_list_add(ret, filesA->resolved_path[ctrA]);
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

		isdirA = 0;
		strA = filesA->resolved_path[ctrA];
		if(strA[strlen(strA)-1] == '/') {
			isdirA = 1;
			strA = strndup(filesA->resolved_path[ctrA], strlen(strA)-1);
		}

		isdirB = 0;
		strB = filesB->resolved_path[ctrB];
		if(strB[strlen(strB)-1] == '/') {
			isdirB = 1;
			strB = strndup(filesB->resolved_path[ctrB], strlen(strB)-1);
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
				ret = alpm_list_add(ret, filesA->resolved_path[ctrA]);
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
