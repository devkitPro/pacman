/*
 *  backup.c
 *
 *  Copyright (c) 2006-2010 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2005 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2005 by Christian Hamar <krics@linuxforum.hu>
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

#include <stdlib.h>
#include <string.h>

/* libalpm */
#include "backup.h"
#include "alpm_list.h"
#include "log.h"
#include "util.h"

/* split a backup string "file\thash" into two strings : file and hash */
static int backup_split(const char *string, char **file, char **hash)
{
	char *str = strdup(string);
	char *ptr;

	/* tab delimiter */
	ptr = strchr(str, '\t');
	if(ptr == NULL) {
		if(file) {
			*file = str;
		} else {
			/* don't need our dup as the fname wasn't requested, so free it */
			FREE(str);
		}
		return(0);
	}
	*ptr = '\0';
	ptr++;
	/* now str points to the filename and ptr points to the hash */
	if(file) {
		*file = strdup(str);
	}
	if(hash) {
		*hash = strdup(ptr);
	}
	FREE(str);
	return(1);
}

char *_alpm_backup_file(const char *string)
{
	char *file = NULL;
	backup_split(string, &file, NULL);
	return(file);
}

char *_alpm_backup_hash(const char *string)
{
	char *hash = NULL;
	backup_split(string, NULL, &hash);
	return(hash);
}

/* Look for a filename in a pmpkg_t.backup list.  If we find it,
 * then we return the md5 hash (parsed from the same line)
 */
char *_alpm_needbackup(const char *file, const alpm_list_t *backup)
{
	const alpm_list_t *lp;

	ALPM_LOG_FUNC;

	if(file == NULL || backup == NULL) {
		return(NULL);
	}

	/* run through the backup list and parse out the hash for our file */
	for(lp = backup; lp; lp = lp->next) {
		char *filename = NULL;
		char *hash = NULL;

		/* no hash found */
		if(!backup_split((char *)lp->data, &filename, &hash)) {
			FREE(filename);
			continue;
		}
		if(strcmp(file, filename) == 0) {
			FREE(filename);
			return(hash);
		}
		FREE(filename);
		FREE(hash);
	}

	return(NULL);
}

/* vim: set ts=2 sw=2 noet: */
