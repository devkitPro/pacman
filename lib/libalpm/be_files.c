/*
 *  be_files.c
 * 
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
#include <dirent.h>
#ifdef CYGWIN
#include <limits.h> /* PATH_MAX */
#endif
/* pacman */
#include "log.h"
#include "util.h"
#include "db.h"
#include "alpm.h"

int _alpm_db_open(pmdb_t *db, int mode)
{
	char path[PATH_MAX];

	if(db == NULL) {
		return(-1);
	}

	snprintf(path, PATH_MAX, "%s/%s", db->path, db->treename);
	
	db->handle = opendir(path);
	if(db->handle == NULL) {
		if(mode & DB_O_CREATE) {
			_alpm_log(PM_LOG_WARNING, "could not open database '%s' -- try creating it", db->treename);
			_alpm_log(PM_LOG_DEBUG, "creating database '%s'", db->treename);
			if(mkdir(path, 0755) == 0) {
				db->handle = opendir(path);
			}
		}
		if(!(mode & DB_O_CREATE) || db->handle == NULL) {
			return(-1);
		}
	}

	return(0);
}

void _alpm_db_close(pmdb_t *db)
{
	if(db == NULL) {
		return;
	}

	if(db->handle) {
		closedir(db->handle);
		db->handle = NULL;
	}
}

void _alpm_db_rewind(pmdb_t *db)
{
	if(db == NULL || db->handle == NULL) {
		return;
	}

	rewinddir(db->handle);
}

pmpkg_t *_alpm_db_scan(pmdb_t *db, char *target, unsigned int inforeq)
{
	struct dirent *ent = NULL;
	struct stat sbuf;
	char path[PATH_MAX];
	char name[PKG_FULLNAME_LEN];
	char *ptr = NULL;
	int found = 0;
	pmpkg_t *pkg;

	if(db == NULL) {
		return(NULL);
	}

	if(target != NULL) {
		/* search for a specific package (by name only) */
		rewinddir(db->handle);
		while(!found && (ent = readdir(db->handle)) != NULL) {
			if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
				continue;
			}
			/* stat the entry, make sure it's a directory */
			snprintf(path, PATH_MAX, "%s/%s/%s", db->path, db->treename, ent->d_name);
			if(stat(path, &sbuf) || !S_ISDIR(sbuf.st_mode)) {
				continue;
			}
			STRNCPY(name, ent->d_name, PKG_FULLNAME_LEN);
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
			ent = readdir(db->handle);
			if(ent == NULL) {
				return(NULL);
			}
			if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
				isdir = 0;
				continue;
			}
			/* stat the entry, make sure it's a directory */
			snprintf(path, PATH_MAX, "%s/%s/%s", db->path, db->treename, ent->d_name);
			if(!stat(path, &sbuf) && S_ISDIR(sbuf.st_mode)) {
				isdir = 1;
			}
		}
	}

	pkg = _alpm_pkg_new(NULL, NULL);
	if(pkg == NULL) {
		return(NULL);
	}
	if(_alpm_pkg_splitname(ent->d_name, pkg->name, pkg->version) == -1) {
		_alpm_log(PM_LOG_ERROR, "invalid name for dabatase entry '%s'", ent->d_name);
		return(NULL);
	}
	if(_alpm_db_read(db, inforeq, pkg) == -1) {
		FREEPKG(pkg);
	}

	return(pkg);
}

int _alpm_db_read(pmdb_t *db, unsigned int inforeq, pmpkg_t *info)
{
	FILE *fp = NULL;
	struct stat buf;
	char path[PATH_MAX];
	char line[512];

	if(db == NULL || info == NULL || info->name[0] == 0 || info->version[0] == 0) {
		return(-1);
	}

	snprintf(path, PATH_MAX, "%s/%s/%s-%s", db->path, db->treename, info->name, info->version);
	if(stat(path, &buf)) {
		/* directory doesn't exist or can't be opened */
		return(-1);
	}

	/* DESC */
	if(inforeq & INFRQ_DESC) {
		snprintf(path, PATH_MAX, "%s/%s/%s-%s/desc", db->path, db->treename, info->name, info->version);
		fp = fopen(path, "r");
		if(fp == NULL) {
			_alpm_log(PM_LOG_ERROR, "%s (%s)", path, strerror(errno));
			goto error;
		}
		while(!feof(fp)) {
			if(fgets(line, 256, fp) == NULL) {
				break;
			}
			_alpm_strtrim(line);
			if(!strcmp(line, "%DESC%")) {
				if(fgets(info->desc, sizeof(info->desc), fp) == NULL) {
					goto error;
				}
				_alpm_strtrim(info->desc);
			} else if(!strcmp(line, "%GROUPS%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->groups = _alpm_list_add(info->groups, strdup(line));
				}
			} else if(!strcmp(line, "%URL%")) {
				if(fgets(info->url, sizeof(info->url), fp) == NULL) {
					goto error;
				}
				_alpm_strtrim(info->url);
			} else if(!strcmp(line, "%LICENSE%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->license = _alpm_list_add(info->license, strdup(line));
				}
			} else if(!strcmp(line, "%ARCH%")) {
				if(fgets(info->arch, sizeof(info->arch), fp) == NULL) {
					goto error;
				}
				_alpm_strtrim(info->arch);
			} else if(!strcmp(line, "%BUILDDATE%")) {
				if(fgets(info->builddate, sizeof(info->builddate), fp) == NULL) {
					goto error;
				}
				_alpm_strtrim(info->builddate);
			} else if(!strcmp(line, "%INSTALLDATE%")) {
				if(fgets(info->installdate, sizeof(info->installdate), fp) == NULL) {
					goto error;
				}
				_alpm_strtrim(info->installdate);
			} else if(!strcmp(line, "%PACKAGER%")) {
				if(fgets(info->packager, sizeof(info->packager), fp) == NULL) {
					goto error;
				}
				_alpm_strtrim(info->packager);
			} else if(!strcmp(line, "%REASON%")) {
				char tmp[32];
				if(fgets(tmp, sizeof(tmp), fp) == NULL) {
					goto error;
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
					goto error;
				}
				_alpm_strtrim(tmp);
				info->size = atol(tmp);
			} else if(!strcmp(line, "%MD5SUM%")) {
				/* MD5SUM tag only appears in sync repositories,
				 * not the local one. */
				if(fgets(info->md5sum, sizeof(info->md5sum), fp) == NULL) {
					goto error;
				}
			/* XXX: these are only here as backwards-compatibility for pacman 2.x
			 * sync repos.... in pacman3, they have been moved to DEPENDS.
			 * Remove this when we move to pacman3 repos.
			 */
			} else if(!strcmp(line, "%REPLACES%")) {
				/* the REPLACES tag is special -- it only appears in sync repositories,
				 * not the local one. */
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->replaces = _alpm_list_add(info->replaces, strdup(line));
				}
			} else if(!strcmp(line, "%FORCE%")) {
				/* FORCE tag only appears in sync repositories,
				 * not the local one. */
				info->force = 1;
			}
		}
		fclose(fp);
		fp = NULL;
	}

	/* FILES */
	if(inforeq & INFRQ_FILES) {
		snprintf(path, PATH_MAX, "%s/%s/%s-%s/files", db->path, db->treename, info->name, info->version);
		fp = fopen(path, "r");
		if(fp == NULL) {
			_alpm_log(PM_LOG_ERROR, "%s (%s)", path, strerror(errno));
			goto error;
		}
		while(fgets(line, 256, fp)) {
			_alpm_strtrim(line);
			if(!strcmp(line, "%FILES%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->files = _alpm_list_add(info->files, strdup(line));
				}
			} else if(!strcmp(line, "%BACKUP%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->backup = _alpm_list_add(info->backup, strdup(line));
				}
			}
		}
		fclose(fp);
		fp = NULL;
	}

	/* DEPENDS */
	if(inforeq & INFRQ_DEPENDS) {
		snprintf(path, PATH_MAX, "%s/%s/%s-%s/depends", db->path, db->treename, info->name, info->version);
		fp = fopen(path, "r");
		if(fp == NULL) {
			_alpm_log(PM_LOG_ERROR, "%s (%s)", path, strerror(errno));
			goto error;
		}
		while(!feof(fp)) {
			fgets(line, 255, fp);
			_alpm_strtrim(line);
			if(!strcmp(line, "%DEPENDS%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->depends = _alpm_list_add(info->depends, strdup(line));
				}
			} else if(!strcmp(line, "%REQUIREDBY%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->requiredby = _alpm_list_add(info->requiredby, strdup(line));
				}
			} else if(!strcmp(line, "%CONFLICTS%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->conflicts = _alpm_list_add(info->conflicts, strdup(line));
				}
			} else if(!strcmp(line, "%PROVIDES%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->provides = _alpm_list_add(info->provides, strdup(line));
				}
			} else if(!strcmp(line, "%REPLACES%")) {
				/* the REPLACES tag is special -- it only appears in sync repositories,
				 * not the local one. */
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->replaces = _alpm_list_add(info->replaces, strdup(line));
				}
			} else if(!strcmp(line, "%FORCE%")) {
				/* FORCE tag only appears in sync repositories,
				 * not the local one. */
				info->force = 1;
			}
		}
		fclose(fp);
		fp = NULL;
	}

	/* INSTALL */
	if(inforeq & INFRQ_SCRIPLET) {
		snprintf(path, PATH_MAX, "%s/%s/%s-%s/install", db->path, db->treename, info->name, info->version);
		if(!stat(path, &buf)) {
			info->scriptlet = 1;
		}
	}

	/* internal */
	info->infolevel |= inforeq;

	return(0);

error:
	if(fp) {
		fclose(fp);
	}
	return(-1);
}

int _alpm_db_write(pmdb_t *db, pmpkg_t *info, unsigned int inforeq)
{
	FILE *fp = NULL;
	char path[PATH_MAX];
	mode_t oldmask;
	PMList *lp = NULL;
	int retval = 0;
	int local = 0;

	if(db == NULL || info == NULL) {
		return(-1);
	}

	snprintf(path, PATH_MAX, "%s/%s/%s-%s", db->path, db->treename, info->name, info->version);
	oldmask = umask(0000);
	mkdir(path, 0755);
	/* make sure we have a sane umask */
	umask(0022);

	if(strcmp(db->treename, "local") == 0) {
		local = 1;
	}

	/* DESC */
	if(inforeq & INFRQ_DESC) {
		snprintf(path, PATH_MAX, "%s/%s/%s-%s/desc", db->path, db->treename, info->name, info->version);
		if((fp = fopen(path, "w")) == NULL) {
			_alpm_log(PM_LOG_ERROR, "db_write: could not open file %s/desc", db->treename);
			retval = 1;
			goto cleanup;
		}
		fprintf(fp, "%%NAME%%\n%s\n\n"
			"%%VERSION%%\n%s\n\n", info->name, info->version);
		if(info->desc[0]) {
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
		if(local) {
			if(info->url[0]) {
				fprintf(fp, "%%URL%%\n"
					"%s\n\n", info->url);
			}
			if(info->license) {
				fputs("%LICENSE%\n", fp);
				for(lp = info->license; lp; lp = lp->next) {
					fprintf(fp, "%s\n", (char *)lp->data);
				}
				fprintf(fp, "\n");
			}
			if(info->arch[0]) {
				fprintf(fp, "%%ARCH%%\n"
					"%s\n\n", info->arch);
			}
			if(info->builddate[0]) {
				fprintf(fp, "%%BUILDDATE%%\n"
					"%s\n\n", info->builddate);
			}
			if(info->installdate[0]) {
				fprintf(fp, "%%INSTALLDATE%%\n"
					"%s\n\n", info->installdate);
			}
			if(info->packager[0]) {
				fprintf(fp, "%%PACKAGER%%\n"
					"%s\n\n", info->packager);
			}
			if(info->size) {
				fprintf(fp, "%%SIZE%%\n"
					"%ld\n\n", info->size);
			}
			if(info->reason) {
				fprintf(fp, "%%REASON%%\n"
					"%d\n\n", info->reason);
			}
		} else {
			if(info->size) {
				fprintf(fp, "%%CSIZE%%\n"
					"%ld\n\n", info->size);
			}
			if(info->reason) {
				fprintf(fp, "%%MD5SUM%%\n"
					"%s\n\n", info->md5sum);
			}
		}
		fclose(fp);
		fp = NULL;
	}

	/* FILES */
	if(local && (inforeq & INFRQ_FILES)) {
		snprintf(path, PATH_MAX, "%s/%s/%s-%s/files", db->path, db->treename, info->name, info->version);
		if((fp = fopen(path, "w")) == NULL) {
			_alpm_log(PM_LOG_ERROR, "db_write: could not open file %s/files", db->treename);
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
		snprintf(path, PATH_MAX, "%s/%s/%s-%s/depends", db->path, db->treename, info->name, info->version);
		if((fp = fopen(path, "w")) == NULL) {
			_alpm_log(PM_LOG_ERROR, "db_write: could not open file %s/depends", db->treename);
			retval = -1;
			goto cleanup;
		}
		if(info->depends) {
			fputs("%DEPENDS%\n", fp);
			for(lp = info->depends; lp; lp = lp->next) {
				fprintf(fp, "%s\n", (char *)lp->data);
			}
			fprintf(fp, "\n");
		}
		if(local && info->requiredby) {
			fputs("%REQUIREDBY%\n", fp);
			for(lp = info->requiredby; lp; lp = lp->next) {
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
		if(!local) {
			if(info->replaces) {
				fputs("%REPLACES%\n", fp);
				for(lp = info->replaces; lp; lp = lp->next) {
					fprintf(fp, "%s\n", (char *)lp->data);
				}
				fprintf(fp, "\n");
			}
			if(info->force) {
				fprintf(fp, "%%FORCE%%\n"
					"\n");
			}
		}
		fclose(fp);
		fp = NULL;
	}

	/* INSTALL */
	/* nothing needed here (script is automatically extracted) */

cleanup:
	umask(oldmask);

	if(fp) {
		fclose(fp);
	}

	return(retval);
}

int _alpm_db_remove(pmdb_t *db, pmpkg_t *info)
{
	char path[PATH_MAX];

	if(db == NULL || info == NULL) {
		return(-1);
	}


	/* DESC */
	snprintf(path, PATH_MAX, "%s/%s/%s-%s/desc",
	                         db->path, db->treename, info->name, info->version);
	unlink(path);
	/* FILES */
	snprintf(path, PATH_MAX, "%s/%s/%s-%s/files",
	                         db->path, db->treename, info->name, info->version);
	unlink(path);
	/* DEPENDS */
	snprintf(path, PATH_MAX, "%s/%s/%s-%s/depends",
	                         db->path, db->treename, info->name, info->version);
	unlink(path);
	/* INSTALL */
	snprintf(path, PATH_MAX, "%s/%s/%s-%s/install",
	                         db->path, db->treename, info->name, info->version);
	unlink(path);

	/* Package directory */
	snprintf(path, PATH_MAX, "%s/%s/%s-%s",
	                         db->path, db->treename, info->name, info->version);
	if(rmdir(path) == -1) {
		return(-1);
	}

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
