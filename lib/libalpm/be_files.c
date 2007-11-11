/*
 *  be_files.c
 * 
 *  Copyright (c) 2006 by Christian Hamar <krics@linuxforum.hu>
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
#include <stdint.h> /* uintmax_t */
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <time.h>
#include <limits.h> /* PATH_MAX */

/* libalpm */
#include "db.h"
#include "alpm_list.h"
#include "log.h"
#include "util.h"
#include "alpm.h"
#include "error.h"
#include "handle.h"
#include "package.h"
#include "delta.h"


/* This function is used to convert the downloaded db file to the proper backend
 * format
 */
int _alpm_db_install(pmdb_t *db, const char *dbfile)
{
	ALPM_LOG_FUNC;

	/* TODO we should not simply unpack the archive, but better parse it and 
	 * db_write each entry (see sync_load_dbarchive to get archive content) */
	_alpm_log(PM_LOG_DEBUG, "unpacking database '%s'\n", dbfile);

	if(_alpm_unpack(dbfile, db->path, NULL)) {
		RET_ERR(PM_ERR_SYSTEM, -1);
	}

	return unlink(dbfile);
}

int _alpm_db_open(pmdb_t *db)
{
	ALPM_LOG_FUNC;

	if(db == NULL) {
		RET_ERR(PM_ERR_DB_NULL, -1);
	}

	_alpm_log(PM_LOG_DEBUG, "opening database from path '%s'\n", db->path);
	db->handle = opendir(db->path);
	if(db->handle == NULL) {
		RET_ERR(PM_ERR_DB_OPEN, -1);
	}

	return(0);
}

void _alpm_db_close(pmdb_t *db)
{
	ALPM_LOG_FUNC;

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
	ALPM_LOG_FUNC;

	if(db == NULL || db->handle == NULL) {
		return;
	}

	rewinddir(db->handle);
}

static int _alpm_db_splitname(const char *target, char *name, char *version)
{
	/* the format of a db entry is as follows:
	 *    package-version-rel/
	 * package name can contain hyphens, so parse from the back- go back
	 * two hyphens and we have split the version from the name.
	 */
	char *tmp, *p, *q;

	if(target == NULL) {
		return(-1);
	}
	tmp = strdup(target);
	p = tmp + strlen(tmp);

	/* do the magic parsing- find the beginning of the version string
	 * by doing two iterations of same loop to lop off two hyphens */
	for(q = --p; *q && *q != '-'; q--);
	for(p = --q; *p && *p != '-'; p--);
	if(*p != '-' || p == tmp) {
		return(-1);
	}

	/* copy into fields and return */
	if(version) {
		strncpy(version, p+1, PKG_VERSION_LEN);
	}
	/* insert a terminator at the end of the name (on hyphen)- then copy it */
	*p = '\0';
	if(name) {
		strncpy(name, tmp, PKG_NAME_LEN);
	}

	free(tmp);
	return(0);
}

pmpkg_t *_alpm_db_scan(pmdb_t *db, const char *target)
{
	struct dirent *ent = NULL;
	struct stat sbuf;
	char path[PATH_MAX];
	char name[PKG_FULLNAME_LEN];
	char *ptr = NULL;
	int found = 0;
	pmpkg_t *pkg = NULL;

	ALPM_LOG_FUNC;

	if(db == NULL) {
		RET_ERR(PM_ERR_DB_NULL, NULL);
	}

	/* We loop here until we read a valid package.  When an iteration of this loop
	 * fails, it means alpm_db_read failed to read a valid package, so we'll read
	 * the next so as not to abort whole-db operations early
	 */
	while(!pkg) {
		if(target != NULL) {
			/* search for a specific package (by name only) */
			rewinddir(db->handle);
			while(!found && (ent = readdir(db->handle)) != NULL) {
				if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
					continue;
				}
				/* stat the entry, make sure it's a directory */
				snprintf(path, PATH_MAX, "%s/%s", db->path, ent->d_name);
				if(stat(path, &sbuf) || !S_ISDIR(sbuf.st_mode)) {
					continue;
				}
				strncpy(name, ent->d_name, PKG_FULLNAME_LEN);
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
		} else { /* target == NULL, full scan */
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
				snprintf(path, PATH_MAX, "%s/%s", db->path, ent->d_name);
				if(!stat(path, &sbuf) && S_ISDIR(sbuf.st_mode)) {
					isdir = 1;
				}
			}
		}

		pkg = _alpm_pkg_new(NULL, NULL);
		if(pkg == NULL) {
			_alpm_log(PM_LOG_DEBUG, "db scan could not find package: %s\n", target);
			return(NULL);
		}
		/* split the db entry name */
		if(_alpm_db_splitname(ent->d_name, pkg->name, pkg->version) != 0) {
			_alpm_log(PM_LOG_ERROR, _("invalid name for database entry '%s'\n"),
					ent->d_name);
			alpm_pkg_free(pkg);
			pkg = NULL;
			continue;
		}

		/* explicitly read with only 'BASE' data, accessors will handle the rest */
		if(_alpm_db_read(db, pkg, INFRQ_BASE) == -1) {
			/* TODO removed corrupt entry from the FS here */
			_alpm_pkg_free(pkg);
		} else {
			pkg->origin = PKG_FROM_CACHE;
			pkg->origin_data.db = db;
		}
	}

	return(pkg);
}

int _alpm_db_read(pmdb_t *db, pmpkg_t *info, pmdbinfrq_t inforeq)
{
	FILE *fp = NULL;
	struct stat buf;
	char path[PATH_MAX+1];
	char line[513];

	ALPM_LOG_FUNC;

	if(db == NULL) {
		RET_ERR(PM_ERR_DB_NULL, -1);
	}

	if(info == NULL || info->name[0] == 0 || info->version[0] == 0) {
		_alpm_log(PM_LOG_DEBUG, "invalid package entry provided to _alpm_db_read, skipping\n");
		return(-1);
	}

	if(info->origin == PKG_FROM_FILE) {
		_alpm_log(PM_LOG_DEBUG, "request to read database info for a file-based package '%s', skipping...\n", info->name);
		return(-1);
	}

	/* bitmask logic here:
	 * infolevel: 00001111
	 * inforeq:   00010100
	 * & result:  00000100
	 * == to inforeq? nope, we need to load more info. */
	if((info->infolevel & inforeq) == inforeq) {
		/* already loaded this info, do nothing */
		return(0);
	}
	_alpm_log(PM_LOG_FUNCTION, _("loading package data for %s : level=%d\n"), info->name, inforeq);

	/* clear out 'line', to be certain - and to make valgrind happy */
	memset(line, 0, 513);

	snprintf(path, PATH_MAX, "%s/%s-%s", db->path, info->name, info->version);
	if(stat(path, &buf)) {
		/* directory doesn't exist or can't be opened */
		_alpm_log(PM_LOG_DEBUG, "cannot find '%s-%s' in db '%s'\n",
				info->name, info->version, db->treename);
		return(-1);
	}

	/* DESC */
	if(inforeq & INFRQ_DESC) {
		snprintf(path, PATH_MAX, "%s/%s-%s/desc", db->path, info->name, info->version);
		if((fp = fopen(path, "r")) == NULL) {
			_alpm_log(PM_LOG_ERROR, _("could not open file %s: %s\n"), path, strerror(errno));
			goto error;
		}
		while(!feof(fp)) {
			if(fgets(line, 256, fp) == NULL) {
				break;
			}
			_alpm_strtrim(line);
			if(!strcmp(line, "%FILENAME%")) {
				/* filename is _new_ - it provides the real name of the package, on the
				 * server, to allow for us to not tie the name of the actual file to the
				 * data of the package
				 */
				if(fgets(info->filename, sizeof(info->filename), fp) == NULL) {
					goto error;
				}
				_alpm_strtrim(info->filename);
		  } else if(!strcmp(line, "%DESC%")) {
				if(fgets(info->desc, sizeof(info->desc), fp) == NULL) {
					goto error;
				}
				_alpm_strtrim(info->desc);
			} else if(!strcmp(line, "%GROUPS%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->groups = alpm_list_add(info->groups, strdup(line));
				}
			} else if(!strcmp(line, "%URL%")) {
				if(fgets(info->url, sizeof(info->url), fp) == NULL) {
					goto error;
				}
				_alpm_strtrim(info->url);
			} else if(!strcmp(line, "%LICENSE%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->licenses = alpm_list_add(info->licenses, strdup(line));
				}
			} else if(!strcmp(line, "%ARCH%")) {
				if(fgets(info->arch, sizeof(info->arch), fp) == NULL) {
					goto error;
				}
				_alpm_strtrim(info->arch);
			} else if(!strcmp(line, "%BUILDDATE%")) {
				char tmp[32];
				if(fgets(tmp, sizeof(tmp), fp) == NULL) {
					goto error;
				}
				_alpm_strtrim(tmp);

				char first = tolower(tmp[0]);
				if(first > 'a' && first < 'z') {
					struct tm tmp_tm = {0}; //initialize to null incase of failure
					setlocale(LC_TIME, "C");
					strptime(tmp, "%a %b %e %H:%M:%S %Y", &tmp_tm);
					info->builddate = mktime(&tmp_tm);
					setlocale(LC_TIME, "");
				} else {
					info->builddate = atol(tmp);
				}
			} else if(!strcmp(line, "%INSTALLDATE%")) {
				char tmp[32];
				if(fgets(tmp, sizeof(tmp), fp) == NULL) {
					goto error;
				}
				_alpm_strtrim(tmp);

				char first = tolower(tmp[0]);
				if(first > 'a' && first < 'z') {
					struct tm tmp_tm = {0}; //initialize to null incase of failure
					setlocale(LC_TIME, "C");
					strptime(tmp, "%a %b %e %H:%M:%S %Y", &tmp_tm);
					info->installdate = mktime(&tmp_tm);
					setlocale(LC_TIME, "");
				} else {
					info->installdate = atol(tmp);
				}
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
				/* also store this value to isize if isize is unset */
				if(info->isize == 0) {
					info->isize = atol(tmp);
				}
			} else if(!strcmp(line, "%ISIZE%")) {
				/* ISIZE (installed size) tag only appears in sync repositories,
				 * not the local one. */
				char tmp[32];
				if(fgets(tmp, sizeof(tmp), fp) == NULL) {
					goto error;
				}
				_alpm_strtrim(tmp);
				info->isize = atol(tmp);
			} else if(!strcmp(line, "%MD5SUM%")) {
				/* MD5SUM tag only appears in sync repositories,
				 * not the local one. */
				if(fgets(info->md5sum, sizeof(info->md5sum), fp) == NULL) {
					goto error;
				}
			} else if(!strcmp(line, "%REPLACES%")) {
				/* the REPLACES tag is special -- it only appears in sync repositories,
				 * not the local one. */
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->replaces = alpm_list_add(info->replaces, strdup(line));
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
		snprintf(path, PATH_MAX, "%s/%s-%s/files", db->path, info->name, info->version);
		if((fp = fopen(path, "r")) == NULL) {
			_alpm_log(PM_LOG_ERROR, _("could not open file %s: %s\n"), path, strerror(errno));
			goto error;
		}
		while(fgets(line, 256, fp)) {
			_alpm_strtrim(line);
			if(!strcmp(line, "%FILES%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->files = alpm_list_add(info->files, strdup(line));
				}
			} else if(!strcmp(line, "%BACKUP%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->backup = alpm_list_add(info->backup, strdup(line));
				}
			}
		}
		fclose(fp);
		fp = NULL;
	}

	/* DEPENDS */
	if(inforeq & INFRQ_DEPENDS) {
		snprintf(path, PATH_MAX, "%s/%s-%s/depends", db->path, info->name, info->version);
		if((fp = fopen(path, "r")) == NULL) {
			_alpm_log(PM_LOG_ERROR, _("could not open file %s: %s\n"), path, strerror(errno));
			goto error;
		}
		while(!feof(fp)) {
			fgets(line, 255, fp);
			_alpm_strtrim(line);
			if(!strcmp(line, "%DEPENDS%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->depends = alpm_list_add(info->depends, strdup(line));
				}
			} else if(!strcmp(line, "%OPTDEPENDS%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->optdepends = alpm_list_add(info->optdepends, strdup(line));
				}
			} else if(!strcmp(line, "%REQUIREDBY%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->requiredby = alpm_list_add(info->requiredby, strdup(line));
				}
			} else if(!strcmp(line, "%CONFLICTS%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->conflicts = alpm_list_add(info->conflicts, strdup(line));
				}
			} else if(!strcmp(line, "%PROVIDES%")) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->provides = alpm_list_add(info->provides, strdup(line));
				}
			}
			/* TODO: we were going to move these things here, but it should wait.
			 * A better change would be to figure out how to restructure the DB. */
				/* else if(!strcmp(line, "%REPLACES%")) {
				 * the REPLACES tag is special -- it only appears in sync repositories,
				 * not the local one. *
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					info->replaces = alpm_list_add(info->replaces, strdup(line));
				} 
			} else if(!strcmp(line, "%FORCE%")) {
				 * FORCE tag only appears in sync repositories,
				 * not the local one. *
				info->force = 1;
			} */
		}
		fclose(fp);
		fp = NULL;
	}

	/* DELTAS */
	if(inforeq & INFRQ_DELTAS) {
		snprintf(path, PATH_MAX, "%s/%s-%s/deltas", db->path, info->name, info->version);
		if((fp = fopen(path, "r"))) {
			while(!feof(fp)) {
				fgets(line, 255, fp);
				_alpm_strtrim(line);
				if(!strcmp(line, "%DELTAS%")) {
					while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
						info->deltas = alpm_list_add(info->deltas, _alpm_delta_parse(line));
					}
				}
			}
			fclose(fp);
			fp = NULL;
		}
	}

	/* INSTALL */
	if(inforeq & INFRQ_SCRIPTLET) {
		snprintf(path, PATH_MAX, "%s/%s-%s/install", db->path, info->name, info->version);
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

int _alpm_db_write(pmdb_t *db, pmpkg_t *info, pmdbinfrq_t inforeq)
{
	FILE *fp = NULL;
	char path[PATH_MAX];
	mode_t oldmask;
	alpm_list_t *lp = NULL;
	int retval = 0;
	int local = 0;

	ALPM_LOG_FUNC;

	if(db == NULL || info == NULL) {
		return(-1);
	}

	snprintf(path, PATH_MAX, "%s/%s-%s", db->path, info->name, info->version);
	oldmask = umask(0000);
	mkdir(path, 0755);
	/* make sure we have a sane umask */
	umask(0022);

	if(strcmp(db->treename, "local") == 0) {
		local = 1;
	}

	/* DESC */
	if(inforeq & INFRQ_DESC) {
		_alpm_log(PM_LOG_DEBUG, "writing %s-%s DESC information back to db\n",
				info->name, info->version);
		snprintf(path, PATH_MAX, "%s/%s-%s/desc", db->path, info->name, info->version);
		if((fp = fopen(path, "w")) == NULL) {
			_alpm_log(PM_LOG_ERROR, _("could not open file %s: %s\n"), path, strerror(errno));
			retval = -1;
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
			if(info->licenses) {
				fputs("%LICENSE%\n", fp);
				for(lp = info->licenses; lp; lp = lp->next) {
					fprintf(fp, "%s\n", (char *)lp->data);
				}
				fprintf(fp, "\n");
			}
			if(info->arch[0]) {
				fprintf(fp, "%%ARCH%%\n"
								"%s\n\n", info->arch);
			}
			if(info->builddate) {
				fprintf(fp, "%%BUILDDATE%%\n"
								"%ju\n\n", (uintmax_t)info->builddate);
			}
			if(info->installdate) {
				fprintf(fp, "%%INSTALLDATE%%\n"
								"%ju\n\n", (uintmax_t)info->installdate);
			}
			if(info->packager[0]) {
				fprintf(fp, "%%PACKAGER%%\n"
								"%s\n\n", info->packager);
			}
			if(info->size) {
				/* only write installed size, csize is irrelevant once installed */
				fprintf(fp, "%%SIZE%%\n"
								"%lu\n\n", info->isize);
			}
			if(info->reason) {
				fprintf(fp, "%%REASON%%\n"
								"%u\n\n", info->reason);
			}
		} else {
			if(info->size) {
				fprintf(fp, "%%CSIZE%%\n"
								"%lu\n\n", info->size);
			}
			if(info->isize) {
				fprintf(fp, "%%ISIZE%%\n"
								"%lu\n\n", info->isize);
			}
			if(info->md5sum) {
				fprintf(fp, "%%MD5SUM%%\n"
								"%s\n\n", info->md5sum);
			}
		}
		fclose(fp);
		fp = NULL;
	}

	/* FILES */
	if(local && (inforeq & INFRQ_FILES)) {
		_alpm_log(PM_LOG_DEBUG, "writing %s-%s FILES information back to db\n",
				info->name, info->version);
		snprintf(path, PATH_MAX, "%s/%s-%s/files", db->path, info->name, info->version);
		if((fp = fopen(path, "w")) == NULL) {
			_alpm_log(PM_LOG_ERROR, _("could not open file %s: %s\n"), path, strerror(errno));
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
		_alpm_log(PM_LOG_DEBUG, "writing %s-%s DEPENDS information back to db\n",
			info->name, info->version);
		snprintf(path, PATH_MAX, "%s/%s-%s/depends", db->path, info->name, info->version);
		if((fp = fopen(path, "w")) == NULL) {
			_alpm_log(PM_LOG_ERROR, _("could not open file %s: %s\n"), path, strerror(errno));
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
		if(info->optdepends) {
			fputs("%OPTDEPENDS%\n", fp);
			for(lp = info->optdepends; lp; lp = lp->next) {
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

	ALPM_LOG_FUNC;

	if(db == NULL || info == NULL) {
		RET_ERR(PM_ERR_DB_NULL, -1);
	}

	snprintf(path, PATH_MAX, "%s%s-%s", db->path, info->name, info->version);
	if(_alpm_rmrf(path) == -1) {
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
int _alpm_db_getlastupdate(const pmdb_t *db, char *ts)
{
	FILE *fp;
	char file[PATH_MAX];

	ALPM_LOG_FUNC;

	if(db == NULL || ts == NULL) {
		return(-1);
	}

	snprintf(file, PATH_MAX, "%s.lastupdate", db->path);

	/* get the last update time, if it's there */
	if((fp = fopen(file, "r")) == NULL) {
		return(-1);
	} else {
		char line[256];
		if(fgets(line, sizeof(line), fp)) {
			strncpy(ts, line, 14); /* YYYYMMDDHHMMSS */
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
int _alpm_db_setlastupdate(const pmdb_t *db, char *ts)
{
	FILE *fp;
	char file[PATH_MAX];

	ALPM_LOG_FUNC;

	if(db == NULL || ts == NULL || strlen(ts) == 0) {
		return(-1);
	}

	snprintf(file, PATH_MAX, "%s.lastupdate", db->path);

	if((fp = fopen(file, "w")) == NULL) {
		return(-1);
	}
	if(fputs(ts, fp) <= 0) {
		fclose(fp);
		return(-1);
	}
	fclose(fp);

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
