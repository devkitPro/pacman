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
#include <mcheck.h> /* debug tracing (mtrace) */
#include <time.h>

/* alpm */
#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "pacman.h"
#include "util.h"
#include "callback.h"
#include "conf.h"
#include "package.h"

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

/**
 * @brief Display usage/syntax for the specified operation.
 *
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
			printf(_("  -u, --upgrades       list all packages that can be upgraded\n"));
			printf(_("  -z, --showsize       display installed size of each package\n"));
		} else if(op == PM_OP_SYNC) {
			printf("%s:  %s {-S --sync} [%s] [%s]\n", str_usg, myname, str_opt, str_pkg);
			printf("%s:\n", str_opt);
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
			printf(_("  -z, --showsize       display download size of each package\n"));
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

/**
 * @brief Output pacman version and copyright.
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

/**
 * @brief Sets up gettext localization.
 *        Safe to call multiple times.
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

/**
 * @brief Set user agent environment variable.
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

/**
 * @brief Catches thrown signals.
 *        Performs necessary cleanup to ensure database is in a consistant
 *        state.
 *
 * @param signum the thrown signal
 */
static void cleanup(int signum)
{
	if(signum==SIGSEGV)
	{
		/* write a log message and write to stderr */
		cb_log(PM_LOG_ERROR, "segmentation fault");
		fprintf(stderr, "Internal pacman error: Segmentation fault.\n"
		        "Please submit a full bug report with --debug if appropriate.\n");
		exit(signum);
	} else if((signum == SIGINT) && (alpm_trans_release() == -1)
						&& (pm_errno == PM_ERR_TRANS_COMMITING)) {
		return;
	}

	/* free alpm library resources */
	if(alpm_release() == -1) {
		fprintf(stderr, _("error: %s\n"), alpm_strerror(pm_errno));
	}

	/* free memory */
	FREELIST(pm_targets);
	if(config) {
		config_free(config);
		config = NULL;
	}

	exit(signum);
}

/**
 * @brief Parse command-line arguments for each operation
 *
 * @param argc argc
 * @param argv argv
 *
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
		{"upgrades",   no_argument,       0, 'u'},
		{"sysupgrade", no_argument,       0, 'u'},
		{"verbose",    no_argument,       0, 'v'},
		{"downloadonly", no_argument,     0, 'w'},
		{"refresh",    no_argument,       0, 'y'},
		{"showsize",   no_argument,       0, 'z'},
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

	while((opt = getopt_long(argc, argv, "ARUFQSTr:b:vkhscVfmnoldepiuwygz", opts, &option_index))) {
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
				 * here, we will ALWAYS set error and warning for now, though perhaps a
				 * --quiet option will remove these later */
				logmask = PM_LOG_ERROR | PM_LOG_WARNING;

				if(optarg) {
					unsigned short debug = atoi(optarg);
					switch(debug) {
						case 3:
							logmask |= PM_LOG_FUNCTION; /* fall through */
						case 2:
							logmask |= PM_LOG_DOWNLOAD; /*fall through */
						case 1:
							logmask |= PM_LOG_DEBUG;
							break;
						default:
						  fprintf(stderr, _("error: '%s' is not a valid debug level"),
							        optarg);
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
					fprintf(stderr, _("error: '%s' is not a valid cache directory\n"),
					        optarg);
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
					fprintf(stderr, _("error: '%s' is not a valid db path\n"),
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
					fprintf(stderr, _("error: '%s' is not a valid root path\n"),
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
			case 'z': config->showsize = 1; break;
			case '?': return(1);
			default: return(1);
		}
	}

	if(config->op == 0) {
		fprintf(stderr, _("error: only one operation may be used at a time\n"));
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

/**
 * @brief Main function.
 *
 * @param argc argc
 * @param argv argv
 *
 * @return A return code indicating success, failure, etc.
 */
int main(int argc, char *argv[])
{
	int ret = 0;
	/* may not work with CYGWIN */
	uid_t myuid;

#if defined(PACMAN_DEBUG)
	/* need to ensure we have mcheck installed */
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
	config->op = PM_OP_MAIN;

	/* disable progressbar if the output is redirected */
	if(!isatty(1)) {
		config->noprogressbar = 1;
	}

	/* initialize pm library */
	if(alpm_initialize() == -1) {
		fprintf(stderr, _("error: failed to initialize alpm library (%s)\n"),
		        alpm_strerror(pm_errno));
		cleanup(1);
	}

	/* parse the command line */
	ret = parseargs(argc, argv);
	if(ret != 0) {
		config_free(config);
		exit(ret);
	}

	/* see if we're root or not */
	myuid = geteuid();

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
				fprintf(stderr, _("error: you cannot perform this operation unless you are root.\n"));
				config_free(config);
				exit(EXIT_FAILURE);
			}
		}
	}

	/* Setup logging as soon as possible, to print out maximum debugging info */
	alpm_option_set_logcb(cb_log);

	if(config->configfile == NULL) {
		config->configfile = strdup(PM_ROOT PM_CONF);
	}

	if(alpm_parse_config(config->configfile, NULL, "") != 0) {
		fprintf(stderr, _("error: failed to parse config (%s)\n"),
		        alpm_strerror(pm_errno));
		cleanup(1);
	}

	/* set library parameters */
	alpm_option_set_dlcb(cb_dl_progress);

	if(config->verbose > 0) {
		printf("Root     : %s\n", alpm_option_get_root());
		printf("DBPath   : %s\n", alpm_option_get_dbpath());
		printf("CacheDir : %s\n", alpm_option_get_cachedir());
		list_display(_("Targets  :"), pm_targets);
	}

	/* Opening local database */
	db_local = alpm_db_register("local");
	if(db_local == NULL) {
		fprintf(stderr, _("error: could not register 'local' database (%s)\n"),
		        alpm_strerror(pm_errno));
		cleanup(1);
	}

	/* TODO This is pretty messy, shouldn't checking be done later in the ops
	 * themselves? I can't even digest this if statement. */
	if(alpm_list_count(pm_targets) == 0
			&& !(config->op == PM_OP_QUERY
					|| (config->op == PM_OP_SYNC
							&& (config->op_s_sync || config->op_s_upgrade || config->op_s_search
								  || config->op_s_clean || config->group
									|| config->op_q_list)))) {
		fprintf(stderr, _("error: no targets specified (use -h for help)\n"));
		cleanup(1);
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
			fprintf(stderr, _("error: no operation specified (use -h for help)\n"));
			ret = 1;
	}

	cleanup(ret);
	/* not reached */
	return(0);
}

/* vim: set ts=2 sw=2 noet: */
