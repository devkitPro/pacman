/*
 *  db.c
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

#include "config.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#ifdef CYGWIN
#include <limits.h> /* PATH_MAX */
#endif
/* pacman */
#include "log.h"
#include "util.h"
#include "group.h"
#include "cache.h"
#include "db.h"
#include "alpm.h"

/* Open a database and return a pmdb_t handle */
pmdb_t *db_open(char *root, char *dbpath, char *treename)
{
	pmdb_t *db;

	if(root == NULL || dbpath == NULL || treename == NULL) {
		return(NULL);
	}

	_alpm_log(PM_LOG_DEBUG, "opening database '%s'", treename);

	MALLOC(db, sizeof(pmdb_t));

	MALLOC(db->path, strlen(root)+strlen(dbpath)+strlen(treename)+2);
	sprintf(db->path, "%s%s/%s", root, dbpath, treename);

	db->dir = opendir(db->path);
	if(db->dir == NULL) {
		FREE(db->path);
		FREE(db);
		return(NULL);
	}

	STRNCPY(db->treename, treename, DB_TREENAME_LEN);

	db->pkgcache = NULL;
	db->grpcache = NULL;

	db_getlastupdate(db, db->lastupdate);

	return(db);
}

void db_close(pmdb_t *db)
{
	if(db == NULL) {
		return;
	}

	_alpm_log(PM_LOG_DEBUG, "closing database '%s'", db->treename);

	if(db->dir) {
		closedir(db->dir);
		db->dir = NULL;
	}
	FREE(db->path);

	db_free_pkgcache(db);
	db_free_grpcache(db);

	free(db);

	return;
}

int db_create(char *root, char *dbpath, char *treename)
{
	char path[PATH_MAX];

	if(root == NULL || dbpath == NULL || treename == NULL) {
		return(-1);
	}

	snprintf(path, PATH_MAX, "%s%s/%s", root, dbpath, treename);
	if(_alpm_makepath(path) != 0) {
		return(-1);
	}

	return(0);
}

/* reads dbpath/.lastupdate and populates *ts with the contents.
 * *ts should be malloc'ed and should be at least 15 bytes.
 *
 * Returns 0 on success, 1 on error
 *
 */
int db_getlastupdate(pmdb_t *db, char *ts)
{
	FILE *fp;
	char path[PATH_MAX];

	if(db == NULL || ts == NULL) {
		return(-1);
	}

	/* get the last update time, if it's there */
	snprintf(path, PATH_MAX, "%s/.lastupdate", db->path);
	if((fp = fopen(path, "r")) == NULL) {
		return(-1);
	} else {
		char line[256];
		if(fgets(line, sizeof(line), fp)) {
			STRNCPY(ts, line, 15); /* YYYYMMDDHHMMSS */
			ts[14] = '\0';
		} else {
			fclose(fp);
			return(-1);
		}
	}
	fclose(fp);
	return(0);
}

/* writes the dbpath/.lastupdate with the contents of *ts
 */
int db_setlastupdate(pmdb_t *db, char *ts)
{
	FILE *fp;
	char file[PATH_MAX];

	if(db == NULL || ts == NULL || strlen(ts) == 0) {
		return(-1);
	}

	snprintf(file, PATH_MAX, "%s/.lastupdate", db->path);
	if((fp = fopen(file, "w")) == NULL) {
		return(-1);
	}
	if(fputs(ts, fp) <= 0) {
		fclose(fp);
		return(-1);
	}
	fclose(fp);

	STRNCPY(db->lastupdate, ts, DB_UPDATE_LEN);

	return(0);
}

void db_rewind(pmdb_t *db)
{
	if(db == NULL || db->dir == NULL) {
		return;
	}

	rewinddir(db->dir);
}

pmpkg_t *db_scan(pmdb_t *db, char *target, unsigned int inforeq)
{
	struct dirent *ent = NULL;
	struct stat sbuf;
	char path[PATH_MAX];
	char name[(PKG_NAME_LEN-1)+1+(PKG_VERSION_LEN-1)+1];
	char *ptr = NULL;
	int found = 0;
	pmpkg_t *pkg;

	if(db == NULL) {
		return(NULL);
	}

	if(target != NULL) {
		/* search for a specific package (by name only) */
		db_rewind(db);
		while(!found && (ent = readdir(db->dir)) != NULL) {
			if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
				continue;
			}
			STRNCPY(name, ent->d_name, (PKG_NAME_LEN-1)+1+(PKG_VERSION_LEN-1)+1);
			/* stat the entry, make sure it's a directory */
			snprintf(path, PATH_MAX, "%s/%s", db->path, name);
			if(stat(path, &sbuf) || !S_ISDIR(sbuf.st_mode)) {
				continue;
			}
			/* truncate the string at the second-to-last hyphen, */
			/* which will give us the package name */
			if((ptr = rindex(name, '-'))) {
				*ptr = '\0';
			}
			if((ptr = rindex(name, '-'))) {
				*ptr = '\0';
			}
			if(!strcmp(name, target)) {
				found = 1;
			}
		}
		if(!found) {
			return(NULL);
		}
	} else {
		/* normal iteration */
		int isdir = 0;
		while(!isdir) {
			ent = readdir(db->dir);
			if(ent == NULL) {
				return(NULL);
			}
			/* stat the entry, make sure it's a directory */
			snprintf(path, PATH_MAX, "%s/%s", db->path, ent->d_name);
			if(!stat(path, &sbuf) && S_ISDIR(sbuf.st_mode)) {
				isdir = 1;
			}
			if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
				isdir = 0;
				continue;
			}
		}
	}

	pkg = pkg_new();
	if(pkg == NULL) {
		return(NULL);
	}
	if(db_read(db, ent->d_name, inforeq, pkg) == -1) {
		FREEPKG(pkg);
	}

	return(pkg);
}

int db_read(pmdb_t *db, char *name, unsigned int inforeq, pmpkg_t *info)
{
	FILE *fp = NULL;
	struct stat buf;
	char path[PATH_MAX];
	char line[512];

	if(db == NULL || name == NULL || info == NULL) {
		return(-1);
	}

	snprintf(path, PATH_MAX, "%s/%s", db->path, name);
	if(stat(path, &buf)) {
		/* directory doesn't exist or can't be opened */
		return(-1);
	}

	/* DESC */
	if(inforeq & INFRQ_DESC) {
		snprintf(path, PATH_MAX, "%s/%s/desc", db->path, name);
		fp = fopen(path, "r");
		if(fp == NULL) {
			_alpm_log(PM_LOG_ERROR, "%s (%s)", path, strerror(errno));
			return(-1);
		}
		while(!feof(fp)) {
			if(fgets(line, 256, fp) == NULL) {
				break;
			}
			_alpm_strtrim(line);
			if(!strcmp(line, "%NAME%")) {
				if(fgets(info->name, sizeof(info->name), fp) == NULL) {
					return(-1);
				}
				_alpm_strtrim(info->name);
			} else if(!strcmp(line, "%VERSION%")) {
				if(fgets(info->version, sizeof(info->version), fp) == NULL) {
					return(-1);
				}
				_alpm_strtrim(info->version);
			} else if(!strcmp(line, "%DESC%")) {
				if(fgets(info->desc, sizeof(info->desc), fp) == NULL) {
					return(-1);
				}
				_alpm_strtrim(info->desc);
			} else if(!strcmp(line, "%GROUPS%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->groups = pm_list_add(info->groups, strdup(line));
				}
			} else if(!strcmp(line, "%URL%")) {
				if(fgets(info->url, sizeof(info->url), fp) == NULL) {
					return(-1);
				}
				_alpm_strtrim(info->url);
			} else if(!strcmp(line, "%LICENSE%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->license = pm_list_add(info->license, strdup(line));
				}
			} else if(!strcmp(line, "%ARCH%")) {
				if(fgets(info->arch, sizeof(info->arch), fp) == NULL) {
					return(-1);
				}
				_alpm_strtrim(info->arch);
			} else if(!strcmp(line, "%BUILDDATE%")) {
				if(fgets(info->builddate, sizeof(info->builddate), fp) == NULL) {
					return(-1);
				}
				_alpm_strtrim(info->builddate);
			} else if(!strcmp(line, "%INSTALLDATE%")) {
				if(fgets(info->installdate, sizeof(info->installdate), fp) == NULL) {
					return(-1);
				}
				_alpm_strtrim(info->installdate);
			} else if(!strcmp(line, "%PACKAGER%")) {
				if(fgets(info->packager, sizeof(info->packager), fp) == NULL) {
					return(-1);
				}
				_alpm_strtrim(info->packager);
			} else if(!strcmp(line, "%REASON%")) {
				char tmp[32];
				if(fgets(tmp, sizeof(tmp), fp) == NULL) {
					return(-1);
				}
				_alpm_strtrim(tmp);
				info->reason = atol(tmp);
			} else if(!strcmp(line, "%SIZE%") || !strcmp(line, "%CSIZE%")) {
				/* NOTE: the CSIZE and SIZE fields both share the "size" field
				 *       in the pkginfo_t struct.  This can be done b/c CSIZE
				 *       is currently only used in sync databases, and SIZE is
				 *       only used in local databases.
				 */
				char tmp[32];
				if(fgets(tmp, sizeof(tmp), fp) == NULL) {
					return(-1);
				}
				_alpm_strtrim(tmp);
				info->size = atol(tmp);
			} else if(!strcmp(line, "%REPLACES%")) {
				/* the REPLACES tag is special -- it only appears in sync repositories,
				 * not the local one. */
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->replaces = pm_list_add(info->replaces, strdup(line));
				}
			} else if(!strcmp(line, "%MD5SUM%")) {
				/* MD5SUM tag only appears in sync repositories,
				 * not the local one. */
				if(fgets(info->md5sum, sizeof(info->md5sum), fp) == NULL) {
					return(-1);
				}
			} else if(!strcmp(line, "%FORCE%")) {
				/* FORCE tag only appears in sync repositories,
				 * not the local one. */
				info->force = 1;
			}
		}
		fclose(fp);
	}

	/* FILES */
	if(inforeq & INFRQ_FILES) {
		snprintf(path, PATH_MAX, "%s/%s/files", db->path, name);
		fp = fopen(path, "r");
		if(fp == NULL) {
			_alpm_log(PM_LOG_ERROR, "%s (%s)", path, strerror(errno));
			return(-1);
		}
		while(fgets(line, 256, fp)) {
			_alpm_strtrim(line);
			if(!strcmp(line, "%FILES%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->files = pm_list_add(info->files, strdup(line));
				}
			} else if(!strcmp(line, "%BACKUP%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->backup = pm_list_add(info->backup, strdup(line));
				}
			}
		}
		fclose(fp);
	}

	/* DEPENDS */
	if(inforeq & INFRQ_DEPENDS) {
		snprintf(path, PATH_MAX, "%s/%s/depends", db->path, name);
		fp = fopen(path, "r");
		if(fp == NULL) {
			_alpm_log(PM_LOG_ERROR, "%s (%s)", path, strerror(errno));
			return(-1);
		}
		while(!feof(fp)) {
			fgets(line, 255, fp);
			_alpm_strtrim(line);
			if(!strcmp(line, "%DEPENDS%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->depends = pm_list_add(info->depends, strdup(line));
				}
			} else if(!strcmp(line, "%REQUIREDBY%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->requiredby = pm_list_add(info->requiredby, strdup(line));
				}
			} else if(!strcmp(line, "%CONFLICTS%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->conflicts = pm_list_add(info->conflicts, strdup(line));
				}
			} else if(!strcmp(line, "%PROVIDES%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->provides = pm_list_add(info->provides, strdup(line));
				}
			}
		}
		fclose(fp);
	}

	/* INSTALL */
	if(inforeq & INFRQ_SCRIPLET) {
		snprintf(path, PATH_MAX, "%s/%s/install", db->path, name);
		if(!stat(path, &buf)) {
			info->scriptlet = 1;
		}
	}

	/* internal */
	info->infolevel |= inforeq;

	return(0);
}

int db_write(pmdb_t *db, pmpkg_t *info, unsigned int inforeq)
{
	char topdir[PATH_MAX];
	FILE *fp = NULL;
	char path[PATH_MAX];
	mode_t oldmask;
	PMList *lp = NULL;

	if(db == NULL || info == NULL) {
		return(-1);
	}

	snprintf(topdir, PATH_MAX, "%s/%s-%s", db->path, info->name, info->version);
	oldmask = umask(0000);
	mkdir(topdir, 0755);
	/* make sure we have a sane umask */
	umask(0022);

	/* DESC */
	if(inforeq & INFRQ_DESC) {
		snprintf(path, PATH_MAX, "%s/desc", topdir);
		if((fp = fopen(path, "w")) == NULL) {
			_alpm_log(PM_LOG_ERROR, "db_write: could not open file %s/desc", db->treename);
			goto error;
		}
		fputs("%NAME%\n", fp);
		fprintf(fp, "%s\n\n", info->name);
		fputs("%VERSION%\n", fp);
		fprintf(fp, "%s\n\n", info->version);
		fputs("%DESC%\n", fp);
		fprintf(fp, "%s\n\n", info->desc);
		fputs("%GROUPS%\n", fp);
		for(lp = info->groups; lp; lp = lp->next) {
			fprintf(fp, "%s\n", (char *)lp->data);
		}
		fprintf(fp, "\n");
		fputs("%URL%\n", fp);
		fprintf(fp, "%s\n\n", info->url);
		fputs("%LICENSE%\n", fp);
		for(lp = info->license; lp; lp = lp->next) {
			fprintf(fp, "%s\n", (char *)lp->data);
		}
		fprintf(fp, "\n");
		fputs("%ARCH%\n", fp);
		fprintf(fp, "%s\n\n", info->arch);
		fputs("%BUILDDATE%\n", fp);
		fprintf(fp, "%s\n\n", info->builddate);
		fputs("%INSTALLDATE%\n", fp);
		fprintf(fp, "%s\n\n", info->installdate);
		fputs("%PACKAGER%\n", fp);
		fprintf(fp, "%s\n\n", info->packager);
		fputs("%SIZE%\n", fp);
		fprintf(fp, "%ld\n\n", info->size);
		fputs("%REASON%\n", fp);
		fprintf(fp, "%d\n\n", info->reason);
		fclose(fp);
	}

	/* FILES */
	if(inforeq & INFRQ_FILES) {
		snprintf(path, PATH_MAX, "%s/files", topdir);
		if((fp = fopen(path, "w")) == NULL) {
			_alpm_log(PM_LOG_ERROR, "db_write: could not open file %s/files", db->treename);
			goto error;
		}
		fputs("%FILES%\n", fp);
		for(lp = info->files; lp; lp = lp->next) {
			fprintf(fp, "%s\n", (char *)lp->data);
		}
		fprintf(fp, "\n");
		fputs("%BACKUP%\n", fp);
		for(lp = info->backup; lp; lp = lp->next) {
			fprintf(fp, "%s\n", (char *)lp->data);
		}
		fprintf(fp, "\n");
		fclose(fp);
	}

	/* DEPENDS */
	if(inforeq & INFRQ_DEPENDS) {
		snprintf(path, PATH_MAX, "%s/depends", topdir);
		if((fp = fopen(path, "w")) == NULL) {
			_alpm_log(PM_LOG_ERROR, "db_write: could not open file %s/depends", db->treename);
			goto error;
		}
		fputs("%DEPENDS%\n", fp);
		for(lp = info->depends; lp; lp = lp->next) {
			fprintf(fp, "%s\n", (char *)lp->data);
		}
		fprintf(fp, "\n");
		fputs("%REQUIREDBY%\n", fp);
		for(lp = info->requiredby; lp; lp = lp->next) {
			fprintf(fp, "%s\n", (char *)lp->data);
		}
		fprintf(fp, "\n");
		fputs("%CONFLICTS%\n", fp);
		for(lp = info->conflicts; lp; lp = lp->next) {
			fprintf(fp, "%s\n", (char *)lp->data);
		}
		fprintf(fp, "\n");
		fputs("%PROVIDES%\n", fp);
		for(lp = info->provides; lp; lp = lp->next) {
			fprintf(fp, "%s\n", (char *)lp->data);
		}
		fprintf(fp, "\n");
		fclose(fp);
	}

	/* INSTALL */
	/* nothing needed here (script is automatically extracted) */

	umask(oldmask);

	return(0);

error:
	umask(oldmask);
	return(-1);
}

int db_remove(pmdb_t *db, pmpkg_t *info)
{
	char topdir[PATH_MAX];
	char file[PATH_MAX];

	if(db == NULL || info == NULL) {
		return(-1);
	}

	snprintf(topdir, PATH_MAX, "%s/%s-%s", db->path, info->name, info->version);

	/* DESC */
	snprintf(file, PATH_MAX, "%s/desc", topdir);
	unlink(file);
	/* FILES */
	snprintf(file, PATH_MAX, "%s/files", topdir);
	unlink(file);
	/* DEPENDS */
	snprintf(file, PATH_MAX, "%s/depends", topdir);
	unlink(file);
	/* INSTALL */
	snprintf(file, PATH_MAX, "%s/install", topdir);
	unlink(file);

	/* Package directory */
	if(rmdir(topdir) == -1) {
		return(-1);
	}

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
