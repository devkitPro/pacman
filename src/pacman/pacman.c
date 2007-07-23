/*
 *  pacman.c
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

/* TODO hard to believe all these are needed just for this file */
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <libintl.h>
#include <locale.h>
#include <time.h>
#if defined(PACMAN_DEBUG) && defined(HAVE_MTRACE)
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

config_t *config;

pmdb_t *db_local;
/* list of targets specified on command line */
static alpm_list_t *pm_targets;

/** Display usage/syntax for the specified operation.
 * @param op     the operation code requested
 * @param myname basename(argv[0])
 */
static void usage(int op, char *myname)
{
	/* prefetch some strings for usage below, which moves a lot of calls
	 * out of gettext. */
	char * const str_opt = _("options");
	char * const str_file = _("file");
	char * const str_pkg = _("package");
	char * const str_usg = _("usage");
	char * const str_opr = _("operation");

	if(op == PM_OP_MAIN) {
		printf("%s:  %s <%s> [...]\n", str_usg, myname, str_opr);
		printf("%s:\n", str_opt);
		printf("    %s {-h --help}\n", myname);
		printf("    %s {-V --version}\n", myname);
		printf("    %s {-A --add}     [%s] <%s>\n", myname, str_opt, str_file);
		printf("    %s {-F --freshen} [%s] <%s>\n", myname, str_opt, str_file);
		printf("    %s {-Q --query}   [%s] [%s]\n", myname, str_opt, str_pkg);
		printf("    %s {-R --remove}  [%s] <%s>\n", myname, str_opt, str_pkg);
		printf("    %s {-S --sync}    [%s] [%s]\n", myname, str_opt, str_pkg);
		printf("    %s {-U --upgrade} [%s] <%s>\n", myname, str_opt, str_file);
		printf(_("\nuse '%s --help' with other options for more syntax\n"), myname);
	} else {
		if(op == PM_OP_ADD) {
			printf("%s:  %s {-A --add} [%s] <%s>\n", str_usg, myname, str_opt, str_file);
			printf("%s:\n", str_opt);
			printf(_("      --asdeps         install packages as non-explicitly installed\n"));
			printf(_("  -d, --nodeps         skip dependency checks\n"));
			printf(_("  -f, --force          force install, overwrite conflicting files\n"));
		} else if(op == PM_OP_REMOVE) {
			printf("%s:  %s {-R --remove} [%s] <%s>\n", str_usg, myname, str_opt, str_pkg);
			printf(_("usage:  %s {-R --remove} [options] <package>\n"), myname);
			printf("%s:\n", str_opt);
			printf(_("  -c, --cascade        remove packages and all packages that depend on them\n"));
			printf(_("  -d, --nodeps         skip dependency checks\n"));
			printf(_("  -k, --dbonly         only remove database entry, do not remove files\n"));
			printf(_("  -n, --nosave         remove configuration files as well\n"));
			printf(_("  -s, --recursive      remove dependencies also (that won't break packages)\n"));
		} else if(op == PM_OP_UPGRADE) {
			if(config->flags & PM_TRANS_FLAG_FRESHEN) {
				printf("%s:  %s {-F --freshen} [%s] <%s>\n", str_usg, myname, str_opt, str_file);
			} else {
				printf("%s:  %s {-U --upgrade} [%s] <%s>\n", str_usg, myname, str_opt, str_file);
			}
			printf("%s:\n", str_opt);
			printf(_("      --asdeps         install packages as non-explicitly installed\n"));
			printf(_("  -d, --nodeps         skip dependency checks\n"));
			printf(_("  -f, --force          force install, overwrite conflicting files\n"));
		} else if(op == PM_OP_QUERY) {
			printf("%s:  %s {-Q --query} [%s] [%s]\n", str_usg, myname, str_opt, str_pkg);
			printf("%s:\n", str_opt);
			printf(_("  -c, --changelog      view the changelog of a package\n"));
			printf(_("  -e, --orphans        list all packages installed as dependencies but no longer\n"
			         "                       required by any package\n"));
			printf(_("  -g, --groups         view all members of a package group\n"));
			printf(_("  -i, --info           view package information\n"));
			printf(_("  -l, --list           list the contents of the queried package\n"));
			printf(_("  -m, --foreign        list installed packages not found in sync db(s)\n"));
			printf(_("  -o, --owns <file>    query the package that owns <file>\n"));
			printf(_("  -p, --file <package> query a package file instead of the database\n"));
			printf(_("  -s, --search <regex> search locally-installed packages for matching strings\n"));
			printf(_("  -t, --test           check the consistency of the local database\n"));
			printf(_("  -u, --upgrades       list all packages that can be upgraded\n"));
		} else if(op == PM_OP_SYNC) {
			printf("%s:  %s {-S --sync} [%s] [%s]\n", str_usg, myname, str_opt, str_pkg);
			printf("%s:\n", str_opt);
			printf(_("      --asdeps         install packages as non-explicitly installed\n"));
			printf(_("  -c, --clean          remove old packages from cache directory (-cc for all)\n"));
			printf(_("  -d, --nodeps         skip dependency checks\n"));
			printf(_("  -e, --dependsonly    install dependencies only\n"));
			printf(_("  -f, --force          force install, overwrite conflicting files\n"));
			printf(_("  -g, --groups         view all members of a package group\n"));
			printf(_("  -i, --info           view package information\n"));
			printf(_("  -l, --list <repo>    view a list of packages in a repo\n"));
			printf(_("  -p, --print-uris     print out URIs for given packages and their dependencies\n"));
			printf(_("  -s, --search <regex> search remote repositories for matching strings\n"));
			printf(_("  -u, --sysupgrade     upgrade all packages that are out of date\n"));
			printf(_("  -w, --downloadonly   download packages but do not install/upgrade anything\n"));
			printf(_("  -y, --refresh        download fresh package databases from the server\n"));
			printf(_("      --ignore <pkg>   ignore a package upgrade (can be used more than once)\n"));
		}
		printf(_("      --config <path>  set an alternate configuration file\n"));
		printf(_("      --noconfirm      do not ask for any confirmation\n"));
		printf(_("      --ask <number>   pre-specify answers for questions (see manpage)\n"));
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
	printf(" .--.                  Pacman v%s - libalpm v%s\n", PACKAGE_VERSION, LIB_VERSION);
	printf("/ _.-' .-.  .-.  .-.   Copyright (C) 2002-2007 Judd Vinet <jvinet@zeroflux.org>\n");
	printf("\\  '-. '-'  '-'  '-'\n");
	printf(" '--'\n");
	printf(_("                       This program may be freely redistributed under\n"
	         "                       the terms of the GNU General Public License\n"));
	printf("\n");
}

/** Sets up gettext localization. Safe to call multiple times.
 */
/* Inspired by the monotone function localize_monotone. */
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

/** Set user agent environment variable.
 */
static void setuseragent(void)
{
	const char *pacman = "Pacman/" PACKAGE_VERSION;
	const char *libalpm = "libalpm/" LIB_VERSION;
	char agent[101];
	struct utsname un;

	uname(&un);
	snprintf(agent, 100, "%s (%s %s %s; %s) %s", pacman, un.sysname,
	         un.machine, un.release, setlocale(LC_MESSAGES, NULL), libalpm);
	setenv("HTTP_USER_AGENT", agent, 0);
}

/** Catches thrown signals. Performs necessary cleanup to ensure database is
 * in a consistant state.
 * @param signum the thrown signal
 */
static void cleanup(int signum)
{
	if(signum==SIGSEGV)
	{
		/* write a log message and write to stderr */
		pm_printf(PM_LOG_ERROR, "segmentation fault\n");
		pm_fprintf(stderr, PM_LOG_ERROR, "Internal pacman error: Segmentation fault.\n"
		        "Please submit a full bug report with --debug if appropriate.\n");
		exit(signum);
	} else if((signum == SIGINT) && (alpm_trans_release() == -1)
						&& (pm_errno == PM_ERR_TRANS_COMMITING)) {
		return;
	}

	/* free alpm library resources */
	if(alpm_release() == -1) {
		pm_printf(PM_LOG_ERROR, alpm_strerror(pm_errno));
	}

	/* free memory */
	FREELIST(pm_targets);
	if(config) {
		config_free(config);
		config = NULL;
	}

	exit(signum);
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
		{"add",        no_argument,       0, 'A'},
		{"freshen",    no_argument,       0, 'F'},
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
		{"dependsonly",no_argument,       0, 'e'},
		{"orphans",    no_argument,       0, 'e'},
		{"force",      no_argument,       0, 'f'},
		{"groups",     no_argument,       0, 'g'},
		{"help",       no_argument,       0, 'h'},
		{"info",       no_argument,       0, 'i'},
		{"dbonly",     no_argument,       0, 'k'},
		{"list",       no_argument,       0, 'l'},
		{"nosave",     no_argument,       0, 'n'},
		{"foreign",    no_argument,       0, 'm'},
		{"owns",       no_argument,       0, 'o'},
		{"file",       no_argument,       0, 'p'},
		{"print-uris", no_argument,       0, 'p'},
		{"root",       required_argument, 0, 'r'},
		{"recursive",  no_argument,       0, 's'},
		{"search",     no_argument,       0, 's'},
		{"test",       no_argument,       0, 't'},
		{"upgrades",   no_argument,       0, 'u'},
		{"sysupgrade", no_argument,       0, 'u'},
		{"verbose",    no_argument,       0, 'v'},
		{"downloadonly", no_argument,     0, 'w'},
		{"refresh",    no_argument,       0, 'y'},
		{"noconfirm",  no_argument,       0, 1000},
		{"config",     required_argument, 0, 1001},
		{"ignore",     required_argument, 0, 1002},
		{"debug",      optional_argument, 0, 1003},
		{"noprogressbar", no_argument,    0, 1004},
		{"noscriptlet", no_argument,      0, 1005},
		{"ask",        required_argument, 0, 1006},
		{"cachedir",   required_argument, 0, 1007},
		{"asdeps",     no_argument,       0, 1008},
		{0, 0, 0, 0}
	};
	struct stat st;

	while((opt = getopt_long(argc, argv, "ARUFQSTr:b:vkhscVfmnoldepituwygz", opts, &option_index))) {
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
			case 1002: alpm_option_add_ignorepkg(strdup(optarg)); break;
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
			case 1006: config->noask = 1; config->ask = atoi(optarg); break;
			case 1007:
				/* TODO redo this logic- check path somewhere else, delete other cachedirs, etc */
				if(stat(optarg, &st) == -1 || !S_ISDIR(st.st_mode)) {
					pm_printf(PM_LOG_ERROR, _("'%s' is not a valid cache directory\n"),
							optarg);
					return(1);
				}
				alpm_option_add_cachedir(optarg);
				break;
			case 1008:
				config->flags |= PM_TRANS_FLAG_ALLDEPS;
				break;
			case 'A': config->op = (config->op != PM_OP_MAIN ? 0 : PM_OP_ADD); break;
			case 'F':
				config->op = (config->op != PM_OP_MAIN ? 0 : PM_OP_UPGRADE);
				config->flags |= PM_TRANS_FLAG_FRESHEN;
				break;
			case 'Q': config->op = (config->op != PM_OP_MAIN ? 0 : PM_OP_QUERY); break;
			case 'R': config->op = (config->op != PM_OP_MAIN ? 0 : PM_OP_REMOVE); break;
			case 'S': config->op = (config->op != PM_OP_MAIN ? 0 : PM_OP_SYNC); break;
			case 'T': config->op = (config->op != PM_OP_MAIN ? 0 : PM_OP_DEPTEST); break;
			case 'U': config->op = (config->op != PM_OP_MAIN ? 0 : PM_OP_UPGRADE); break;
			case 'V': config->version = 1; break;
			case 'b':
				if(stat(optarg, &st) == -1 || !S_ISDIR(st.st_mode)) {
					pm_printf(PM_LOG_ERROR, _("'%s' is not a valid db path\n"),
							optarg);
					return(1);
				}
				alpm_option_set_dbpath(optarg);
				break;
			case 'c':
				(config->op_s_clean)++;
				config->flags |= PM_TRANS_FLAG_CASCADE;
				config->op_q_changelog = 1;
				break;
			case 'd': config->flags |= PM_TRANS_FLAG_NODEPS; break;
			case 'e':
				config->op_q_orphans++;
				config->flags |= PM_TRANS_FLAG_DEPENDSONLY;
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
			case 'r':
				if(stat(optarg, &st) == -1 || !S_ISDIR(st.st_mode)) {
					pm_printf(PM_LOG_ERROR, _("'%s' is not a valid root path\n"),
							optarg);
					return(1);
				}
				alpm_option_set_root(optarg);
				break;
			case 's':
				config->op_s_search = 1;
				config->op_q_search = 1;
				config->flags |= PM_TRANS_FLAG_RECURSE;
				break;
			case 't':
				config->op_q_test = 1;
				break;
			case 'u':
				config->op_s_upgrade = 1;
				config->op_q_upgrade = 1;
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
		usage(config->op, basename(argv[0]));
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
	struct stat st;

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
				return(1);
			}
			/* a section/database named local is not allowed */
			if(!strcmp(section, "local")) {
				pm_printf(PM_LOG_ERROR, _("config file %s, line %d: 'local' cannot be used as section name.\n"),
						file, linenum);
				return(1);
			}
			/* if we are not looking at the options section, register a db */
			if(strcmp(section, "options") != 0) {
				db = alpm_db_register(section);
			}
		} else {
			/* directive */
			char *key;
			const char *upperkey;
			/* strsep modifies the 'line' string: 'key \0 ptr' */
			key = line;
			ptr = line;
			strsep(&ptr, "=");
			strtrim(key);
			strtrim(ptr);

			if(key == NULL) {
				pm_printf(PM_LOG_ERROR, _("config file %s, line %d: syntax error in config file- missing key.\n"),
						file, linenum);
				return(1);
			}
			upperkey = strtoupper(strdup(key));
			if(section == NULL && (strcmp(key, "Include") == 0 || strcmp(upperkey, "INCLUDE") == 0)) {
				pm_printf(PM_LOG_ERROR, _("config file %s, line %d: 'Include' directive must belong to a section.\n"),
						file, linenum);
				return(1);
			}
			if(ptr == NULL && strcmp(section, "options") == 0) {
				/* directives without settings, all in [options] */
				if(strcmp(key, "NoPassiveFTP") == 0 || strcmp(upperkey, "NOPASSIVEFTP") == 0) {
					alpm_option_set_nopassiveftp(1);
					pm_printf(PM_LOG_DEBUG, "config: nopassiveftp\n");
				} else if(strcmp(key, "UseSyslog") == 0 || strcmp(upperkey, "USESYSLOG") == 0) {
					alpm_option_set_usesyslog(1);
					pm_printf(PM_LOG_DEBUG, "config: usesyslog\n");
				} else if(strcmp(key, "ILoveCandy") == 0 || strcmp(upperkey, "ILOVECANDY") == 0) {
					config->chomp = 1;
					pm_printf(PM_LOG_DEBUG, "config: chomp\n");
				} else if(strcmp(key, "UseColor") == 0 || strcmp(upperkey, "USECOLOR") == 0) {
					config->usecolor = 1;
					pm_printf(PM_LOG_DEBUG, "config: usecolor\n");
				} else if(strcmp(key, "ShowSize") == 0 || strcmp(upperkey, "SHOWSIZE") == 0) {
					config->showsize= 1;
					pm_printf(PM_LOG_DEBUG, "config: showsize\n");
				} else {
					pm_printf(PM_LOG_ERROR, _("config file %s, line %d: directive '%s' not recognized.\n"),
							file, linenum, key);
					return(1);
				}
			} else {
				/* directives with settings */
				if(strcmp(key, "Include") == 0 || strcmp(upperkey, "INCLUDE") == 0) {
					int ret;
					pm_printf(PM_LOG_DEBUG, "config: including %s\n", ptr);
					ret = _parseconfig(ptr, section, db);
					if(ret != 0) {
						return(ret);
					}
				} else if(strcmp(section, "options") == 0) {
					if(strcmp(key, "NoUpgrade") == 0 || strcmp(upperkey, "NOUPGRADE") == 0) {
						/* TODO functionalize this */
						char *p = ptr;
						char *q;

						while((q = strchr(p, ' '))) {
							*q = '\0';
							alpm_option_add_noupgrade(p);
							pm_printf(PM_LOG_DEBUG, "config: noupgrade: %s\n", p);
							p = q;
							p++;
						}
						alpm_option_add_noupgrade(p);
						pm_printf(PM_LOG_DEBUG, "config: noupgrade: %s\n", p);
					} else if(strcmp(key, "NoExtract") == 0 || strcmp(upperkey, "NOEXTRACT") == 0) {
						char *p = ptr;
						char *q;

						while((q = strchr(p, ' '))) {
							*q = '\0';
							alpm_option_add_noextract(p);
							pm_printf(PM_LOG_DEBUG, "config: noextract: %s\n", p);
							p = q;
							p++;
						}
						alpm_option_add_noextract(p);
						pm_printf(PM_LOG_DEBUG, "config: noextract: %s\n", p);
					} else if(strcmp(key, "IgnorePkg") == 0 || strcmp(upperkey, "IGNOREPKG") == 0) {
						char *p = ptr;
						char *q;

						while((q = strchr(p, ' '))) {
							*q = '\0';
							alpm_option_add_ignorepkg(p);
							pm_printf(PM_LOG_DEBUG, "config: ignorepkg: %s", p);
							p = q;
							p++;
						}
						alpm_option_add_ignorepkg(p);
						pm_printf(PM_LOG_DEBUG, "config: ignorepkg: %s\n", p);
					} else if(strcmp(key, "HoldPkg") == 0 || strcmp(upperkey, "HOLDPKG") == 0) {
						char *p = ptr;
						char *q;

						while((q = strchr(p, ' '))) {
							*q = '\0';
							alpm_option_add_holdpkg(p);
							pm_printf(PM_LOG_DEBUG, "config: holdpkg: %s\n", p);
							p = q;
							p++;
						}
						alpm_option_add_holdpkg(p);
						pm_printf(PM_LOG_DEBUG, "config: holdpkg: %s\n", p);
					} else if(strcmp(key, "DBPath") == 0 || strcmp(upperkey, "DBPATH") == 0) {
						/* don't overwrite a path specified on the command line */
						if(alpm_option_get_dbpath() == NULL) {
							if(stat(ptr, &st) == -1 || !S_ISDIR(st.st_mode)) {
								pm_printf(PM_LOG_ERROR, _("'%s' is not a valid db path\n"),
										ptr);
								return(1);
							}
							alpm_option_set_dbpath(ptr);
							pm_printf(PM_LOG_DEBUG, "config: dbpath: %s\n", ptr);
						}
					} else if(strcmp(key, "CacheDir") == 0 || strcmp(upperkey, "CACHEDIR") == 0) {
						if(stat(ptr, &st) == -1 || !S_ISDIR(st.st_mode)) {
							pm_printf(PM_LOG_WARNING, _("'%s' is not a valid cache directory\n"),
									ptr);
						} else {
							alpm_option_add_cachedir(ptr);
							pm_printf(PM_LOG_DEBUG, "config: cachedir: %s\n", ptr);
						}
					} else if(strcmp(key, "RootDir") == 0 || strcmp(upperkey, "ROOTDIR") == 0) {
						/* don't overwrite a path specified on the command line */
						if(alpm_option_get_root() == NULL) {
							if(stat(ptr, &st) == -1 || !S_ISDIR(st.st_mode)) {
								pm_printf(PM_LOG_ERROR, _("'%s' is not a valid root path\n"),
										ptr);
								return(1);
							}
							alpm_option_set_root(ptr);
							pm_printf(PM_LOG_DEBUG, "config: rootdir: %s\n", ptr);
						}
					} else if (strcmp(key, "LogFile") == 0 || strcmp(upperkey, "LOGFILE") == 0) {
						if(alpm_option_get_logfile() == NULL) {
							alpm_option_set_logfile(ptr);
							pm_printf(PM_LOG_DEBUG, "config: logfile: %s\n", ptr);
						}
					} else if (strcmp(key, "XferCommand") == 0 || strcmp(upperkey, "XFERCOMMAND") == 0) {
						alpm_option_set_xfercommand(ptr);
						pm_printf(PM_LOG_DEBUG, "config: xfercommand: %s\n", ptr);
					} else if (strcmp(key, "UpgradeDelay") == 0 || strcmp(upperkey, "UPGRADEDELAY") == 0) {
						/* The config value is in days, we use seconds */
						time_t ud = atol(ptr) * 60 * 60 *24;
						alpm_option_set_upgradedelay(ud);
						pm_printf(PM_LOG_DEBUG, "config: upgradedelay: %d\n", (int)ud);
					} else {
						pm_printf(PM_LOG_ERROR, _("config file %s, line %d: directive '%s' not recognized.\n"),
								file, linenum, key);
						return(1);
					}
				} else if(strcmp(key, "Server") == 0 || strcmp(upperkey, "SERVER") == 0) {
					/* let's attempt a replacement for the current repo */
					char *server = strreplace(ptr, "$repo", section);

					if(alpm_db_setserver(db, server) != 0) {
						/* pm_errno is set by alpm_db_setserver */
						return(1);
					}

					free(server);
				} else {
					pm_printf(PM_LOG_ERROR, _("config file %s, line %d: directive '%s' not recognized.\n"),
							file, linenum, key);
					return(1);
				}
			}
		}
	}
	fclose(fp);

	pm_printf(PM_LOG_DEBUG, "config: finished parsing %s\n", file);
	return(0);
}

/** Parse a configuration file.
 * @param file path to the config file.
 * @return 0 on success, non-zero on error
 */
int parseconfig(const char *file)
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
#if defined(HAVE_GETEUID)
	/* geteuid undefined in CYGWIN */
	uid_t myuid = geteuid();
#endif

#if defined(PACMAN_DEBUG) && defined(HAVE_MTRACE)
	/*setenv("MALLOC_TRACE","pacman.mtrace", 0);*/
	mtrace();
#endif

	/* set signal handlers */
	signal(SIGINT, cleanup);
	signal(SIGTERM, cleanup);
	signal(SIGSEGV, cleanup);

	/* i18n init */
	localize();

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
		        alpm_strerror(pm_errno));
		cleanup(EXIT_FAILURE);
	}

	/* Setup logging as soon as possible, to print out maximum debugging info */
	alpm_option_set_logcb(cb_log);
	alpm_option_set_dlcb(cb_dl_progress);

	/* parse the command line */
	ret = parseargs(argc, argv);
	if(ret != 0) {
		cleanup(ret);
	}

	/* use default config file if location wasn't specified on cmdline */
	if(config->configfile == NULL) {
		config->configfile = strdup(CONFFILE);
	}

	/* parse the config file */
	ret = parseconfig(config->configfile);
	if(ret != 0) {
		cleanup(ret);
	}

	/* ensure root and dbpath were defined */
	if(alpm_option_get_root() == NULL) {
		alpm_option_set_root(ROOTDIR);
	}
	if(alpm_option_get_dbpath() == NULL) {
		alpm_option_set_dbpath(DBPATH);
	}

#if defined(HAVE_GETEUID)
	/* check if we have sufficient permission for the requested operation */
if(0) {
	if(myuid > 0) {
		if(config->op != PM_OP_MAIN && config->op != PM_OP_QUERY && config->op != PM_OP_DEPTEST) {
			if((config->op == PM_OP_SYNC && !config->op_s_sync &&
					(config->op_s_search || config->group || config->op_q_list || config->op_q_info
					 || config->flags & PM_TRANS_FLAG_PRINTURIS))
				 || config->op == PM_OP_DEPTEST
				 || (strcmp(alpm_option_get_root(), "/") != 0)) {
				/* special case: PM_OP_SYNC can be used w/ config->op_s_search by any user */
				/* special case: ignore root user check if -r is specified, fall back on
				 * normal FS checking */
			} else {
				pm_printf(PM_LOG_ERROR, _("you cannot perform this operation unless you are root.\n"));
				cleanup(EXIT_FAILURE);
			}
		}
	}
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
	db_local = alpm_db_register("local");
	if(db_local == NULL) {
		pm_printf(PM_LOG_ERROR, _("could not register 'local' database (%s)\n"),
		        alpm_strerror(pm_errno));
		cleanup(EXIT_FAILURE);
	}

	/* start the requested operation */
	switch(config->op) {
		case PM_OP_ADD:
			ret = pacman_add(pm_targets);
			break;
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
