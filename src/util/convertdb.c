/*
 *  convertdb.c
 * 
 *  Copyright (c) 2002-2005 by Judd Vinet <jvinet@zeroflux.org>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
 *  USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "alpm.h"
#include "list.h"
#include "util.h"

int main(int argc, char* argv[])
{
	FILE* db = NULL;
	FILE* fp = NULL;
	char* ptr = NULL;
	char  name[256];
	char  ver[256];
	char  line[PATH_MAX+1];
	char  topdir[PATH_MAX+1];
	char  path[PATH_MAX+1];
	mode_t oldumask;
	struct stat buf;
	char dbdir[PATH_MAX];
 
	sprintf(dbdir, "/%s", PM_DBPATH);

	if(argc < 2) {
		printf("converts a pacman 1.x database to a pacman 2.0 format\n");
		printf("usage: %s <target_dir>\n\n", basename(argv[0]));
		printf("convertdb will convert all package data from /var/lib/pacman/pacman.db\n");
		printf("to a 2.0 format and place it in target_dir.\n\n");
		return(0);
	}

	db = fopen(dbdir, "r");
	if(db == NULL) {
		perror(dbdir);
		return(1);
	}

	oldumask = umask(0000);
	while(fgets(name, 255, db)) {
		PMList *backup = NULL;
		PMList *lp;

		if(!fgets(ver, 255, db)) {
			perror(dbdir);
			return(1);
		}
		_alpm_strtrim(name);
		_alpm_strtrim(ver);
		fprintf(stderr, "converting %s\n", name);
		/* package directory */
		snprintf(topdir, PATH_MAX, "%s/%s-%s", argv[1], name, ver);
		mkdir(topdir, 0755);

		/* DESC */
		snprintf(path, PATH_MAX, "%s/desc", topdir);
		if(!(fp = fopen(path, "w"))) {
			perror(path);
			return(1);
		}
		fputs("%NAME%\n", fp);
		fprintf(fp, "%s\n\n", name);
		fputs("%VERSION%\n", fp);
		fprintf(fp, "%s\n\n", ver);
		fputs("%DESC%\n\n", fp);
		fclose(fp);

		/* DEPENDS */
		snprintf(path, PATH_MAX, "%s/depends", topdir);
		if(!(fp = fopen(path, "w"))) {
			perror(path);
			return(1);
		}
		fputs("%DEPENDS%\n\n", fp);
		fputs("%REQUIREDBY%\n\n", fp);
		fputs("%CONFLICTS%\n\n", fp);
		fclose(fp);

		/* FILES */
		snprintf(path, PATH_MAX, "%s/files", topdir);
		if(!(fp = fopen(path, "w"))) {
			perror(path);
			return(1);
		}
		fputs("%FILES%\n", fp);
		while(fgets(line, 255, db) && strcmp(_alpm_strtrim(line), "")) {
			_alpm_strtrim(line);
			ptr = line;

			/* check for backup designation and frontslashes that shouldn't be there */
			if(line[0] == '*') ptr++;
			if(*ptr == '/')    ptr++;
			if(line[0] == '*') backup = pm_list_add(backup, strdup(ptr));
	
			fprintf(fp, "%s\n", ptr);
		}
		fprintf(fp, "\n");
		fputs("%BACKUP%\n", fp);
		for(lp = backup; lp; lp = lp->next) {
			/* print the filename and a bad md5 hash.  we just use 32 f's cuz we can't
			 * md5 the original file since we don't have it
			 */
			fprintf(fp, "%s\tffffffffffffffffffffffffffffffff\n", (char*)lp->data);
		}
		fputs("\n", fp);
		fclose(fp);
		snprintf(path, PATH_MAX, "/var/lib/pacman/scripts/%s", name);
		if(!stat(path, &buf)) {
			snprintf(line, PATH_MAX, "/bin/cp %s %s/install", path, topdir);
			system(line);
		}
		pm_list_free(backup);
	}
	umask(oldumask);
	return(0);
}

/* vim: set ts=2 sw=2 noet: */
