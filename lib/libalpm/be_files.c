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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
#include <locale.h> /* setlocale */

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
#include "deps.h"


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

static int splitname(const char *target, pmpkg_t *pkg)
{
	/* the format of a db entry is as follows:
	 *    package-version-rel/
	 * package name can contain hyphens, so parse from the back- go back
	 * two hyphens and we have split the version from the name.
	 */
	char *tmp, *p, *q;

	if(target == NULL || pkg == NULL) {
		return(-1);
	}
	STRDUP(tmp, target, RET_ERR(PM_ERR_MEMORY, -1));
	p = tmp + strlen(tmp);

	/* do the magic parsing- find the beginning of the version string
	 * by doing two iterations of same loop to lop off two hyphens */
	for(q = --p; *q && *q != '-'; q--);
	for(p = --q; *p && *p != '-'; p--);
	if(*p != '-' || p == tmp) {
		return(-1);
	}

	/* copy into fields and return */
	if(pkg->version) {
		FREE(pkg->version);
	}
	STRDUP(pkg->version, p+1, RET_ERR(PM_ERR_MEMORY, -1));
	/* insert a terminator at the end of the name (on hyphen)- then copy it */
	*p = '\0';
	if(pkg->name) {
		FREE(pkg->name);
	}
	STRDUP(pkg->name, tmp, RET_ERR(PM_ERR_MEMORY, -1));

	free(tmp);
	return(0);
}

pmpkg_t *_alpm_db_scan(pmdb_t *db, const char *target)
{
	struct dirent *ent = NULL;
	struct stat sbuf;
	char path[PATH_MAX];
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
				char *name;

				if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
					continue;
				}
				/* stat the entry, make sure it's a directory */
				snprintf(path, PATH_MAX, "%s/%s", db->path, ent->d_name);
				if(stat(path, &sbuf) || !S_ISDIR(sbuf.st_mode)) {
					continue;
				}

				STRDUP(name, ent->d_name, return(NULL));

				/* truncate the string at the second-to-last hyphen, */
				/* which will give us the package name */
				if((ptr = rindex(name, '-'))) {
					*ptr = '\0';
				}
				if((ptr = rindex(name, '-'))) {
					*ptr = '\0';
				}
				if(strcmp(name, target) == 0) {
					found = 1;
				}
				FREE(name);
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
				if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
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
		if(splitname(ent->d_name, pkg) != 0) {
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

	if(info == NULL || info->name == NULL || info->version == NULL) {
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
	_alpm_log(PM_LOG_FUNCTION, "loading package data for %s : level=0x%x\n",
			info->name, inforeq);

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
			if(strcmp(line, "%FILENAME%") == 0) {
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				STRDUP(info->filename, _alpm_strtrim(line), goto error);
		  } else if(strcmp(line, "%DESC%") == 0) {
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				STRDUP(info->desc, _alpm_strtrim(line), goto error);
			} else if(strcmp(line, "%GROUPS%") == 0) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alpm_strtrim(line), goto error);
					info->groups = alpm_list_add(info->groups, linedup);
				}
			} else if(strcmp(line, "%URL%") == 0) {
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				STRDUP(info->url, _alpm_strtrim(line), goto error);
			} else if(strcmp(line, "%LICENSE%") == 0) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alpm_strtrim(line), goto error);
					info->licenses = alpm_list_add(info->licenses, linedup);
				}
			} else if(strcmp(line, "%ARCH%") == 0) {
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				STRDUP(info->arch, _alpm_strtrim(line), goto error);
			} else if(strcmp(line, "%BUILDDATE%") == 0) {
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				_alpm_strtrim(line);

				char first = tolower(line[0]);
				if(first > 'a' && first < 'z') {
					struct tm tmp_tm = {0}; //initialize to null incase of failure
					setlocale(LC_TIME, "C");
					strptime(line, "%a %b %e %H:%M:%S %Y", &tmp_tm);
					info->builddate = mktime(&tmp_tm);
					setlocale(LC_TIME, "");
				} else {
					info->builddate = atol(line);
				}
			} else if(strcmp(line, "%INSTALLDATE%") == 0) {
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				_alpm_strtrim(line);

				char first = tolower(line[0]);
				if(first > 'a' && first < 'z') {
					struct tm tmp_tm = {0}; //initialize to null incase of failure
					setlocale(LC_TIME, "C");
					strptime(line, "%a %b %e %H:%M:%S %Y", &tmp_tm);
					info->installdate = mktime(&tmp_tm);
					setlocale(LC_TIME, "");
				} else {
					info->installdate = atol(line);
				}
			} else if(strcmp(line, "%PACKAGER%") == 0) {
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				STRDUP(info->packager, _alpm_strtrim(line), goto error);
			} else if(strcmp(line, "%REASON%") == 0) {
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				info->reason = atol(_alpm_strtrim(line));
			} else if(strcmp(line, "%SIZE%") == 0 || strcmp(line, "%CSIZE%") == 0) {
				/* NOTE: the CSIZE and SIZE fields both share the "size" field
				 *       in the pkginfo_t struct.  This can be done b/c CSIZE
				 *       is currently only used in sync databases, and SIZE is
				 *       only used in local databases.
				 */
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				info->size = atol(_alpm_strtrim(line));
				/* also store this value to isize if isize is unset */
				if(info->isize == 0) {
					info->isize = info->size;
				}
			} else if(strcmp(line, "%ISIZE%") == 0) {
				/* ISIZE (installed size) tag only appears in sync repositories,
				 * not the local one. */
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				info->isize = atol(_alpm_strtrim(line));
			} else if(strcmp(line, "%MD5SUM%") == 0) {
				/* MD5SUM tag only appears in sync repositories,
				 * not the local one. */
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				STRDUP(info->md5sum, _alpm_strtrim(line), goto error);
			} else if(strcmp(line, "%REPLACES%") == 0) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alpm_strtrim(line), goto error);
					info->replaces = alpm_list_add(info->replaces, linedup);
				}
			} else if(strcmp(line, "%FORCE%") == 0) {
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
			if(strcmp(line, "%FILES%") == 0) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alpm_strtrim(line), goto error);
					info->files = alpm_list_add(info->files, linedup);
				}
			} else if(strcmp(line, "%BACKUP%") == 0) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alpm_strtrim(line), goto error);
					info->backup = alpm_list_add(info->backup, linedup);
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
			if(strcmp(line, "%DEPENDS%") == 0) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					pmdepend_t *dep = alpm_splitdep(_alpm_strtrim(line));
					info->depends = alpm_list_add(info->depends, dep);
				}
			} else if(strcmp(line, "%OPTDEPENDS%") == 0) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alpm_strtrim(line), goto error);
					info->optdepends = alpm_list_add(info->optdepends, linedup);
				}
			} else if(strcmp(line, "%CONFLICTS%") == 0) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alpm_strtrim(line), goto error);
					info->conflicts = alpm_list_add(info->conflicts, linedup);
				}
			} else if(strcmp(line, "%PROVIDES%") == 0) {
				while(fgets(line, 512, fp) && strlen(_alpm_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alpm_strtrim(line), goto error);
					info->provides = alpm_list_add(info->provides, linedup);
				}
			}
		}
		fclose(fp);
		fp = NULL;
	}

	/* DELTAS */
	if(inforeq & INFRQ_DELTAS) {
		snprintf(path, PATH_MAX, "%s/%s-%s/deltas", db->path,
				info->name, info->version);
		if((fp = fopen(path, "r"))) {
			while(!feof(fp)) {
				fgets(line, 255, fp);
				_alpm_strtrim(line);
				if(strcmp(line, "%DELTAS%") == 0) {
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
		if(info->desc) {
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
			if(info->url) {
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
			if(info->arch) {
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
			if(info->packager) {
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
				char *depstring = alpm_dep_get_string(lp->data);
				fprintf(fp, "%s\n", depstring);
				free(depstring);
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
		if(info->replaces) {
			fputs("%REPLACES%\n", fp);
			for(lp = info->replaces; lp; lp = lp->next) {
				fprintf(fp, "%s\n", (char *)lp->data);
			}
			fprintf(fp, "\n");
		}
		if(info->force) {
			/* note the extra newline character, which is necessary! */
			fprintf(fp, "%%FORCE%%\n\n");
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

/*
 * Return the last update time as number of seconds from the epoch.
 * Returns 0 if the value is unknown or can't be read.
 */
time_t _alpm_db_getlastupdate(const pmdb_t *db)
{
	FILE *fp;
	char file[PATH_MAX];
	time_t ret = 0;

	ALPM_LOG_FUNC;

	if(db == NULL) {
		return(ret);
	}

	snprintf(file, PATH_MAX, "%s.lastupdate", db->path);

	/* get the last update time, if it's there */
	if((fp = fopen(file, "r")) == NULL) {
		return(ret);
	} else {
		char line[64];
		if(fgets(line, sizeof(line), fp)) {
			ret = atol(line);
		}
	}
	fclose(fp);
	return(ret);
}

/*
 * writes the dbpath/.lastupdate file with the value in time
 */
int _alpm_db_setlastupdate(const pmdb_t *db, time_t time)
{
	FILE *fp;
	char file[PATH_MAX];
	int ret = 0;

	ALPM_LOG_FUNC;

	if(db == NULL || time == 0) {
		return(-1);
	}

	snprintf(file, PATH_MAX, "%s.lastupdate", db->path);

	if((fp = fopen(file, "w")) == NULL) {
		return(-1);
	}
	if(fprintf(fp, "%ju", (uintmax_t)time) <= 0) {
		ret = -1;
	}
	fclose(fp);

	return(ret);
}

/* vim: set ts=2 sw=2 noet: */
