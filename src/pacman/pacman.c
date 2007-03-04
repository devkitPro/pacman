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
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libintl.h>
#include <locale.h>
#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__OpenBSD__) || defined(__APPLE__)
#include <sys/malloc.h>
#elif defined(CYGWIN)
#include <libgen.h> /* basename */
#else
#include <mcheck.h> /* debug */
#endif
#include <time.h>

#include <alpm.h>
#include <alpm_list.h>
/* pacman */
#include "util.h"
#include "log.h"
#include "downloadprog.h"
#include "conf.h"
#include "package.h"
#include "add.h"
#include "remove.h"
#include "upgrade.h"
#include "query.h"
#include "sync.h"
#include "deptest.h"

#if defined(__OpenBSD__) || defined(__APPLE__)
#define BSD
#endif

/* Operations */
enum {
	PM_OP_MAIN = 1,
	PM_OP_ADD,
	PM_OP_REMOVE,
	PM_OP_UPGRADE,
	PM_OP_QUERY,
	PM_OP_SYNC,
	PM_OP_DEPTEST
};

config_t *config;

pmdb_t *db_local;
/* list of targets specified on command line */
static alpm_list_t *pm_targets;

/* Display usage/syntax for the specified operation.
 *     op:     the operation code requested
 *     myname: basename(argv[0])
 */
static void usage(int op, char *myname)
{
	if(op == PM_OP_MAIN) {
		printf(_("usage:  %s {-h --help}\n"), myname);
		printf(_("        %s {-V --version}\n"), myname);
		printf(_("        %s {-A --add}     [options] <file>\n"), myname);
		printf(_("        %s {-F --freshen} [options] <file>\n"), myname);
		printf(_("        %s {-Q --query}   [options] [package]\n"), myname);
		printf(_("        %s {-R --remove}  [options] <package>\n"), myname);
		printf(_("        %s {-S --sync}    [options] [package]\n"), myname);
		printf(_("        %s {-U --upgrade} [options] <file>\n"), myname);
		printf(_("\nuse '%s --help' with other options for more syntax\n"), myname);
	} else {
		if(op == PM_OP_ADD) {
			printf(_("usage:  %s {-A --add} [options] <file>\n"), myname);
			printf(_("options:\n"));
			printf(_("  -d, --nodeps         skip dependency checks\n"));
			printf(_("  -f, --force          force install, overwrite conflicting files\n"));
		} else if(op == PM_OP_REMOVE) {
			printf(_("usage:  %s {-R --remove} [options] <package>\n"), myname);
			printf(_("options:\n"));
			printf(_("  -c, --cascade        remove packages and all packages that depend on them\n"));
			printf(_("  -d, --nodeps         skip dependency checks\n"));
			printf(_("  -k, --dbonly         only remove database entry, do not remove files\n"));
			printf(_("  -n, --nosave         remove configuration files as well\n"));
			printf(_("  -s, --recursive      remove dependencies also (that won't break packages)\n"));
		} else if(op == PM_OP_UPGRADE) {
			if(config->flags & PM_TRANS_FLAG_FRESHEN) {
				printf(_("usage:  %s {-F --freshen} [options] <file>\n"), myname);
			} else {
				printf(_("usage:  %s {-U --upgrade} [options] <file>\n"), myname);
			}
			printf(_("options:\n"));
			printf(_("  -d, --nodeps         skip dependency checks\n"));
			printf(_("  -f, --force          force install, overwrite conflicting files\n"));
		} else if(op == PM_OP_QUERY) {
			printf(_("usage:  %s {-Q --query} [options] [package]\n"), myname);
			printf(_("options:\n"));
			printf(_("  -c, --changelog      view the changelog of a package\n"));
			printf(_("  -e, --orphans        list all packages installed as dependencies but no longer\n"));
			printf(_("                       required by any package\n"));
			printf(_("  -g, --groups         view all members of a package group\n"));
			printf(_("  -i, --info           view package information\n"));
			printf(_("  -l, --list           list the contents of the queried package\n"));
			printf(_("  -m, --foreign        list all packages that were not found in the sync db(s)\n"));
			printf(_("  -o, --owns <file>    query the package that owns <file>\n"));
			printf(_("  -p, --file           query the package file [package] instead of the database\n"));
			printf(_("  -s, --search         search locally-installed packages for matching strings\n"));
			printf(_("  -u, --upgrades       list all packages that can be upgraded\n"));
		} else if(op == PM_OP_SYNC) {
			printf(_("usage:  %s {-S --sync} [options] [package]\n"), myname);
			printf(_("options:\n"));
			printf(_("  -c, --clean          remove old packages from cache directory (-cc for all)\n"));
			printf(_("  -d, --nodeps         skip dependency checks\n"));
			printf(_("  -e, --dependsonly    install dependencies only\n"));
			printf(_("  -f, --force          force install, overwrite conflicting files\n"));
			printf(_("  -g, --groups         view all members of a package group\n"));
			printf(_("  -i, --info           view package information\n"));
			printf(_("  -p, --print-uris     print out URIs for given packages and their dependencies\n"));
			printf(_("  -s, --search         search remote repositories for matching strings\n"));
			printf(_("  -u, --sysupgrade     upgrade all packages that are out of date\n"));
			printf(_("  -w, --downloadonly   download packages but do not install/upgrade anything\n"));
			printf(_("  -y, --refresh        download fresh package databases from the server\n"));
			printf(_("      --ignore <pkg>   ignore a package upgrade (can be used more than once)\n"));
		}
		printf(_("      --config <path>  set an alternate configuration file\n"));
		printf(_("      --noconfirm      do not ask for anything confirmation\n"));
		printf(_("      --ask <number>   pre-specify answers for questions (see manpage)\n"));
		printf(_("      --noprogressbar  do not show a progress bar when downloading files\n"));
		printf(_("      --noscriptlet    do not execute the install scriptlet if there is any\n"));
		printf(_("  -v, --verbose        be verbose\n"));
		printf(_("  -r, --root <path>    set an alternate installation root\n"));
		printf(_("  -b, --dbpath <path>  set an alternate database location\n"));
		printf(_("      --cachedir <dir> set an alternate package cache location\n"));
	}
}

/* Version
 */
static void version()
{
	printf("\n");
	printf(" .--.                  Pacman v%s - libalpm v%s\n", PACKAGE_VERSION, LIB_VERSION);
	printf("/ _.-' .-.  .-.  .-.   Copyright (C) 2002-2006 Judd Vinet <jvinet@zeroflux.org>\n");
	printf("\\  '-. '-'  '-'  '-'\n");
	printf(" '--'                  \n");
	printf(_("                       This program may be freely redistributed under\n"));
	printf(_("                       the terms of the GNU General Public License\n"));
	printf("\n");
}

static void cleanup(int signum)
{
	if(signum==SIGSEGV)
	{
		fprintf(stderr, "Internal pacman error: Segmentation fault\n"
			"Please submit a full bug report, with the given package if appropriate.\n");
		exit(signum);
	} else if((signum == SIGINT) && (alpm_trans_release() == -1)
						&& (pm_errno == PM_ERR_TRANS_COMMITING)) {
		return;
	}
	if(signum != 0) {
		/* TODO why is this here? */
		fprintf(stderr, "\n");
	}

	/* free alpm library resources */
	if(alpm_release() == -1) {
		ERR(NL, "%s\n", alpm_strerror(pm_errno));
	}

	/* free memory */
	FREELIST(pm_targets);
	FREECONF(config);

	/* This fixes up any missing newlines (neednl) */
	MSG(NL, "");

	exit(signum);
}

/* Parse command-line arguments for each operation
 *     argc: argc
 *     argv: argv
 *     
 * Returns: 0 on success, 1 on error
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
		{"upgrades",   no_argument,       0, 'u'},
		{"sysupgrade", no_argument,       0, 'u'},
		{"verbose",    no_argument,       0, 'v'},
		{"downloadonly", no_argument,     0, 'w'},
		{"refresh",    no_argument,       0, 'y'},
		{"noconfirm",  no_argument,       0, 1000},
		{"config",     required_argument, 0, 1001},
		{"ignore",     required_argument, 0, 1002},
		{"debug",      optional_argument, 0, 1003},
		{"noprogressbar",  no_argument,   0, 1004},
		{"noscriptlet", no_argument,      0, 1005},
		{"ask",        required_argument, 0, 1006},
		{"cachedir",   required_argument, 0, 1007},
		{0, 0, 0, 0}
	};
	struct stat st;
	unsigned short logmask;

	while((opt = getopt_long(argc, argv, "ARUFQSTr:b:vkhscVfmnoldepiuwyg", opts, &option_index))) {
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
				#if defined(__OpenBSD__) || defined(__APPLE__)
				config->configfile = strdup(optarg);
				#else
				config->configfile = strndup(optarg, PATH_MAX);
				#endif
				break;
			case 1002: alpm_option_add_ignorepkg(strdup(optarg)); break;
			case 1003:
				/* debug levels are made more 'human readable' than using a raw logmask
				 * here, we will ALWAYS set error and warning for now, though perhaps a
				 * --quiet option will remove these later */
				logmask = PM_LOG_ERROR | PM_LOG_WARNING;

				if(optarg) {
					unsigned short debug = atoi(optarg);
					switch(debug) {
						case 3: logmask |= PM_LOG_FUNCTION; /* fall through */
						case 2: logmask |= PM_LOG_DOWNLOAD; /*fall through */
						case 1: logmask |= PM_LOG_DEBUG; break;
						default:
						  ERR(NL, _("'%s' is not a valid debug level"), optarg);
							return(1);
					}
				} else {
					logmask |= PM_LOG_DEBUG;
				}
				/* progress bars get wonky with debug on, shut them off */
				config->noprogressbar = 1;
				alpm_option_set_logmask(logmask);
				break;
			case 1004: config->noprogressbar = 1; break;
			case 1005: config->flags |= PM_TRANS_FLAG_NOSCRIPTLET; break;
			case 1006: config->noask = 1; config->ask = atoi(optarg); break;
			case 1007:
				if(stat(optarg, &st) == -1 || !S_ISDIR(st.st_mode)) {
					ERR(NL, _("'%s' is not a valid cache directory\n"), optarg);
					return(1);
				}
				alpm_option_set_cachedir(optarg);
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
					ERR(NL, _("'%s' is not a valid db path\n"), optarg);
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
				config->op_q_orphans = 1;
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
					ERR(NL, _("'%s' is not a valid root path\n"), optarg);
					return(1);
				}
				alpm_option_set_root(optarg);
				break;
			case 's':
				config->op_s_search = 1;
				config->op_q_search = 1;
				config->flags |= PM_TRANS_FLAG_RECURSE;
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
		ERR(NL, _("only one operation may be used at a time\n"));
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

int main(int argc, char *argv[])
{
	int ret = 0;
	char *lang = NULL;
#ifndef CYGWIN
	uid_t myuid;
#endif

#if defined(PACMAN_DEBUG) && !defined(CYGWIN) && !defined(BSD)
	/*setenv("MALLOC_TRACE","pacman.mtrace", 0);*/
	mtrace();
#endif
	/* set signal handlers */
	signal(SIGINT, cleanup);
	signal(SIGTERM, cleanup);
	signal(SIGSEGV, cleanup);

	/* i18n init */
	lang = setlocale(LC_ALL, "");
	/* if setlocale returns null, the locale was invalid- override it */
	if (lang == NULL) {
		lang = "C";
		setlocale(LC_ALL, "C");
		setenv("LC_ALL", lang, 1);
		MSG(NL, _("warning: current locale is invalid; using default \"C\" locale"));
	}

	/* workaround for tr_TR */
	if(lang && !strcmp(lang, "tr_TR")) {
		setlocale(LC_CTYPE, "C");
	}
	bindtextdomain("pacman", "/usr/share/locale");
	textdomain("pacman");

	/* init config data */
	config = config_new();
	config->op = PM_OP_MAIN;
	/* disable progressbar if the output is redirected */
	if(!isatty(1)) {
		config->noprogressbar = 1;
	}

	/* initialize pm library */
	if(alpm_initialize() == -1) {
		ERR(NL, _("failed to initilize alpm library (%s)\n"), alpm_strerror(pm_errno));
		cleanup(1);
	}

	/* parse the command line */
	ret = parseargs(argc, argv);
	if(ret != 0) {
		config_free(config);
		exit(ret);
	}

#ifndef CYGWIN
	/* see if we're root or not */
	myuid = geteuid();
#ifndef FAKEROOT
	if(!myuid && getenv("FAKEROOTKEY")) {
		/* fakeroot doesn't count, we're non-root */
		myuid = 99;
	}
#endif

	/* check if we have sufficient permission for the requested operation */
	if(myuid > 0) {
		if(config->op != PM_OP_MAIN && config->op != PM_OP_QUERY && config->op != PM_OP_DEPTEST) {
			if((config->op == PM_OP_SYNC && !config->op_s_sync &&
					(config->op_s_search || config->group || config->op_q_list || config->op_q_info
					 || config->flags & PM_TRANS_FLAG_PRINTURIS))
				 || (config->op == PM_OP_DEPTEST && config->op_d_resolve)
				 || (strcmp(alpm_option_get_root(), PM_ROOT) != 0)) {
				/* special case: PM_OP_SYNC can be used w/ config->op_s_search by any user */
				/* special case: ignore root user check if -r is specified, fall back on
				 * normal FS checking */
			} else {
				ERR(NL, _("you cannot perform this operation unless you are root.\n"));
				config_free(config);
				exit(EXIT_FAILURE);
			}
		}
	}
#endif

	/* Setup logging as soon as possible, to print out maximum debugging info */
	alpm_option_set_logcb(cb_log);

	if(config->configfile == NULL) {
		config->configfile = strdup(PACCONF);
	}

	if(alpm_parse_config(config->configfile, NULL, "") != 0) {
		ERR(NL, _("failed to parse config (%s)\n"), alpm_strerror(pm_errno));
		cleanup(1);
	}

	/* set library parameters */
	alpm_option_set_dlcb(log_progress);

	if(config->verbose > 0) {
		printf("Root  : %s\n", alpm_option_get_root());
		printf("DBPath: %s\n", alpm_option_get_dbpath());
		list_display(_("Targets:"), pm_targets);
	}

	/* Opening local database */
	db_local = alpm_db_register("local");
	if(db_local == NULL) {
		ERR(NL, _("could not register 'local' database (%s)\n"), alpm_strerror(pm_errno));
		cleanup(1);
	}

	if(alpm_list_count(pm_targets) == 0 && !(config->op == PM_OP_QUERY || (config->op == PM_OP_SYNC
	   && (config->op_s_sync || config->op_s_upgrade || config->op_s_clean || config->group 
	   || config->op_q_list)))) {
		ERR(NL, _("no targets specified (use -h for help)\n"));
		cleanup(1);
	}

	/* start the requested operation */
	switch(config->op) {
		case PM_OP_ADD:     ret = pacman_add(pm_targets);     break;
		case PM_OP_REMOVE:  ret = pacman_remove(pm_targets);  break;
		case PM_OP_UPGRADE: ret = pacman_upgrade(pm_targets); break;
		case PM_OP_QUERY:   ret = pacman_query(pm_targets);   break;
		case PM_OP_SYNC:    ret = pacman_sync(pm_targets);    break;
		case PM_OP_DEPTEST: ret = pacman_deptest(pm_targets); break;
		default:
			ERR(NL, _("no operation specified (use -h for help)\n"));
			ret = 1;
	}

	cleanup(ret);
	/* not reached */
	return(0);
}

/* vim: set ts=2 sw=2 noet: */
