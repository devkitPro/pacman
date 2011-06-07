/*
 *  conf.c
 *
 *  Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <errno.h>
#include <glob.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h> /* strdup */
#include <sys/stat.h>
#include <sys/utsname.h> /* uname */
#include <unistd.h>

/* pacman */
#include "conf.h"
#include "util.h"
#include "pacman.h"
#include "callback.h"

/* global config variable */
config_t *config = NULL;

config_t *config_new(void)
{
	config_t *newconfig = calloc(1, sizeof(config_t));
	if(!newconfig) {
			pm_fprintf(stderr, PM_LOG_ERROR,
					_("malloc failure: could not allocate %zd bytes\n"),
					sizeof(config_t));
			return NULL;
	}
	/* defaults which may get overridden later */
	newconfig->op = PM_OP_MAIN;
	newconfig->logmask = PM_LOG_ERROR | PM_LOG_WARNING;
	newconfig->configfile = strdup(CONFFILE);
	newconfig->sigverify = PM_PGP_VERIFY_UNKNOWN;

	return newconfig;
}

int config_free(config_t *oldconfig)
{
	if(oldconfig == NULL) {
		return -1;
	}

	FREELIST(oldconfig->holdpkg);
	FREELIST(oldconfig->syncfirst);
	FREELIST(oldconfig->ignorepkg);
	FREELIST(oldconfig->ignoregrp);
	FREELIST(oldconfig->noupgrade);
	FREELIST(oldconfig->noextract);
	free(oldconfig->configfile);
	free(oldconfig->rootdir);
	free(oldconfig->dbpath);
	free(oldconfig->logfile);
	free(oldconfig->gpgdir);
	FREELIST(oldconfig->cachedirs);
	free(oldconfig->xfercommand);
	free(oldconfig->print_format);
	free(oldconfig->arch);
	free(oldconfig);
	oldconfig = NULL;

	return 0;
}

/** Helper function for download_with_xfercommand() */
static char *get_filename(const char *url) {
	char *filename = strrchr(url, '/');
	if(filename != NULL) {
		filename++;
	}
	return filename;
}

/** Helper function for download_with_xfercommand() */
static char *get_destfile(const char *path, const char *filename) {
	char *destfile;
	/* len = localpath len + filename len + null */
	size_t len = strlen(path) + strlen(filename) + 1;
	destfile = calloc(len, sizeof(char));
	snprintf(destfile, len, "%s%s", path, filename);

	return destfile;
}

/** Helper function for download_with_xfercommand() */
static char *get_tempfile(const char *path, const char *filename) {
	char *tempfile;
	/* len = localpath len + filename len + '.part' len + null */
	size_t len = strlen(path) + strlen(filename) + 6;
	tempfile = calloc(len, sizeof(char));
	snprintf(tempfile, len, "%s%s.part", path, filename);

	return tempfile;
}

/** External fetch callback */
static int download_with_xfercommand(const char *url, const char *localpath,
		int force) {
	int ret = 0;
	int retval;
	int usepart = 0;
	struct stat st;
	char *parsedcmd,*tempcmd;
	char cwd[PATH_MAX];
	int restore_cwd = 0;
	char *destfile, *tempfile, *filename;

	if(!config->xfercommand) {
		return -1;
	}

	filename = get_filename(url);
	if(!filename) {
		return -1;
	}
	destfile = get_destfile(localpath, filename);
	tempfile = get_tempfile(localpath, filename);

	if(force && stat(tempfile, &st) == 0) {
		unlink(tempfile);
	}
	if(force && stat(destfile, &st) == 0) {
		unlink(destfile);
	}

	tempcmd = strdup(config->xfercommand);
	/* replace all occurrences of %o with fn.part */
	if(strstr(tempcmd, "%o")) {
		usepart = 1;
		parsedcmd = strreplace(tempcmd, "%o", tempfile);
		free(tempcmd);
		tempcmd = parsedcmd;
	}
	/* replace all occurrences of %u with the download URL */
	parsedcmd = strreplace(tempcmd, "%u", url);
	free(tempcmd);

	/* save the cwd so we can restore it later */
	if(getcwd(cwd, PATH_MAX) == NULL) {
		pm_printf(PM_LOG_ERROR, _("could not get current working directory\n"));
	} else {
		restore_cwd = 1;
	}

	/* cwd to the download directory */
	if(chdir(localpath)) {
		pm_printf(PM_LOG_WARNING, _("could not chdir to download directory %s\n"), localpath);
		ret = -1;
		goto cleanup;
	}
	/* execute the parsed command via /bin/sh -c */
	pm_printf(PM_LOG_DEBUG, "running command: %s\n", parsedcmd);
	retval = system(parsedcmd);

	if(retval == -1) {
		pm_printf(PM_LOG_WARNING, _("running XferCommand: fork failed!\n"));
		ret = -1;
	} else if(retval != 0) {
		/* download failed */
		pm_printf(PM_LOG_DEBUG, "XferCommand command returned non-zero status "
				"code (%d)\n", retval);
		ret = -1;
	} else {
		/* download was successful */
		if(usepart) {
			rename(tempfile, destfile);
		}
		ret = 0;
	}

cleanup:
	/* restore the old cwd if we have it */
	if(restore_cwd && chdir(cwd) != 0) {
		pm_printf(PM_LOG_ERROR, _("could not change directory to %s (%s)\n"),
				cwd, strerror(errno));
	}

	if(ret == -1) {
		/* hack to let an user the time to cancel a download */
		sleep(2);
	}
	free(destfile);
	free(tempfile);
	free(parsedcmd);

	return ret;
}


int config_set_arch(const char *arch)
{
	if(strcmp(arch, "auto") == 0) {
		struct utsname un;
		uname(&un);
		config->arch = strdup(un.machine);
	} else {
		config->arch = strdup(arch);
	}
	pm_printf(PM_LOG_DEBUG, "config: arch: %s\n", config->arch);
	return 0;
}

static pgp_verify_t option_verifysig(const char *value)
{
	pgp_verify_t level;
	if(strcmp(value, "Always") == 0) {
		level = PM_PGP_VERIFY_ALWAYS;
	} else if(strcmp(value, "Optional") == 0) {
		level = PM_PGP_VERIFY_OPTIONAL;
	} else if(strcmp(value, "Never") == 0) {
		level = PM_PGP_VERIFY_NEVER;
	} else {
		level = PM_PGP_VERIFY_UNKNOWN;
	}
	pm_printf(PM_LOG_DEBUG, "config: VerifySig = %s (%d)\n", value, level);
	return level;
}

static int process_cleanmethods(alpm_list_t *values) {
	alpm_list_t *i;
	for(i = values; i; i = alpm_list_next(i)) {
		const char *value = i->data;
		if(strcmp(value, "KeepInstalled") == 0) {
			config->cleanmethod |= PM_CLEAN_KEEPINST;
		} else if(strcmp(value, "KeepCurrent") == 0) {
			config->cleanmethod |= PM_CLEAN_KEEPCUR;
		} else {
			pm_printf(PM_LOG_ERROR, _("invalid value for 'CleanMethod' : '%s'\n"),
					value);
			return 1;
		}
	}
	return 0;
}

/** Add repeating options such as NoExtract, NoUpgrade, etc to libalpm
 * settings. Refactored out of the parseconfig code since all of them did
 * the exact same thing and duplicated code.
 * @param ptr a pointer to the start of the multiple options
 * @param option the string (friendly) name of the option, used for messages
 * @param list the list to add the option to
 */
static void setrepeatingoption(char *ptr, const char *option,
		alpm_list_t **list)
{
	char *q;

	while((q = strchr(ptr, ' '))) {
		*q = '\0';
		*list = alpm_list_add(*list, strdup(ptr));
		pm_printf(PM_LOG_DEBUG, "config: %s: %s\n", option, ptr);
		ptr = q;
		ptr++;
	}
	*list = alpm_list_add(*list, strdup(ptr));
	pm_printf(PM_LOG_DEBUG, "config: %s: %s\n", option, ptr);
}

static int _parse_options(const char *key, char *value,
		const char *file, int linenum)
{
	if(value == NULL) {
		/* options without settings */
		if(strcmp(key, "UseSyslog") == 0) {
			config->usesyslog = 1;
			pm_printf(PM_LOG_DEBUG, "config: usesyslog\n");
		} else if(strcmp(key, "ILoveCandy") == 0) {
			config->chomp = 1;
			pm_printf(PM_LOG_DEBUG, "config: chomp\n");
		} else if(strcmp(key, "VerbosePkgLists") == 0) {
			config->verbosepkglists = 1;
			pm_printf(PM_LOG_DEBUG, "config: verbosepkglists\n");
		} else if(strcmp(key, "UseDelta") == 0) {
			config->usedelta = 1;
			pm_printf(PM_LOG_DEBUG, "config: usedelta\n");
		} else if(strcmp(key, "TotalDownload") == 0) {
			config->totaldownload = 1;
			pm_printf(PM_LOG_DEBUG, "config: totaldownload\n");
		} else if(strcmp(key, "CheckSpace") == 0) {
			config->checkspace = 1;
		} else {
			pm_printf(PM_LOG_WARNING,
					_("config file %s, line %d: directive '%s' in section '%s' not recognized.\n"),
					file, linenum, key, "options");
		}
	} else {
		/* options with settings */
		if(strcmp(key, "NoUpgrade") == 0) {
			setrepeatingoption(value, "NoUpgrade", &(config->noupgrade));
		} else if(strcmp(key, "NoExtract") == 0) {
			setrepeatingoption(value, "NoExtract", &(config->noextract));
		} else if(strcmp(key, "IgnorePkg") == 0) {
			setrepeatingoption(value, "IgnorePkg", &(config->ignorepkg));
		} else if(strcmp(key, "IgnoreGroup") == 0) {
			setrepeatingoption(value, "IgnoreGroup", &(config->ignoregrp));
		} else if(strcmp(key, "HoldPkg") == 0) {
			setrepeatingoption(value, "HoldPkg", &(config->holdpkg));
		} else if(strcmp(key, "SyncFirst") == 0) {
			setrepeatingoption(value, "SyncFirst", &(config->syncfirst));
		} else if(strcmp(key, "CacheDir") == 0) {
			setrepeatingoption(value, "CacheDir", &(config->cachedirs));
		} else if(strcmp(key, "Architecture") == 0) {
			if(!config->arch) {
				config_set_arch(value);
			}
		} else if(strcmp(key, "DBPath") == 0) {
			/* don't overwrite a path specified on the command line */
			if(!config->dbpath) {
				config->dbpath = strdup(value);
				pm_printf(PM_LOG_DEBUG, "config: dbpath: %s\n", value);
			}
		} else if(strcmp(key, "RootDir") == 0) {
			/* don't overwrite a path specified on the command line */
			if(!config->rootdir) {
				config->rootdir = strdup(value);
				pm_printf(PM_LOG_DEBUG, "config: rootdir: %s\n", value);
			}
		} else if(strcmp(key, "GPGDir") == 0) {
			if(!config->gpgdir) {
				config->gpgdir = strdup(value);
				pm_printf(PM_LOG_DEBUG, "config: gpgdir: %s\n", value);
			}
		} else if(strcmp(key, "LogFile") == 0) {
			if(!config->logfile) {
				config->logfile = strdup(value);
				pm_printf(PM_LOG_DEBUG, "config: logfile: %s\n", value);
			}
		} else if(strcmp(key, "XferCommand") == 0) {
			config->xfercommand = strdup(value);
			pm_printf(PM_LOG_DEBUG, "config: xfercommand: %s\n", value);
		} else if(strcmp(key, "CleanMethod") == 0) {
			alpm_list_t *methods = NULL;
			setrepeatingoption(value, "CleanMethod", &methods);
			if(process_cleanmethods(methods)) {
				FREELIST(methods);
				return 1;
			}
			FREELIST(methods);
		} else if(strcmp(key, "VerifySig") == 0) {
			pgp_verify_t level = option_verifysig(value);
			if(level != PM_PGP_VERIFY_UNKNOWN) {
				config->sigverify = level;
			} else {
				pm_printf(PM_LOG_ERROR,
						_("config file %s, line %d: directive '%s' has invalid value '%s'\n"),
						file, linenum, key, value);
				return 1;
			}
		} else {
			pm_printf(PM_LOG_WARNING,
					_("config file %s, line %d: directive '%s' in section '%s' not recognized.\n"),
					file, linenum, key, "options");
		}

	}
	return 0;
}

static int _add_mirror(pmdb_t *db, char *value)
{
	const char *dbname = alpm_db_get_name(db);
	/* let's attempt a replacement for the current repo */
	char *temp = strreplace(value, "$repo", dbname);
	/* let's attempt a replacement for the arch */
	const char *arch = config->arch;
	char *server;
	if(arch) {
		server = strreplace(temp, "$arch", arch);
		free(temp);
	} else {
		if(strstr(temp, "$arch")) {
			free(temp);
			pm_printf(PM_LOG_ERROR, _("The mirror '%s' contains the $arch"
						" variable, but no Architecture is defined.\n"), value);
			return 1;
		}
		server = temp;
	}

	if(alpm_db_add_server(db, server) != 0) {
		/* pm_errno is set by alpm_db_setserver */
		pm_printf(PM_LOG_ERROR, _("could not add server URL to database '%s': %s (%s)\n"),
				dbname, server, alpm_strerrorlast());
		free(server);
		return 1;
	}

	free(server);
	return 0;
}

/** Sets up libalpm global stuff in one go. Called after the command line
 * and inital config file parsing. Once this is complete, we can see if any
 * paths were defined. If a rootdir was defined and nothing else, we want all
 * of our paths to live under the rootdir that was specified. Safe to call
 * multiple times (will only do anything the first time).
 */
static int setup_libalpm(void)
{
	int ret = 0;
	enum _pmerrno_t err;
	pmhandle_t *handle;

	pm_printf(PM_LOG_DEBUG, "setup_libalpm called\n");

	/* Configure root path first. If it is set and dbpath/logfile were not
	 * set, then set those as well to reside under the root. */
	if(config->rootdir) {
		char path[PATH_MAX];
		if(!config->dbpath) {
			snprintf(path, PATH_MAX, "%s/%s", config->rootdir, DBPATH + 1);
			config->dbpath = strdup(path);
		}
		if(!config->logfile) {
			snprintf(path, PATH_MAX, "%s/%s", config->rootdir, LOGFILE + 1);
			config->logfile = strdup(path);
		}
	} else {
		config->rootdir = strdup(ROOTDIR);
		if(!config->dbpath) {
			config->dbpath = strdup(DBPATH);
		}
	}

	/* initialize library */
	handle = alpm_initialize(config->rootdir, config->dbpath, &err);
	if(!handle) {
		pm_printf(PM_LOG_ERROR, _("failed to initialize alpm library (%s)\n"),
		        alpm_strerror(err));
		return -1;
	}
	config->handle = handle;

	alpm_option_set_logcb(handle, cb_log);
	alpm_option_set_dlcb(handle, cb_dl_progress);

	config->logfile = config->logfile ? config->logfile : strdup(LOGFILE);
	ret = alpm_option_set_logfile(handle, config->logfile);
	if(ret != 0) {
		pm_printf(PM_LOG_ERROR, _("problem setting logfile '%s' (%s)\n"),
				config->logfile, alpm_strerrorlast());
		return ret;
	}

	/* Set GnuPG's home directory.  This is not relative to rootdir, even if
	 * rootdir is defined. Reasoning: gpgdir contains configuration data. */
	config->gpgdir = config->gpgdir ? config->gpgdir : strdup(GPGDIR);
	ret = alpm_option_set_signaturedir(handle, config->gpgdir);
	if(ret != 0) {
		pm_printf(PM_LOG_ERROR, _("problem setting gpgdir '%s' (%s)\n"),
				config->gpgdir, alpm_strerrorlast());
		return ret;
	}

	/* add a default cachedir if one wasn't specified */
	if(config->cachedirs == NULL) {
		alpm_option_add_cachedir(handle, CACHEDIR);
	} else {
		alpm_option_set_cachedirs(handle, config->cachedirs);
	}

	if(config->sigverify != PM_PGP_VERIFY_UNKNOWN) {
		alpm_option_set_default_sigverify(handle, config->sigverify);
	}

	if(config->xfercommand) {
		alpm_option_set_fetchcb(handle, download_with_xfercommand);
	}

	if(config->totaldownload) {
		alpm_option_set_totaldlcb(handle, cb_dl_total);
	}

	alpm_option_set_arch(handle, config->arch);
	alpm_option_set_checkspace(handle, config->checkspace);
	alpm_option_set_usesyslog(handle, config->usesyslog);
	alpm_option_set_usedelta(handle, config->usedelta);
	alpm_option_set_default_sigverify(handle, config->sigverify);

	alpm_option_set_ignorepkgs(handle, config->ignorepkg);
	alpm_option_set_ignoregrps(handle, config->ignoregrp);
	alpm_option_set_noupgrades(handle, config->noupgrade);
	alpm_option_set_noextracts(handle, config->noextract);

	return 0;
}


/* The real parseconfig. Called with a null section argument by the publicly
 * visible parseconfig so we can recall from within ourself on an include */
static int _parseconfig(const char *file, int parse_options,
		char **section, pmdb_t *db)
{
	FILE *fp = NULL;
	char line[PATH_MAX];
	int linenum = 0;
	char *ptr;
	int ret = 0;

	pm_printf(PM_LOG_DEBUG, "config: attempting to read file %s\n", file);
	fp = fopen(file, "r");
	if(fp == NULL) {
		pm_printf(PM_LOG_ERROR, _("config file %s could not be read.\n"), file);
		return 1;
	}

	while(fgets(line, PATH_MAX, fp)) {
		char *key, *value;

		linenum++;
		strtrim(line);

		/* ignore whole line and end of line comments */
		if(strlen(line) == 0 || line[0] == '#') {
			continue;
		}
		if((ptr = strchr(line, '#'))) {
			*ptr = '\0';
		}

		/* sanity check */
		if(parse_options && db) {
			pm_printf(PM_LOG_ERROR, _("config file %s, line %d: parsing options but have a database.\n"),
						file, linenum);
			ret = 1;
			goto cleanup;
		}

		if(line[0] == '[' && line[strlen(line)-1] == ']') {
			char *name;
			/* new config section, skip the '[' */
			ptr = line;
			ptr++;
			name = strdup(ptr);
			name[strlen(name)-1] = '\0';
			if(!strlen(name)) {
				pm_printf(PM_LOG_ERROR, _("config file %s, line %d: bad section name.\n"),
						file, linenum);
				ret = 1;
				goto cleanup;
			}
			pm_printf(PM_LOG_DEBUG, "config: new section '%s'\n", name);
			/* if we are not looking at the options section, register a db */
			if(!parse_options && strcmp(name, "options") != 0) {
				db = alpm_db_register_sync(config->handle, name);
				if(db == NULL) {
					pm_printf(PM_LOG_ERROR, _("could not register '%s' database (%s)\n"),
							name, alpm_strerrorlast());
					ret = 1;
					goto cleanup;
				}
			}
			if(*section) {
				free(*section);
			}
			*section = name;
			continue;
		}

		/* directive */
		/* strsep modifies the 'line' string: 'key \0 value' */
		key = line;
		value = line;
		strsep(&value, "=");
		strtrim(key);
		strtrim(value);

		if(key == NULL) {
			pm_printf(PM_LOG_ERROR, _("config file %s, line %d: syntax error in config file- missing key.\n"),
					file, linenum);
			ret = 1;
			goto cleanup;
		}
		/* For each directive, compare to the camelcase string. */
		if(*section == NULL) {
			pm_printf(PM_LOG_ERROR, _("config file %s, line %d: All directives must belong to a section.\n"),
					file, linenum);
			ret = 1;
			goto cleanup;
		}
		/* Include is allowed in both options and repo sections */
		if(strcmp(key, "Include") == 0) {
			glob_t globbuf;
			int globret;
			size_t gindex;

			if(value == NULL) {
				pm_printf(PM_LOG_ERROR, _("config file %s, line %d: directive '%s' needs a value\n"),
						file, linenum, key);
				ret = 1;
				goto cleanup;
			}
			/* Ignore include failures... assume non-critical */
			globret = glob(value, GLOB_NOCHECK, NULL, &globbuf);
			switch(globret) {
				case GLOB_NOSPACE:
					pm_printf(PM_LOG_DEBUG,
							"config file %s, line %d: include globbing out of space\n",
							file, linenum);
				break;
				case GLOB_ABORTED:
					pm_printf(PM_LOG_DEBUG,
							"config file %s, line %d: include globbing read error for %s\n",
							file, linenum, value);
				break;
				case GLOB_NOMATCH:
					pm_printf(PM_LOG_DEBUG,
							"config file %s, line %d: no include found for %s\n",
							file, linenum, value);
				break;
				default:
					for(gindex = 0; gindex < globbuf.gl_pathc; gindex++) {
						pm_printf(PM_LOG_DEBUG, "config file %s, line %d: including %s\n",
								file, linenum, globbuf.gl_pathv[gindex]);
						_parseconfig(globbuf.gl_pathv[gindex], parse_options, section, db);
					}
				break;
			}
			globfree(&globbuf);
			continue;
		}
		if(parse_options && strcmp(*section, "options") == 0) {
			/* we are either in options ... */
			if((ret = _parse_options(key, value, file, linenum)) != 0) {
				goto cleanup;
			}
		} else if (!parse_options && strcmp(*section, "options") != 0) {
			/* ... or in a repo section */
			if(strcmp(key, "Server") == 0) {
				if(value == NULL) {
					pm_printf(PM_LOG_ERROR, _("config file %s, line %d: directive '%s' needs a value\n"),
							file, linenum, key);
					ret = 1;
					goto cleanup;
				}
				if(_add_mirror(db, value) != 0) {
					ret = 1;
					goto cleanup;
				}
			} else if(strcmp(key, "VerifySig") == 0) {
				pgp_verify_t level = option_verifysig(value);
				if(level != PM_PGP_VERIFY_UNKNOWN) {
					ret = alpm_db_set_pgp_verify(db, level);
					if(ret != 0) {
						pm_printf(PM_LOG_ERROR, _("could not add set verify option for database '%s': %s (%s)\n"),
								alpm_db_get_name(db), value, alpm_strerrorlast());
						goto cleanup;
					}
				} else {
					pm_printf(PM_LOG_ERROR,
							_("config file %s, line %d: directive '%s' has invalid value '%s'\n"),
							file, linenum, key, value);
					ret = 1;
					goto cleanup;
				}
			} else {
				pm_printf(PM_LOG_WARNING,
						_("config file %s, line %d: directive '%s' in section '%s' not recognized.\n"),
						file, linenum, key, *section);
			}
		}
	}

cleanup:
	fclose(fp);
	pm_printf(PM_LOG_DEBUG, "config: finished parsing %s\n", file);
	return ret;
}

/** Parse a configuration file.
 * @param file path to the config file.
 * @return 0 on success, non-zero on error
 */
int parseconfig(const char *file)
{
	int ret;
	char *section = NULL;
	/* the config parse is a two-pass affair. We first parse the entire thing for
	 * the [options] section so we can get all default and path options set.
	 * Next, we go back and parse everything but [options]. */

	/* call the real parseconfig function with a null section & db argument */
	pm_printf(PM_LOG_DEBUG, "parseconfig: options pass\n");
	if((ret = _parseconfig(file, 1, &section, NULL))) {
		free(section);
		return ret;
	}
	free(section);
	if((ret = setup_libalpm())) {
		return ret;
	}
	/* second pass, repo section parsing */
	section = NULL;
	pm_printf(PM_LOG_DEBUG, "parseconfig: repo pass\n");
	return _parseconfig(file, 0, &section, NULL);
	free(section);
}

/* vim: set ts=2 sw=2 noet: */
