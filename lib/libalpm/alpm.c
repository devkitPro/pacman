/*
 *  alpm.c
 * 
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2005 by Christian Hamar <krics@linuxforum.hu>
 *  Copyright (c) 2005, 2006 by Miklos Vajna <vmiklos@frugalware.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h> /* PATH_MAX */
#include <stdarg.h>

/* libalpm */
#include "alpm.h"
#include "alpm_list.h"
#include "error.h"
#include "handle.h"
#include "util.h"

#define min(X, Y)  ((X) < (Y) ? (X) : (Y))

/* Globals */
pmhandle_t *handle = NULL;
enum _pmerrno_t pm_errno SYMEXPORT;

/** \addtogroup alpm_interface Interface Functions
 * @brief Functions to initialize and release libalpm
 * @{
 */

/** Initializes the library.  This must be called before any other
 * functions are called.
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_initialize()
{
	ASSERT(handle == NULL, RET_ERR(PM_ERR_HANDLE_NOT_NULL, -1));

	handle = _alpm_handle_new();
	if(handle == NULL) {
		RET_ERR(PM_ERR_MEMORY, -1);
	}

	return(0);
}

/** Release the library.  This should be the last alpm call you make.
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_release()
{
	int dbs_left = 0;

	ALPM_LOG_FUNC;

	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));

	/* close local database */
	if(handle->db_local) {
		alpm_db_unregister(handle->db_local);
		handle->db_local = NULL;
	}
	/* and also sync ones */
	while((dbs_left = alpm_list_count(handle->dbs_sync)) > 0) {
		pmdb_t *db = (pmdb_t *)handle->dbs_sync->data;
		_alpm_log(PM_LOG_DEBUG, _("removing DB %s, %d remaining..."), db->treename, dbs_left);
		alpm_db_unregister(db);
		db = NULL;
	}

	_alpm_handle_free(handle);

	return(0);
}

/** @} */

/** @defgroup alpm_misc Miscellaneous Functions
 * @brief Various libalpm functions
 */

/** Parses a configuration file.
 * @param file path to the config file.
 * @param callback a function to be called upon new database creation
 * @param this_section the config current section being parsed
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 * @addtogroup alpm_misc
 */
int SYMEXPORT alpm_parse_config(char *file, alpm_cb_db_register callback, const char *this_section)
{
	FILE *fp = NULL;
	char line[PATH_MAX+1];
	char *ptr = NULL;
	char *key = NULL;
	int linenum = 0;
	char origkey[256];
	char section[256] = "";
	pmdb_t *db = NULL;

	ALPM_LOG_FUNC;

	fp = fopen(file, "r");
	if(fp == NULL) {
		return(0);
	}

	if(this_section != NULL && strlen(this_section) > 0) {
		strncpy(section, this_section, min(255, strlen(this_section)));
		if(!strcmp(section, "local")) {
			RET_ERR(PM_ERR_CONF_LOCAL, -1);
		}
		if(strcmp(section, "options")) {
			db = _alpm_db_register(section, callback);
		}
	}

	while(fgets(line, PATH_MAX, fp)) {
		linenum++;
		_alpm_strtrim(line);
		if(strlen(line) == 0 || line[0] == '#') {
			continue;
		}
		if((ptr = strchr(line, '#'))) {
			*ptr = '\0';
		}
		if(line[0] == '[' && line[strlen(line)-1] == ']') {
			/* new config section */
			ptr = line;
			ptr++;
			strncpy(section, ptr, min(255, strlen(ptr)-1));
			section[min(255, strlen(ptr)-1)] = '\0';
			_alpm_log(PM_LOG_DEBUG, _("config: new section '%s'"), section);
			if(!strlen(section)) {
				RET_ERR(PM_ERR_CONF_BAD_SECTION, -1);
			}
			if(!strcmp(section, "local")) {
				RET_ERR(PM_ERR_CONF_LOCAL, -1);
			}
			if(strcmp(section, "options")) {
				db = _alpm_db_register(section, callback);
				if(db == NULL) {
					/* pm_errno is set by alpm_db_register */
					return(-1);
				}
			}
		} else {
			/* directive */
			ptr = line;
			key = strsep(&ptr, "=");
			if(key == NULL) {
				RET_ERR(PM_ERR_CONF_BAD_SYNTAX, -1);
			}
			_alpm_strtrim(key);
			strncpy(origkey, key, min(255, strlen(key)));
			key = _alpm_strtoupper(key);
			if(!strlen(section) && strcmp(key, "INCLUDE")) {
				RET_ERR(PM_ERR_CONF_DIRECTIVE_OUTSIDE_SECTION, -1);
			}
			if(ptr == NULL) {
				if(strcmp(origkey, "NoPassiveFTP") == 0 || strcmp(key, "NOPASSIVEFTP") == 0) {
					alpm_option_set_nopassiveftp(1);
					_alpm_log(PM_LOG_DEBUG, _("config: nopassiveftp"));
				} else if(strcmp(origkey, "UseSyslog") == 0 || strcmp(key, "USESYSLOG") == 0) {
					alpm_option_set_usesyslog(1);
					_alpm_log(PM_LOG_DEBUG, _("config: usesyslog"));
				} else if(strcmp(origkey, "ILoveCandy") == 0 || strcmp(key, "ILOVECANDY") == 0) {
					alpm_option_set_chomp(1);
					_alpm_log(PM_LOG_DEBUG, _("config: chomp"));
				} else if(strcmp(origkey, "UseColor") == 0 || strcmp(key, "USECOLOR") == 0) {
					alpm_option_set_usecolor(1);
					_alpm_log(PM_LOG_DEBUG, _("config: usecolor"));
				} else if(strcmp(origkey, "ShowSize") == 0 || strcmp(key, "SHOWSIZE") == 0) {
					alpm_option_set_showsize(1);
					_alpm_log(PM_LOG_DEBUG, _("config: showsize"));
				} else {
					RET_ERR(PM_ERR_CONF_BAD_SYNTAX, -1);
				}
			} else {
				_alpm_strtrim(ptr);
				if(strcmp(origkey, "Include") == 0 || strcmp(key, "INCLUDE") == 0) {
					char conf[PATH_MAX];
					strncpy(conf, ptr, PATH_MAX);
					_alpm_log(PM_LOG_DEBUG, _("config: including %s"), conf);
					alpm_parse_config(conf, callback, section);
				} else if(strcmp(section, "options") == 0) {
					if(strcmp(origkey, "NoUpgrade") == 0 || strcmp(key, "NOUPGRADE") == 0) {
						char *p = ptr;
						char *q;

						while((q = strchr(p, ' '))) {
							*q = '\0';
							alpm_option_add_noupgrade(p);
							_alpm_log(PM_LOG_DEBUG, _("config: noupgrade: %s"), p);
							p = q;
							p++;
						}
						alpm_option_add_noupgrade(p);
						_alpm_log(PM_LOG_DEBUG, _("config: noupgrade: %s"), p);
					} else if(strcmp(origkey, "NoExtract") == 0 || strcmp(key, "NOEXTRACT") == 0) {
						char *p = ptr;
						char *q;

						while((q = strchr(p, ' '))) {
							*q = '\0';
							alpm_option_add_noextract(p);
							_alpm_log(PM_LOG_DEBUG, _("config: noextract: %s"), p);
							p = q;
							p++;
						}
						alpm_option_add_noextract(p);
						_alpm_log(PM_LOG_DEBUG, _("config: noextract: %s"), p);
					} else if(strcmp(origkey, "IgnorePkg") == 0 || strcmp(key, "IGNOREPKG") == 0) {
						char *p = ptr;
						char *q;

						while((q = strchr(p, ' '))) {
							*q = '\0';
							alpm_option_add_ignorepkg(p);
							_alpm_log(PM_LOG_DEBUG, _("config: ignorepkg: %s"), p);
							p = q;
							p++;
						}
						alpm_option_add_ignorepkg(p);
						_alpm_log(PM_LOG_DEBUG, _("config: ignorepkg: %s"), p);
					} else if(strcmp(origkey, "HoldPkg") == 0 || strcmp(key, "HOLDPKG") == 0) {
						char *p = ptr;
						char *q;

						while((q = strchr(p, ' '))) {
							*q = '\0';
							alpm_option_add_holdpkg(p);
							_alpm_log(PM_LOG_DEBUG, _("config: holdpkg: %s"), p);
							p = q;
							p++;
						}
						alpm_option_add_holdpkg(p);
						_alpm_log(PM_LOG_DEBUG, _("config: holdpkg: %s"), p);
					} else if(strcmp(origkey, "DBPath") == 0 || strcmp(key, "DBPATH") == 0) {
						alpm_option_set_dbpath(ptr);
						_alpm_log(PM_LOG_DEBUG, _("config: dbpath: %s"), ptr);
					} else if(strcmp(origkey, "CacheDir") == 0 || strcmp(key, "CACHEDIR") == 0) {
						alpm_option_set_cachedir(ptr);
						_alpm_log(PM_LOG_DEBUG, _("config: cachedir: %s"), ptr);
					} else if(strcmp(origkey, "RootDir") == 0 || strcmp(key, "ROOTDIR") == 0) {
						alpm_option_set_root(ptr);
						_alpm_log(PM_LOG_DEBUG, _("config: rootdir: %s"), ptr);
					} else if (strcmp(origkey, "LogFile") == 0 || strcmp(key, "LOGFILE") == 0) {
						alpm_option_set_logfile(ptr);
						_alpm_log(PM_LOG_DEBUG, _("config: logfile: %s"), ptr);
					} else if (strcmp(origkey, "LockFile") == 0 || strcmp(key, "LOCKFILE") == 0) {
						alpm_option_set_lockfile(ptr);
						_alpm_log(PM_LOG_DEBUG, _("config: lockfile: %s"), ptr);
					} else if (strcmp(origkey, "XferCommand") == 0 || strcmp(key, "XFERCOMMAND") == 0) {
						alpm_option_set_xfercommand(ptr);
						_alpm_log(PM_LOG_DEBUG, _("config: xfercommand: %s"), ptr);
					} else if (strcmp(origkey, "UpgradeDelay") == 0 || strcmp(key, "UPGRADEDELAY") == 0) {
						/* The config value is in days, we use seconds */
						time_t ud = atol(ptr) * 60 * 60 *24;
						alpm_option_set_upgradedelay(ud);
						_alpm_log(PM_LOG_DEBUG, _("config: upgradedelay: %d"), ud);
					} else {
						RET_ERR(PM_ERR_CONF_BAD_SYNTAX, -1);
					}
				} else {
					if(strcmp(origkey, "Server") == 0 || strcmp(key, "SERVER") == 0) {
						/* let's attempt a replacement for the current repo */
						char *server = _alpm_strreplace(ptr, "$repo", section);

						if(alpm_db_setserver(db, server) != 0) {
							/* pm_errno is set by alpm_db_setserver */
							return(-1);
						}

						free(server);
					} else {
						RET_ERR(PM_ERR_CONF_BAD_SYNTAX, -1);
					}
				}
				line[0] = '\0';
			}
		}
	}
	fclose(fp);

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
