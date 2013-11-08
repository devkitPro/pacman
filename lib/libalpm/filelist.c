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
		char *strA = filesA->files[ctrA].name;
		char *strB = filesB->files[ctrB].name;

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
		ret = alpm_list_add(ret, filesA->files[ctrA].name);
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
		strA = filesA->files[ctrA].name;
		if(strA[strlen(strA)-1] == '/') {
			isdirA = 1;
			strA = strndup(strA, strlen(strA)-1);
		}

		isdirB = 0;
		strB = filesB->files[ctrB].name;
		if(strB[strlen(strB)-1] == '/') {
			isdirB = 1;
			strB = strndup(strB, strlen(strB)-1);
		}

		cmp = strcmp(strA, strB);
		if(cmp < 0) {
			ctrA++;
		} else if(cmp > 0) {
			ctrB++;
		} else {
			/* TODO: this creates conflicts between a symlink to a directory in
			 * one package and a real directory in the other. For example,
			 * lib -> /usr/lib in pkg1 and /lib in pkg2. This would be allowed
			 * when installing one package at a time _provided_ pkg1 is installed
			 * first. This will need adjusted if the order of package install can
			 * be guaranteed to install the symlink first */

			/* when not directories, item in both qualifies as an intersect */
			if(! (isdirA && isdirB)) {
				ret = alpm_list_add(ret, filesA->files[ctrA].name);
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

alpm_file_t SYMEXPORT *alpm_filelist_contains(alpm_filelist_t *filelist,
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
