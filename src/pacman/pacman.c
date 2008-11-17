/*
 *  pacman.c
 *
 *  Copyright (c) 2002-2008 by Judd Vinet <jvinet@zeroflux.org>
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

/* special handling of package version for GIT */
#if defined(GIT_VERSION)
#undef PACKAGE_VERSION
#define PACKAGE_VERSION GIT_VERSION
#endif

#include <stdlib.h> /* atoi */
#include <stdio.h>
#include <limits.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/utsname.h> /* uname */
#include <locale.h> /* setlocale */
#include <time.h> /* time_t */
#include <errno.h>
#if defined(PACMAN_DEBUG) && defined(HAVE_MCHECK_H)
#include <mcheck.h> /* debug tracing (mtrace) */
#endif

/* alpm */
#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "pacman.h"
#include "util.h"
#include "callback.h"
#include "conf.h"
#include "package.h"

pmdb_t *db_local;
/* list of targets specified on command line */
static alpm_list_t *pm_targets;

/** Display usage/syntax for the specified operation.
 * @param op     the operation code requested
 * @param myname basename(argv[0])
 */
static void usage(int op, const char * const myname)
{
	/* prefetch some strings for usage below, which moves a lot of calls
	 * out of gettext. */
	char const * const str_opt = _("options");
	char const * const str_file = _("file");
	char const * const str_pkg = _("package");
	char const * const str_usg = _("usage");
	char const * const str_opr = _("operation");

	if(op == PM_OP_MAIN) {
		printf("%s:  %s <%s> [...]\n", str_usg, myname, str_opr);
		printf(_("operations:\n"));
		printf("    %s {-h --help}\n", myname);
		printf("    %s {-V --version}\n", myname);
		printf("    %s {-Q --query}   [%s] [%s]\n", myname, str_opt, str_pkg);
		printf("    %s {-R --remove}  [%s] <%s>\n", myname, str_opt, str_pkg);
		printf("    %s {-S --sync}    [%s] [%s]\n", myname, str_opt, str_pkg);
		printf("    %s {-U --upgrade} [%s] <%s>\n", myname, str_opt, str_file);
		printf(_("\nuse '%s {-h --help}' with an operation for available options\n"),
				myname);
	} else {
		if(op == PM_OP_REMOVE) {
			printf("%s:  %s {-R --remove} [%s] <%s>\n", str_usg, myname, str_opt, str_pkg);
			printf("%s:\n", str_opt);
			printf(_("  -c, --cascade        remove packages and all packages that depend on them\n"));
			printf(_("  -d, --nodeps         skip dependency checks\n"));
			printf(_("  -k, --dbonly         only remove database entry, do not remove files\n"));
			printf(_("  -n, --nosave         remove configuration files as well\n"));
			printf(_("  -s, --recursive      remove dependencies also (that won't break packages)\n"
				 "                       (-ss includes explicitly installed dependencies too)\n"));
			printf(_("  -u, --unneeded       remove unneeded packages (that won't break packages)\n"));
		} else if(op == PM_OP_UPGRADE) {
			printf("%s:  %s {-U --upgrade} [%s] <%s>\n", str_usg, myname, str_opt, str_file);
			printf("%s:\n", str_opt);
			printf(_("      --asdeps         install packages as non-explicitly installed\n"));
			printf(_("      --asexplicit     install packages as explicitly installed\n"));
			printf(_("  -d, --nodeps         skip dependency checks\n"));
			printf(_("  -f, --force          force install, overwrite conflicting files\n"));
		} else if(op == PM_OP_QUERY) {
			printf("%s:  %s {-Q --query} [%s] [%s]\n", str_usg, myname, str_opt, str_pkg);
			printf("%s:\n", str_opt);
			printf(_("  -c, --changelog      view the changelog of a package\n"));
			printf(_("  -d, --deps           list all packages installed as dependencies\n"));
			printf(_("  -e, --explicit       list all packages explicitly installed\n"));
			printf(_("  -g, --groups         view all members of a package group\n"));
			printf(_("  -i, --info           view package information (-ii for backup files)\n"));
			printf(_("  -l, --list           list the contents of the queried package\n"));
			printf(_("  -m, --foreign        list installed packages not found in sync db(s)\n"));
			printf(_("  -o, --owns <file>    query the package that owns <file>\n"));
			printf(_("  -p, --file <package> query a package file instead of the database\n"));
			printf(_("  -s, --search <regex> search locally-installed packages for matching strings\n"));
			printf(_("  -t, --unrequired     list all packages not required by any package\n"));
			printf(_("  -u, --upgrades       list all packages that can be upgraded\n"));
			printf(_("  -q, --quiet          show less information for query and search\n"));
		} else if(op == PM_OP_SYNC) {
			printf("%s:  %s {-S --sync} [%s] [%s]\n", str_usg, myname, str_opt, str_pkg);
			printf("%s:\n", str_opt);
			printf(_("      --asdeps         install packages as non-explicitly installed\n"));
			printf(_("      --asexplicit     install packages as explicitly installed\n"));
			printf(_("  -c, --clean          remove old packages from cache directory (-cc for all)\n"));
			printf(_("  -d, --nodeps         skip dependency checks\n"));
			printf(_("  -f, --force          force install, overwrite conflicting files\n"));
			printf(_("  -g, --groups         view all members of a package group\n"));
			printf(_("  -i, --info           view package information\n"));
			printf(_("  -l, --list <repo>    view a list of packages in a repo\n"));
			printf(_("  -p, --print-uris     print out URIs for given packages and their dependencies\n"));
			printf(_("  -s, --search <regex> search remote repositories for matching strings\n"));
			printf(_("  -u, --sysupgrade     upgrade all packages that are out of date\n"));
			printf(_("  -w, --downloadonly   download packages but do not install/upgrade anything\n"));
			printf(_("  -y, --refresh        download fresh package databases from the server\n"));
			printf(_("      --needed         don't reinstall up to date packages\n"));
			printf(_("      --ignore <pkg>   ignore a package upgrade (can be used more than once)\n"));
			printf(_("      --ignoregroup <grp>\n"
			         "                       ignore a group upgrade (can be used more than once)\n"));
			printf(_("  -q, --quiet          show less information for query and search\n"));
		}
		printf(_("      --config <path>  set an alternate configuration file\n"));
		printf(_("      --logfile <path> set an alternate log file\n"));
		printf(_("      --noconfirm      do not ask for any confirmation\n"));
		printf(_("      --noprogressbar  do not show a progress bar when downloading files\n"));
		printf(_("      --noscriptlet    do not execute the install scriptlet if one exists\n"));
		printf(_("  -v, --verbose        be verbose\n"));
		printf(_("  -r, --root <path>    set an alternate installation root\n"));
		printf(_("  -b, --dbpath <path>  set an alternate database location\n"));
		printf(_("      --cachedir <dir> set an alternate package cache location\n"));
	}
}

/** Output pacman version and copyright.
 */
static void version(void)
{
	printf("\n");
	printf(" .--.                  Pacman v%s - libalpm v%s\n", PACKAGE_VERSION, alpm_version());
	printf("/ _.-' .-.  .-.  .-.   Copyright (C) 2006-2008 Dan McGee <dan@archlinux.org>\n");
	printf("\\  '-. '-'  '-'  '-'   Copyright (C) 2002-2006 Judd Vinet <jvinet@zeroflux.org>\n");
	printf(" '--'\n");
	printf(_("                       This program may be freely redistributed under\n"
	         "                       the terms of the GNU General Public License.\n"));
	printf("\n");
}

/** Sets up gettext localization. Safe to call multiple times.
 */
/* Inspired by the monotone function localize_monotone. */
#if defined(ENABLE_NLS)
static void localize(void)
{
	static int init = 0;
	if (!init) {
		setlocale(LC_ALL, "");
		bindtextdomain(PACKAGE, LOCALEDIR);
		textdomain(PACKAGE);
		init = 1;
	}
}
#endif

/** Set user agent environment variable.
 */
static void setuseragent(void)
{
	char agent[101];
	struct utsname un;

	uname(&un);
	snprintf(agent, 100, "pacman/%s (%s %s) libalpm/%s",
			PACKAGE_VERSION, un.sysname, un.machine, alpm_version());
	setenv("HTTP_USER_AGENT", agent, 0);
}

/** Free the resources.
 *
 * @param ret the return value
 */
static void cleanup(int ret) {
	/* free alpm library resources */
	if(alpm_release() == -1) {
		pm_printf(PM_LOG_ERROR, alpm_strerrorlast());
	}

	/* free memory */
	FREELIST(pm_targets);
	if(config) {
		config_free(config);
		config = NULL;
	}

	exit(ret);
}

/** Write function that correctly handles EINTR.
 */
static ssize_t xwrite(int fd, const void *buf, size_t count)
{
	ssize_t ret;
	while((ret = write(fd, buf, count)) == -1 && errno == EINTR);
	return(ret);
}

/** Catches thrown signals. Performs necessary cleanup to ensure database is
 * in a consistant state.
 * @param signum the thrown signal
 */
static RETSIGTYPE handler(int signum)
{
	int out = fileno(stdout);
	int err = fileno(stderr);
	if(signum == SIGSEGV) {
		const char *msg1 = "error: segmentation fault\n";
		const char *msg2 = "Internal pacman error: Segmentation fault.\n"
			"Please submit a full bug report with --debug if appropriate.\n";
		/* write a error message to out, the rest to err */
		xwrite(out, msg1, strlen(msg1));
		xwrite(err, msg2, strlen(msg2));
		exit(signum);
	} else if((signum == SIGINT)) {
		const char *msg = "\nInterrupt signal received\n";
		xwrite(err, msg, strlen(msg));
		if(alpm_trans_interrupt() == 0) {
			/* a transaction is being interrupted, don't exit pacman yet. */
			return;
		}
		/* no commiting transaction, we can release it now and then exit pacman */
		alpm_trans_release();
		/* output a newline to be sure we clear any line we may be on */
		xwrite(out, "\n", 1);
	}
	cleanup(signum);
}

/** Sets all libalpm required paths in one go. Called after the command line
 * and inital config file parsing. Once this is complete, we can see if any
 * paths were defined. If a rootdir was defined and nothing else, we want all
 * of our paths to live under the rootdir that was specified. Safe to call
 * multiple times (will only do anything the first time).
 */
static void setlibpaths(void)
{
	static int init = 0;
	if (!init) {
		int ret = 0;

		pm_printf(PM_LOG_DEBUG, "setlibpaths() called\n");
		/* Configure root path first. If it is set and dbpath/logfile were not
		 * set, then set those as well to reside under the root. */
		if(config->rootdir) {
			char path[PATH_MAX];
			ret = alpm_option_set_root(config->rootdir);
			if(ret != 0) {
				pm_printf(PM_LOG_ERROR, _("problem setting rootdir '%s' (%s)\n"),
						config->rootdir, alpm_strerrorlast());
				cleanup(ret);
			}
			if(!config->dbpath) {
				/* omit leading slash from our static DBPATH, root handles it */
				snprintf(path, PATH_MAX, "%s%s", alpm_option_get_root(), DBPATH + 1);
				config->dbpath = strdup(path);
			}
			if(!config->logfile) {
				/* omit leading slash from our static LOGFILE path, root handles it */
				snprintf(path, PATH_MAX, "%s%s", alpm_option_get_root(), LOGFILE + 1);
				config->logfile = strdup(path);
			}
		}
		/* Set other paths if they were configured. Note that unless rootdir
		 * was left undefined, these two paths (dbpath and logfile) will have
		 * been set locally above, so the if cases below will now trigger. */
		if(config->dbpath) {
			ret = alpm_option_set_dbpath(config->dbpath);
			if(ret != 0) {
				pm_printf(PM_LOG_ERROR, _("problem setting dbpath '%s' (%s)\n"),
						config->dbpath, alpm_strerrorlast());
				cleanup(ret);
			}
		}
		if(config->logfile) {
			ret = alpm_option_set_logfile(config->logfile);
			if(ret != 0) {
				pm_printf(PM_LOG_ERROR, _("problem setting logfile '%s' (%s)\n"),
						config->logfile, alpm_strerrorlast());
				cleanup(ret);
			}
		}

		/* add a default cachedir if one wasn't specified */
		if(alpm_option_get_cachedirs() == NULL) {
			alpm_option_add_cachedir(CACHEDIR);
		}
		init = 1;
	}
}

/** Parse command-line arguments for each operation.
 * @param argc argc
 * @param argv argv
 * @return 0 on success, 1 on error
 */
static int parseargs(int argc, char *argv[])
{
	int opt;
	int option_index = 0;
	static struct option opts[] =
	{
		{"query",      no_argument,       0, 'Q'},
		{"remove",     no_argument,       0, 'R'},
		{"sync",       no_argument,       0, 'S'},
		{"deptest",    no_argument,       0, 'T'}, /* used by makepkg */
		{"upgrade",    no_argument,       0, 'U'},
		{"version",    no_argument,       0, 'V'},
		{"dbpath",     required_argument, 0, 'b'},
		{"cascade",    no_argument,       0, 'c'},
		{"changelog",  no_argument,       0, 'c'},
		{"clean",      no_argument,       0, 'c'},
		{"nodeps",     no_argument,       0, 'd'},
		{"deps",       no_argument,       0, 'd'},
		{"explicit",   no_argument,       0, 'e'},
		{"force",      no_argument,       0, 'f'},
		{"groups",     no_argument,       0, 'g'},
		{"help",       no_argument,       0, 'h'},
		{"info",       no_argument,       0, 'i'},
		{"dbonly",     no_argument,       0, 'k'},
		{"list",       no_argument,       0, 'l'},
		{"foreign",    no_argument,       0, 'm'},
		{"nosave",     no_argument,       0, 'n'},
		{"owns",       no_argument,       0, 'o'},
		{"file",       no_argument,       0, 'p'},
		{"print-uris", no_argument,       0, 'p'},
		{"quiet",      no_argument,       0, 'q'},
		{"root",       required_argument, 0, 'r'},
		{"recursive",  no_argument,       0, 's'},
		{"search",     no_argument,       0, 's'},
		{"unrequired", no_argument,       0, 't'},
		{"upgrades",   no_argument,       0, 'u'},
		{"sysupgrade", no_argument,       0, 'u'},
		{"unneeded",   no_argument,       0, 'u'},
		{"verbose",    no_argument,       0, 'v'},
		{"downloadonly", no_argument,     0, 'w'},
		{"refresh",    no_argument,       0, 'y'},
		{"noconfirm",  no_argument,       0, 1000},
		{"config",     required_argument, 0, 1001},
		{"ignore",     required_argument, 0, 1002},
		{"debug",      optional_argument, 0, 1003},
		{"noprogressbar", no_argument,    0, 1004},
		{"noscriptlet", no_argument,      0, 1005},
		{"cachedir",   required_argument, 0, 1007},
		{"asdeps",     no_argument,       0, 1008},
		{"logfile",    required_argument, 0, 1009},
		{"ignoregroup", required_argument, 0, 1010},
		{"needed",     no_argument,       0, 1011},
		{"asexplicit",     no_argument,   0, 1012},
		{0, 0, 0, 0}
	};

	while((opt = getopt_long(argc, argv, "RUFQSTr:b:vkhscVfmnoldepqituwygz", opts, &option_index))) {
		alpm_list_t *list = NULL, *item = NULL; /* lists for splitting strings */

		if(opt < 0) {
			break;
		}
		switch(opt) {
			case 0: break;
			case 1000: config->noconfirm = 1; break;
			case 1001:
				if(config->configfile) {
					free(config->configfile);
				}
				config->configfile = strndup(optarg, PATH_MAX);
				break;
			case 1002:
				list = strsplit(optarg, ',');
				for(item = list; item; item = alpm_list_next(item)) {
					alpm_option_add_ignorepkg((char *)alpm_list_getdata(item));
				}
				FREELIST(list);
				break;
			case 1003:
				/* debug levels are made more 'human readable' than using a raw logmask
				 * here, error and warning are set in config_new, though perhaps a
				 * --quiet option will remove these later */
				if(optarg) {
					unsigned short debug = atoi(optarg);
					switch(debug) {
						case 2:
							config->logmask |= PM_LOG_FUNCTION; /* fall through */
						case 1:
							config->logmask |= PM_LOG_DEBUG;
							break;
						default:
						  pm_printf(PM_LOG_ERROR, _("'%s' is not a valid debug level\n"),
									optarg);
							return(1);
					}
				} else {
					config->logmask |= PM_LOG_DEBUG;
				}
				/* progress bars get wonky with debug on, shut them off */
				config->noprogressbar = 1;
				break;
			case 1004: config->noprogressbar = 1; break;
			case 1005: config->flags |= PM_TRANS_FLAG_NOSCRIPTLET; break;
			case 1007:
				if(alpm_option_add_cachedir(optarg) != 0) {
					pm_printf(PM_LOG_ERROR, _("problem adding cachedir '%s' (%s)\n"),
							optarg, alpm_strerrorlast());
					return(1);
				}
				break;
			case 1008:
				config->flags |= PM_TRANS_FLAG_ALLDEPS;
				break;
			case 1009:
				config->logfile = strdup(optarg);
				break;
			case 1010:
				list = strsplit(optarg, ',');
				for(item = list; item; item = alpm_list_next(item)) {
					alpm_option_add_ignoregrp((char *)alpm_list_getdata(item));
				}
				FREELIST(list);
				break;
			case 1011: config->flags |= PM_TRANS_FLAG_NEEDED; break;
			case 1012:
				config->flags |= PM_TRANS_FLAG_ALLEXPLICIT;
				break;
			case 'Q': config->op = (config->op != PM_OP_MAIN ? 0 : PM_OP_QUERY); break;
			case 'R': config->op = (config->op != PM_OP_MAIN ? 0 : PM_OP_REMOVE); break;
			case 'S': config->op = (config->op != PM_OP_MAIN ? 0 : PM_OP_SYNC); break;
			case 'T': config->op = (config->op != PM_OP_MAIN ? 0 : PM_OP_DEPTEST); break;
			case 'U': config->op = (config->op != PM_OP_MAIN ? 0 : PM_OP_UPGRADE); break;
			case 'V': config->version = 1; break;
			case 'b':
				config->dbpath = strdup(optarg);
				break;
			case 'c':
				(config->op_s_clean)++;
				config->flags |= PM_TRANS_FLAG_CASCADE;
				config->op_q_changelog = 1;
				break;
			case 'd':
				config->op_q_deps = 1;
				config->flags |= PM_TRANS_FLAG_NODEPS;
				break;
			case 'e':
				config->op_q_explicit = 1;
				break;
			case 'f': config->flags |= PM_TRANS_FLAG_FORCE; break;
			case 'g': (config->group)++; break;
			case 'h': config->help = 1; break;
			case 'i': (config->op_q_info)++; (config->op_s_info)++; break;
			case 'k': config->flags |= PM_TRANS_FLAG_DBONLY; break;
			case 'l': config->op_q_list = 1; break;
			case 'm': config->op_q_foreign = 1; break;
			case 'n': config->flags |= PM_TRANS_FLAG_NOSAVE; break;
			case 'o': config->op_q_owns = 1; break;
			case 'p':
				config->op_q_isfile = 1;
				config->flags |= PM_TRANS_FLAG_PRINTURIS;
				break;
			case 'q':
				config->quiet = 1;
				break;
			case 'r':
				config->rootdir = strdup(optarg);
				break;
			case 's':
				config->op_s_search = 1;
				config->op_q_search = 1;
				if(config->flags & PM_TRANS_FLAG_RECURSE) {
					config->flags |= PM_TRANS_FLAG_RECURSEALL;
				} else {
					config->flags |= PM_TRANS_FLAG_RECURSE;
				}
				break;
			case 't':
				config->op_q_unrequired = 1;
				break;
			case 'u':
				config->op_s_upgrade = 1;
				config->op_q_upgrade = 1;
				config->flags |= PM_TRANS_FLAG_UNNEEDED;
				break;
			case 'v': (config->verbose)++; break;
			case 'w':
				config->op_s_downloadonly = 1;
				config->flags |= PM_TRANS_FLAG_DOWNLOADONLY;
				config->flags |= PM_TRANS_FLAG_NOCONFLICTS;
				break;
			case 'y': (config->op_s_sync)++; break;
			case '?': return(1);
			default: return(1);
		}
	}

	if(config->op == 0) {
		pm_printf(PM_LOG_ERROR, _("only one operation may be used at a time\n"));
		return(1);
	}

	if(config->help) {
		usage(config->op, mbasename(argv[0]));
		return(2);
	}
	if(config->version) {
		version();
		return(2);
	}

	while(optind < argc) {
		/* add the target to our target array */
		pm_targets = alpm_list_add(pm_targets, strdup(argv[optind]));
		optind++;
	}

	return(0);
}

/* helper for being used with setrepeatingoption */
static void option_add_syncfirst(const char *name) {
	config->syncfirst = alpm_list_add(config->syncfirst, strdup(name));
}

/** Add repeating options such as NoExtract, NoUpgrade, etc to libalpm
 * settings. Refactored out of the parseconfig code since all of them did
 * the exact same thing and duplicated code.
 * @param ptr a pointer to the start of the multiple options
 * @param option the string (friendly) name of the option, used for messages
 * @param optionfunc a function pointer to an alpm_option_add_* function
 */
static void setrepeatingoption(const char *ptr, const char *option,
		void (*optionfunc)(const char*))
{
	char *p = (char*)ptr;
	char *q;

	while((q = strchr(p, ' '))) {
		*q = '\0';
		(*optionfunc)(p);
		pm_printf(PM_LOG_DEBUG, "config: %s: %s\n", option, p);
		p = q;
		p++;
	}
	(*optionfunc)(p);
	pm_printf(PM_LOG_DEBUG, "config: %s: %s\n", option, p);
}

/* The real parseconfig. Called with a null section argument by the publicly
 * visible parseconfig so we can recall from within ourself on an include */
static int _parseconfig(const char *file, const char *givensection,
                        pmdb_t * const givendb)
{
	FILE *fp = NULL;
	char line[PATH_MAX+1];
	int linenum = 0;
	char *ptr, *section = NULL;
	pmdb_t *db = NULL;
	int ret = 0;

	pm_printf(PM_LOG_DEBUG, "config: attempting to read file %s\n", file);
	fp = fopen(file, "r");
	if(fp == NULL) {
		pm_printf(PM_LOG_ERROR, _("config file %s could not be read.\n"), file);
		return(1);
	}

	/* if we are passed a section, use it as our starting point */
	if(givensection != NULL) {
		section = strdup(givensection);
	}
	/* if we are passed a db, use it as our starting point */
	if(givendb != NULL) {
		db = givendb;
	}

	while(fgets(line, PATH_MAX, fp)) {
		linenum++;
		strtrim(line);

		/* ignore whole line and end of line comments */
		if(strlen(line) == 0 || line[0] == '#') {
			continue;
		}
		if((ptr = strchr(line, '#'))) {
			*ptr = '\0';
		}

		if(line[0] == '[' && line[strlen(line)-1] == ']') {
			/* new config section, skip the '[' */
			ptr = line;
			ptr++;
			if(section) {
				free(section);
			}
			section = strdup(ptr);
			section[strlen(section)-1] = '\0';
			pm_printf(PM_LOG_DEBUG, "config: new section '%s'\n", section);
			if(!strlen(section)) {
				pm_printf(PM_LOG_ERROR, _("config file %s, line %d: bad section name.\n"),
						file, linenum);
				ret = 1;
				goto cleanup;
			}
			/* if we are not looking at the options section, register a db and also
			 * ensure we have set all of our library paths as the library is too stupid
			 * at the moment to do lazy opening of the databases */
			if(strcmp(section, "options") != 0) {
				setlibpaths();
				db = alpm_db_register_sync(section);
				if(db == NULL) {
					pm_printf(PM_LOG_ERROR, _("could not register '%s' database (%s)\n"),
							section, alpm_strerrorlast());
					ret = 1;
					goto cleanup;
				}
			}
		} else {
			/* directive */
			char *key;
			/* strsep modifies the 'line' string: 'key \0 ptr' */
			key = line;
			ptr = line;
			strsep(&ptr, "=");
			strtrim(key);
			strtrim(ptr);

			if(key == NULL) {
				pm_printf(PM_LOG_ERROR, _("config file %s, line %d: syntax error in config file- missing key.\n"),
						file, linenum);
				ret = 1;
				goto cleanup;
			}
			/* For each directive, compare to the camelcase string. */
			if(section == NULL) {
				pm_printf(PM_LOG_ERROR, _("config file %s, line %d: All directives must belong to a section.\n"),
						file, linenum);
				ret = 1;
				goto cleanup;
			}
			if(ptr == NULL && strcmp(section, "options") == 0) {
				/* directives without settings, all in [options] */
				if(strcmp(key, "NoPassiveFtp") == 0) {
					alpm_option_set_nopassiveftp(1);
					pm_printf(PM_LOG_DEBUG, "config: nopassiveftp\n");
				} else if(strcmp(key, "UseSyslog") == 0) {
					alpm_option_set_usesyslog(1);
					pm_printf(PM_LOG_DEBUG, "config: usesyslog\n");
				} else if(strcmp(key, "ILoveCandy") == 0) {
					config->chomp = 1;
					pm_printf(PM_LOG_DEBUG, "config: chomp\n");
				} else if(strcmp(key, "ShowSize") == 0) {
					config->showsize = 1;
					pm_printf(PM_LOG_DEBUG, "config: showsize\n");
				} else if(strcmp(key, "UseDelta") == 0) {
					alpm_option_set_usedelta(1);
					pm_printf(PM_LOG_DEBUG, "config: usedelta\n");
				} else if(strcmp(key, "TotalDownload") == 0) {
					config->totaldownload = 1;
					pm_printf(PM_LOG_DEBUG, "config: totaldownload\n");
				} else {
					pm_printf(PM_LOG_ERROR, _("config file %s, line %d: directive '%s' not recognized.\n"),
							file, linenum, key);
					ret = 1;
					goto cleanup;
				}
			} else {
				/* directives with settings */
				if(strcmp(key, "Include") == 0) {
					pm_printf(PM_LOG_DEBUG, "config: including %s\n", ptr);
					_parseconfig(ptr, section, db);
					/* Ignore include failures... assume non-critical */
				} else if(strcmp(section, "options") == 0) {
					if(strcmp(key, "NoUpgrade") == 0) {
						setrepeatingoption(ptr, "NoUpgrade", alpm_option_add_noupgrade);
					} else if(strcmp(key, "NoExtract") == 0) {
						setrepeatingoption(ptr, "NoExtract", alpm_option_add_noextract);
					} else if(strcmp(key, "IgnorePkg") == 0) {
						setrepeatingoption(ptr, "IgnorePkg", alpm_option_add_ignorepkg);
					} else if(strcmp(key, "IgnoreGroup") == 0) {
						setrepeatingoption(ptr, "IgnoreGroup", alpm_option_add_ignoregrp);
					} else if(strcmp(key, "HoldPkg") == 0) {
						setrepeatingoption(ptr, "HoldPkg", alpm_option_add_holdpkg);
					} else if(strcmp(key, "SyncFirst") == 0) {
						setrepeatingoption(ptr, "SyncFirst", option_add_syncfirst);
					} else if(strcmp(key, "DBPath") == 0) {
						/* don't overwrite a path specified on the command line */
						if(!config->dbpath) {
							config->dbpath = strdup(ptr);
							pm_printf(PM_LOG_DEBUG, "config: dbpath: %s\n", ptr);
						}
					} else if(strcmp(key, "CacheDir") == 0) {
						if(alpm_option_add_cachedir(ptr) != 0) {
							pm_printf(PM_LOG_ERROR, _("problem adding cachedir '%s' (%s)\n"),
									ptr, alpm_strerrorlast());
							ret = 1;
							goto cleanup;
						}
						pm_printf(PM_LOG_DEBUG, "config: cachedir: %s\n", ptr);
					} else if(strcmp(key, "RootDir") == 0) {
						/* don't overwrite a path specified on the command line */
						if(!config->rootdir) {
							config->rootdir = strdup(ptr);
							pm_printf(PM_LOG_DEBUG, "config: rootdir: %s\n", ptr);
						}
					} else if (strcmp(key, "LogFile") == 0) {
						if(!config->logfile) {
							config->logfile = strdup(ptr);
							pm_printf(PM_LOG_DEBUG, "config: logfile: %s\n", ptr);
						}
					} else if (strcmp(key, "XferCommand") == 0) {
						alpm_option_set_xfercommand(ptr);
						pm_printf(PM_LOG_DEBUG, "config: xfercommand: %s\n", ptr);
					} else if (strcmp(key, "CleanMethod") == 0) {
						if (strcmp(ptr, "KeepInstalled") == 0) {
							config->cleanmethod = PM_CLEAN_KEEPINST;
						} else if (strcmp(ptr, "KeepCurrent") == 0) {
							config->cleanmethod = PM_CLEAN_KEEPCUR;
						} else {
							pm_printf(PM_LOG_ERROR, _("invalid value for 'CleanMethod' : '%s'\n"), ptr);
							ret = 1;
							goto cleanup;
						}
						pm_printf(PM_LOG_DEBUG, "config: cleanmethod: %s\n", ptr);
					} else {
						pm_printf(PM_LOG_ERROR, _("config file %s, line %d: directive '%s' not recognized.\n"),
								file, linenum, key);
						ret = 1;
						goto cleanup;
					}
				} else if(strcmp(key, "Server") == 0) {
					/* let's attempt a replacement for the current repo */
					char *server = strreplace(ptr, "$repo", section);

					if(alpm_db_setserver(db, server) != 0) {
						/* pm_errno is set by alpm_db_setserver */
						pm_printf(PM_LOG_ERROR, _("could not add server URL to database '%s': %s (%s)\n"),
								alpm_db_get_name(db), server, alpm_strerrorlast());
						free(server);
						ret = 1;
						goto cleanup;
					}

					free(server);
				} else {
					pm_printf(PM_LOG_ERROR, _("config file %s, line %d: directive '%s' not recognized.\n"),
							file, linenum, key);
					ret = 1;
					goto cleanup;
				}
			}
		}
	}

cleanup:
	if(fp) {
		fclose(fp);
	}
	if(section){
		free(section);
	}
	/* call setlibpaths here to ensure we have called it at least once */
	setlibpaths();
	pm_printf(PM_LOG_DEBUG, "config: finished parsing %s\n", file);
	return(ret);
}

/** Parse a configuration file.
 * @param file path to the config file.
 * @return 0 on success, non-zero on error
 */
static int parseconfig(const char *file)
{
	/* call the real parseconfig function with a null section & db argument */
	return(_parseconfig(file, NULL, NULL));
}

/** Main function.
 * @param argc argc
 * @param argv argv
 * @return A return code indicating success, failure, etc.
 */
int main(int argc, char *argv[])
{
	int ret = 0;
	struct sigaction new_action, old_action;
#if defined(HAVE_GETEUID) && !defined(CYGWIN)
	/* geteuid undefined in CYGWIN */
	uid_t myuid = geteuid();
#endif

#if defined(PACMAN_DEBUG) && defined(HAVE_MCHECK_H)
	/*setenv("MALLOC_TRACE","pacman.mtrace", 0);*/
	mtrace();
#endif

	/* Set signal handlers */
	/* Set up the structure to specify the new action. */
	new_action.sa_handler = handler;
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = 0;

	sigaction(SIGINT, NULL, &old_action);
	if(old_action.sa_handler != SIG_IGN) {
		sigaction(SIGINT, &new_action, NULL);
	}
	sigaction(SIGTERM, NULL, &old_action);
	if(old_action.sa_handler != SIG_IGN) {
		sigaction(SIGTERM, &new_action, NULL);
	}
	sigaction(SIGSEGV, NULL, &old_action);
	if(old_action.sa_handler != SIG_IGN) {
		sigaction(SIGSEGV, &new_action, NULL);
	}

	/* i18n init */
#if defined(ENABLE_NLS)
	localize();
#endif

	/* set user agent for downloading */
	setuseragent();

	/* init config data */
	config = config_new();

	/* disable progressbar if the output is redirected */
	if(!isatty(1)) {
		config->noprogressbar = 1;
	}

	/* initialize library */
	if(alpm_initialize() == -1) {
		pm_printf(PM_LOG_ERROR, _("failed to initialize alpm library (%s)\n"),
		        alpm_strerrorlast());
		cleanup(EXIT_FAILURE);
	}

	/* Setup logging as soon as possible, to print out maximum debugging info */
	alpm_option_set_logcb(cb_log);
	alpm_option_set_dlcb(cb_dl_progress);
	/* define paths to reasonable defaults */
	alpm_option_set_root(ROOTDIR);
	alpm_option_set_dbpath(DBPATH);
	alpm_option_set_logfile(LOGFILE);

	/* Priority of options:
	 * 1. command line
	 * 2. config file
	 * 3. compiled-in defaults
	 * However, we have to parse the command line first because a config file
	 * location can be specified here, so we need to make sure we prefer these
	 * options over the config file coming second.
	 */

	/* parse the command line */
	ret = parseargs(argc, argv);
	if(ret != 0) {
		cleanup(ret);
	}

	/* parse the config file */
	ret = parseconfig(config->configfile);
	if(ret != 0) {
		cleanup(ret);
	}

	/* set TotalDownload callback if option enabled */
	if(config->totaldownload) {
		alpm_option_set_totaldlcb(cb_dl_total);
	}

#if defined(HAVE_GETEUID) && !defined(CYGWIN)
	/* check if we have sufficient permission for the requested operation */
	if(myuid > 0 && needs_transaction()) {
		pm_printf(PM_LOG_ERROR, _("you cannot perform this operation unless you are root.\n"));
		cleanup(EXIT_FAILURE);
	}
#endif

	if(config->verbose > 0) {
		alpm_list_t *i;
		printf("Root      : %s\n", alpm_option_get_root());
		printf("Conf File : %s\n", config->configfile);
		printf("DB Path   : %s\n", alpm_option_get_dbpath());
		printf("Cache Dirs: ");
		for(i = alpm_option_get_cachedirs(); i; i = alpm_list_next(i)) {
			printf("%s  ", (char*)alpm_list_getdata(i));
		}
		printf("\n");
		printf("Lock File : %s\n", alpm_option_get_lockfile());
		printf("Log File  : %s\n", alpm_option_get_logfile());
		list_display("Targets   :", pm_targets);
	}

	/* Opening local database */
	db_local = alpm_db_register_local();
	if(db_local == NULL) {
		pm_printf(PM_LOG_ERROR, _("could not register 'local' database (%s)\n"),
		        alpm_strerrorlast());
		cleanup(EXIT_FAILURE);
	}

	/* start the requested operation */
	switch(config->op) {
		case PM_OP_REMOVE:
			ret = pacman_remove(pm_targets);
			break;
		case PM_OP_UPGRADE:
			ret = pacman_upgrade(pm_targets);
			break;
		case PM_OP_QUERY:
			ret = pacman_query(pm_targets);
			break;
		case PM_OP_SYNC:
			ret = pacman_sync(pm_targets);
			break;
		case PM_OP_DEPTEST:
			ret = pacman_deptest(pm_targets);
			break;
		default:
			pm_printf(PM_LOG_ERROR, _("no operation specified (use -h for help)\n"));
			ret = EXIT_FAILURE;
	}

	cleanup(ret);
	/* not reached */
	return(EXIT_SUCCESS);
}

/* vim: set ts=2 sw=2 noet: */
