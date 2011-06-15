/*
 *  pacman.c
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

/* special handling of package version for GIT */
#if defined(GIT_VERSION)
#undef PACKAGE_VERSION
#define PACKAGE_VERSION GIT_VERSION
#endif

#include <ctype.h> /* isspace */
#include <stdlib.h> /* atoi */
#include <stdio.h>
#include <ctype.h> /* isspace */
#include <limits.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/utsname.h> /* uname */
#include <locale.h> /* setlocale */
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
#include "conf.h"

/* list of targets specified on command line */
static alpm_list_t *pm_targets;

/* Used to sort the options in --help */
static int options_cmp(const void *p1, const void *p2)
{
	const char *s1 = p1;
	const char *s2 = p2;

	if(s1 == s2) return 0;
	if(!s1) return -1;
	if(!s2) return 1;
	/* First skip all spaces in both strings */
	while(isspace((unsigned char)*s1)) {
		s1++;
	}
	while(isspace((unsigned char)*s2)) {
		s2++;
	}
	/* If we compare a long option (--abcd) and a short one (-a),
	 * the short one always wins */
	if(*s1 == '-' && *s2 == '-') {
		s1++;
		s2++;
		if(*s1 == '-' && *s2 == '-') {
			/* two long -> strcmp */
			s1++;
			s2++;
		} else if(*s2 == '-') {
			/* s1 short, s2 long */
			return -1;
		} else if(*s1 == '-') {
			/* s1 long, s2 short */
			return 1;
		}
		/* two short -> strcmp */
	}

	return strcmp(s1, s2);
}

/** Display usage/syntax for the specified operation.
 * @param op     the operation code requested
 * @param myname basename(argv[0])
 */
static void usage(int op, const char * const myname)
{
#define addlist(s) (list = alpm_list_add(list, s))
	alpm_list_t *list = NULL, *i;
	/* prefetch some strings for usage below, which moves a lot of calls
	 * out of gettext. */
	char const * const str_opt = _("options");
	char const * const str_file = _("file(s)");
	char const * const str_pkg = _("package(s)");
	char const * const str_usg = _("usage");
	char const * const str_opr = _("operation");

	/* please limit your strings to 80 characters in width */
	if(op == PM_OP_MAIN) {
		printf("%s:  %s <%s> [...]\n", str_usg, myname, str_opr);
		printf(_("operations:\n"));
		printf("    %s {-h --help}\n", myname);
		printf("    %s {-V --version}\n", myname);
		printf("    %s {-D --database} <%s> <%s>\n", myname, str_opt, str_pkg);
		printf("    %s {-Q --query}    [%s] [%s]\n", myname, str_opt, str_pkg);
		printf("    %s {-R --remove}   [%s] <%s>\n", myname, str_opt, str_pkg);
		printf("    %s {-S --sync}     [%s] [%s]\n", myname, str_opt, str_pkg);
		printf("    %s {-T --deptest}  [%s] [%s]\n", myname, str_opt, str_pkg);
		printf("    %s {-U --upgrade}  [%s] <%s>\n", myname, str_opt, str_file);
		printf(_("\nuse '%s {-h --help}' with an operation for available options\n"),
				myname);
	} else {
		if(op == PM_OP_REMOVE) {
			printf("%s:  %s {-R --remove} [%s] <%s>\n", str_usg, myname, str_opt, str_pkg);
			printf("%s:\n", str_opt);
			addlist(_("  -c, --cascade        remove packages and all packages that depend on them\n"));
			addlist(_("  -n, --nosave         remove configuration files\n"));
			addlist(_("  -s, --recursive      remove unnecessary dependencies\n"
			          "                       (-ss includes explicitly installed dependencies)\n"));
			addlist(_("  -u, --unneeded       remove unneeded packages\n"));
		} else if(op == PM_OP_UPGRADE) {
			printf("%s:  %s {-U --upgrade} [%s] <%s>\n", str_usg, myname, str_opt, str_file);
			printf("%s:\n", str_opt);
		} else if(op == PM_OP_QUERY) {
			printf("%s:  %s {-Q --query} [%s] [%s]\n", str_usg, myname, str_opt, str_pkg);
			printf("%s:\n", str_opt);
			addlist(_("  -c, --changelog      view the changelog of a package\n"));
			addlist(_("  -d, --deps           list packages installed as dependencies [filter]\n"));
			addlist(_("  -e, --explicit       list packages explicitly installed [filter]\n"));
			addlist(_("  -g, --groups         view all members of a package group\n"));
			addlist(_("  -i, --info           view package information (-ii for backup files)\n"));
			addlist(_("  -k, --check          check that the files owned by the package(s) are present\n"));
			addlist(_("  -l, --list           list the contents of the queried package\n"));
			addlist(_("  -m, --foreign        list installed packages not found in sync db(s) [filter]\n"));
			addlist(_("  -o, --owns <file>    query the package that owns <file>\n"));
			addlist(_("  -p, --file <package> query a package file instead of the database\n"));
			addlist(_("  -q, --quiet          show less information for query and search\n"));
			addlist(_("  -s, --search <regex> search locally-installed packages for matching strings\n"));
			addlist(_("  -t, --unrequired     list packages not required by any package [filter]\n"));
			addlist(_("  -u, --upgrades       list outdated packages [filter]\n"));
		} else if(op == PM_OP_SYNC) {
			printf("%s:  %s {-S --sync} [%s] [%s]\n", str_usg, myname, str_opt, str_pkg);
			printf("%s:\n", str_opt);
			addlist(_("  -c, --clean          remove old packages from cache directory (-cc for all)\n"));
			addlist(_("  -g, --groups         view all members of a package group\n"));
			addlist(_("  -i, --info           view package information\n"));
			addlist(_("  -l, --list <repo>    view a list of packages in a repo\n"));
			addlist(_("  -q, --quiet          show less information for query and search\n"));
			addlist(_("  -s, --search <regex> search remote repositories for matching strings\n"));
			addlist(_("  -u, --sysupgrade     upgrade installed packages (-uu allows downgrade)\n"));
			addlist(_("  -w, --downloadonly   download packages but do not install/upgrade anything\n"));
			addlist(_("  -y, --refresh        download fresh package databases from the server\n"));
			addlist(_("      --needed         don't reinstall up to date packages\n"));
		} else if(op == PM_OP_DATABASE) {
			printf("%s:  %s {-D --database} <%s> <%s>\n", str_usg, myname, str_opt, str_pkg);
			printf("%s:\n", str_opt);
			addlist(_("      --asdeps         mark packages as non-explicitly installed\n"));
			addlist(_("      --asexplicit     mark packages as explicitly installed\n"));
		} else if(op == PM_OP_DEPTEST) {
			printf("%s:  %s {-T --deptest} [%s] [%s]\n", str_usg, myname, str_opt, str_pkg);
			printf("%s:\n", str_opt);
		}
		switch(op) {
			case PM_OP_SYNC:
			case PM_OP_UPGRADE:
				addlist(_("  -f, --force          force install, overwrite conflicting files\n"));
				addlist(_("      --asdeps         install packages as non-explicitly installed\n"));
				addlist(_("      --asexplicit     install packages as explicitly installed\n"));
				addlist(_("      --ignore <pkg>   ignore a package upgrade (can be used more than once)\n"));
				addlist(_("      --ignoregroup <grp>\n"
				          "                       ignore a group upgrade (can be used more than once)\n"));
				/* pass through */
			case PM_OP_REMOVE:
				addlist(_("  -d, --nodeps         skip dependency version checks (-dd to skip all checks)\n"));
				addlist(_("  -k, --dbonly         only modify database entries, not package files\n"));
				addlist(_("      --noprogressbar  do not show a progress bar when downloading files\n"));
				addlist(_("      --noscriptlet    do not execute the install scriptlet if one exists\n"));
				addlist(_("      --print          print the targets instead of performing the operation\n"));
				addlist(_("      --print-format <string>\n"
				          "                       specify how the targets should be printed\n"));
				break;
		}

		addlist(_("  -b, --dbpath <path>  set an alternate database location\n"));
		addlist(_("  -r, --root <path>    set an alternate installation root\n"));
		addlist(_("  -v, --verbose        be verbose\n"));
		addlist(_("      --arch <arch>    set an alternate architecture\n"));
		addlist(_("      --cachedir <dir> set an alternate package cache location\n"));
		addlist(_("      --config <path>  set an alternate configuration file\n"));
		addlist(_("      --debug          display debug messages\n"));
		addlist(_("      --gpgdir <path>  set an alternate home directory for GnuPG\n"));
		addlist(_("      --logfile <path> set an alternate log file\n"));
		addlist(_("      --noconfirm      do not ask for any confirmation\n"));
	}
	list = alpm_list_msort(list, alpm_list_count(list), options_cmp);
	for (i = list; i; i = alpm_list_next(i)) {
		printf("%s", (char *)alpm_list_getdata(i));
	}
	alpm_list_free(list);
#undef addlist
}

/** Output pacman version and copyright.
 */
static void version(void)
{
	printf("\n");
	printf(" .--.                  Pacman v%s - libalpm v%s\n", PACKAGE_VERSION, alpm_version());
	printf("/ _.-' .-.  .-.  .-.   Copyright (C) 2006-2011 Pacman Development Team\n");
	printf("\\  '-. '-'  '-'  '-'   Copyright (C) 2002-2006 Judd Vinet\n");
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
	if(!init) {
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
	if(config->handle && alpm_release(config->handle) == -1) {
		pm_printf(PM_LOG_ERROR, "error releasing alpm library\n");
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
	do {
		ret = write(fd, buf, count);
	} while(ret == -1 && errno == EINTR);
	return ret;
}

/** Catches thrown signals. Performs necessary cleanup to ensure database is
 * in a consistant state.
 * @param signum the thrown signal
 */
static void handler(int signum)
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
	} else if(signum == SIGINT) {
		const char *msg = "\nInterrupt signal received\n";
		xwrite(err, msg, strlen(msg));
		if(alpm_trans_interrupt(config->handle) == 0) {
			/* a transaction is being interrupted, don't exit pacman yet. */
			return;
		}
		/* no commiting transaction, we can release it now and then exit pacman */
		alpm_trans_release(config->handle);
		/* output a newline to be sure we clear any line we may be on */
		xwrite(out, "\n", 1);
	}
	cleanup(signum);
}

#define check_optarg() if(!optarg) { return 1; }

static int parsearg_util_addlist(alpm_list_t **list)
{
	alpm_list_t *split, *item;

	check_optarg();
	split = strsplit(optarg, ',');
	for(item = split; item; item = alpm_list_next(item)) {
		*list = alpm_list_add(*list, item->data);
	}
	alpm_list_free(split);
	return 0;
}

/** Helper function for parsing operation from command-line arguments.
 * @param opt Keycode returned by getopt_long
 * @param dryrun If nonzero, application state is NOT changed
 * @return 0 if opt was handled, 1 if it was not handled
 */
static int parsearg_op(int opt, int dryrun)
{
	switch(opt) {
		/* operations */
		case 'D':
			if(dryrun) break;
			config->op = (config->op != PM_OP_MAIN ? 0 : PM_OP_DATABASE); break;
		case 'Q':
			if(dryrun) break;
			config->op = (config->op != PM_OP_MAIN ? 0 : PM_OP_QUERY); break;
		case 'R':
			if(dryrun) break;
			config->op = (config->op != PM_OP_MAIN ? 0 : PM_OP_REMOVE); break;
		case 'S':
			if(dryrun) break;
			config->op = (config->op != PM_OP_MAIN ? 0 : PM_OP_SYNC); break;
		case 'T':
			if(dryrun) break;
			config->op = (config->op != PM_OP_MAIN ? 0 : PM_OP_DEPTEST); break;
		case 'U':
			if(dryrun) break;
			config->op = (config->op != PM_OP_MAIN ? 0 : PM_OP_UPGRADE); break;
		case 'V':
			if(dryrun) break;
			config->version = 1; break;
		case 'h':
			if(dryrun) break;
			config->help = 1; break;
		default:
			return 1;
	}
	return 0;
}

/** Helper functions for parsing command-line arguments.
 * @param opt Keycode returned by getopt_long
 * @return 0 on success, 1 on failure
 */
static int parsearg_global(int opt)
{
	switch(opt) {
		case OP_ARCH:
			check_optarg();
			config_set_arch(strdup(optarg));
			break;
		case OP_ASK:
			check_optarg();
			config->noask = 1;
			config->ask = (unsigned int)atoi(optarg);
			break;
		case OP_CACHEDIR:
			check_optarg();
			config->cachedirs = alpm_list_add(config->cachedirs, strdup(optarg));
			break;
		case OP_CONFIG:
			check_optarg();
			if(config->configfile) {
				free(config->configfile);
			}
			config->configfile = strndup(optarg, PATH_MAX);
			break;
		case OP_DEBUG:
			/* debug levels are made more 'human readable' than using a raw logmask
			 * here, error and warning are set in config_new, though perhaps a
			 * --quiet option will remove these later */
			if(optarg) {
				unsigned short debug = (unsigned short)atoi(optarg);
				switch(debug) {
					case 2:
						config->logmask |= PM_LOG_FUNCTION; /* fall through */
					case 1:
						config->logmask |= PM_LOG_DEBUG;
						break;
					default:
						pm_printf(PM_LOG_ERROR, _("'%s' is not a valid debug level\n"),
								optarg);
						return 1;
				}
			} else {
				config->logmask |= PM_LOG_DEBUG;
			}
			/* progress bars get wonky with debug on, shut them off */
			config->noprogressbar = 1;
			break;
		case OP_GPGDIR:
			config->gpgdir = strdup(optarg);
			break;
		case OP_LOGFILE:
			check_optarg();
			config->logfile = strndup(optarg, PATH_MAX);
			break;
		case OP_NOCONFIRM: config->noconfirm = 1; break;
		case 'b':
			check_optarg();
			config->dbpath = strdup(optarg);
			break;
		case 'r': check_optarg(); config->rootdir = strdup(optarg); break;
		case 'v': (config->verbose)++; break;
		default: return 1;
	}
	return 0;
}

static int parsearg_database(int opt)
{
	switch(opt) {
		case OP_ASDEPS: config->flags |= PM_TRANS_FLAG_ALLDEPS; break;
		case OP_ASEXPLICIT: config->flags |= PM_TRANS_FLAG_ALLEXPLICIT; break;
		default: return 1;
	}
	return 0;
}

static int parsearg_query(int opt)
{
	switch(opt) {
		case 'c': config->op_q_changelog = 1; break;
		case 'd': config->op_q_deps = 1; break;
		case 'e': config->op_q_explicit = 1; break;
		case 'g': (config->group)++; break;
		case 'i': (config->op_q_info)++; break;
		case 'k': config->op_q_check = 1; break;
		case 'l': config->op_q_list = 1; break;
		case 'm': config->op_q_foreign = 1; break;
		case 'o': config->op_q_owns = 1; break;
		case 'p': config->op_q_isfile = 1; break;
		case 'q': config->quiet = 1; break;
		case 's': config->op_q_search = 1; break;
		case 't': config->op_q_unrequired = 1; break;
		case 'u': config->op_q_upgrade = 1; break;
		default: return 1;
	}
	return 0;
}

/* options common to -S -R -U */
static int parsearg_trans(int opt)
{
	switch(opt) {
		case 'd':
			if(config->flags & PM_TRANS_FLAG_NODEPVERSION) {
				config->flags |= PM_TRANS_FLAG_NODEPS;
			} else {
				config->flags |= PM_TRANS_FLAG_NODEPVERSION;
			}
			break;
		case 'k': config->flags |= PM_TRANS_FLAG_DBONLY; break;
		case OP_NOPROGRESSBAR: config->noprogressbar = 1; break;
		case OP_NOSCRIPTLET: config->flags |= PM_TRANS_FLAG_NOSCRIPTLET; break;
		case 'p': config->print = 1; break;
		case OP_PRINTFORMAT:
			check_optarg();
			config->print_format = strdup(optarg);
			break;
		default: return 1;
	}
	return 0;
}

static int parsearg_remove(int opt)
{
	if(parsearg_trans(opt) == 0)
		return 0;
	switch(opt) {
		case 'c': config->flags |= PM_TRANS_FLAG_CASCADE; break;
		case 'n': config->flags |= PM_TRANS_FLAG_NOSAVE; break;
		case 's':
			if(config->flags & PM_TRANS_FLAG_RECURSE) {
				config->flags |= PM_TRANS_FLAG_RECURSEALL;
			} else {
				config->flags |= PM_TRANS_FLAG_RECURSE;
			}
			break;
		case 'u': config->flags |= PM_TRANS_FLAG_UNNEEDED; break;
		default: return 1;
	}
	return 0;
}

/* options common to -S -U */
static int parsearg_upgrade(int opt)
{
	if(parsearg_trans(opt) == 0)
		return 0;
	switch(opt) {
		case 'f': config->flags |= PM_TRANS_FLAG_FORCE; break;
		case OP_ASDEPS: config->flags |= PM_TRANS_FLAG_ALLDEPS; break;
		case OP_ASEXPLICIT: config->flags |= PM_TRANS_FLAG_ALLEXPLICIT; break;
		case OP_IGNORE:
			parsearg_util_addlist(&(config->ignorepkg));
			break;
		case OP_IGNOREGROUP:
			parsearg_util_addlist(&(config->ignoregrp));
			break;
		default: return 1;
	}
	return 0;
}

static int parsearg_sync(int opt)
{
	if(parsearg_upgrade(opt) == 0)
		return 0;
	switch(opt) {
		case OP_NEEDED: config->flags |= PM_TRANS_FLAG_NEEDED; break;
		case 'c': (config->op_s_clean)++; break;
		case 'g': (config->group)++; break;
		case 'i': (config->op_s_info)++; break;
		case 'l': config->op_q_list = 1; break;
		case 'q': config->quiet = 1; break;
		case 's': config->op_s_search = 1; break;
		case 'u': (config->op_s_upgrade)++; break;
		case 'w':
			config->op_s_downloadonly = 1;
			config->flags |= PM_TRANS_FLAG_DOWNLOADONLY;
			config->flags |= PM_TRANS_FLAG_NOCONFLICTS;
			break;
		case 'y': (config->op_s_sync)++; break;
		default: return 1;
	}
	return 0;
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
	int result;
	const char *optstring = "DQRSTUVb:cdefghiklmnopqr:stuvwy";
	static struct option opts[] =
	{
		{"database",   no_argument,       0, 'D'},
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
		{"check",      no_argument,       0, 'k'},
		{"list",       no_argument,       0, 'l'},
		{"foreign",    no_argument,       0, 'm'},
		{"nosave",     no_argument,       0, 'n'},
		{"owns",       no_argument,       0, 'o'},
		{"file",       no_argument,       0, 'p'},
		{"print",      no_argument,       0, 'p'},
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
		{"noconfirm",  no_argument,       0, OP_NOCONFIRM},
		{"config",     required_argument, 0, OP_CONFIG},
		{"ignore",     required_argument, 0, OP_IGNORE},
		{"debug",      optional_argument, 0, OP_DEBUG},
		{"noprogressbar", no_argument,    0, OP_NOPROGRESSBAR},
		{"noscriptlet", no_argument,      0, OP_NOSCRIPTLET},
		{"ask",        required_argument, 0, OP_ASK},
		{"cachedir",   required_argument, 0, OP_CACHEDIR},
		{"asdeps",     no_argument,       0, OP_ASDEPS},
		{"logfile",    required_argument, 0, OP_LOGFILE},
		{"ignoregroup", required_argument, 0, OP_IGNOREGROUP},
		{"needed",     no_argument,       0, OP_NEEDED},
		{"asexplicit",     no_argument,   0, OP_ASEXPLICIT},
		{"arch",       required_argument, 0, OP_ARCH},
		{"print-format", required_argument, 0, OP_PRINTFORMAT},
		{"gpgdir",     required_argument, 0, OP_GPGDIR},
		{0, 0, 0, 0}
	};

	/* parse operation */
	while((opt = getopt_long(argc, argv, optstring, opts, &option_index))) {
		if(opt < 0) {
			break;
		} else if(opt == 0) {
			continue;
		} else if(opt == '?') {
			/* unknown option, getopt printed an error */
			return 1;
		}
		parsearg_op(opt, 0);
	}

	if(config->op == 0) {
		pm_printf(PM_LOG_ERROR, _("only one operation may be used at a time\n"));
		return 1;
	}
	if(config->help) {
		usage(config->op, mbasename(argv[0]));
		return 2;
	}
	if(config->version) {
		version();
		return 2;
	}

	/* parse all other options */
	optind = 1;
	while((opt = getopt_long(argc, argv, optstring, opts, &option_index))) {
		if(opt < 0) {
			break;
		} else if(opt == 0) {
			continue;
		} else if(opt == '?') {
			/* this should have failed during first pass already */
			return 1;
		} else if(parsearg_op(opt, 1) == 0) {
			/* opt is an operation */
			continue;
		}

		switch(config->op) {
			case PM_OP_DATABASE:
				result = parsearg_database(opt);
				break;
			case PM_OP_QUERY:
				result = parsearg_query(opt);
				break;
			case PM_OP_REMOVE:
				result = parsearg_remove(opt);
				break;
			case PM_OP_SYNC:
				result = parsearg_sync(opt);
				break;
			case PM_OP_UPGRADE:
				result = parsearg_upgrade(opt);
				break;
			case PM_OP_DEPTEST:
			default:
				result = 1;
				break;
		}
		if(result == 0) {
			continue;
		}

		/* fall back to global options */
		result = parsearg_global(opt);
		if(result != 0) {
			/* global option parsing failed, abort */
			pm_printf(PM_LOG_ERROR, _("invalid option\n"));
			return result;
		}
	}

	while(optind < argc) {
		/* add the target to our target array */
		pm_targets = alpm_list_add(pm_targets, strdup(argv[optind]));
		optind++;
	}

	return 0;
}

/** print commandline to logfile
 */
static void cl_to_log(int argc, char* argv[])
{
	size_t size = 0;
	int i;
	for(i = 0; i<argc; i++) {
		size += strlen(argv[i]) + 1;
	}
	char *cl_text = malloc(size);
	if(!cl_text) {
		return;
	}
	char *p = cl_text;
	for(i = 0; i<argc-1; i++) {
		strcpy(p, argv[i]);
		p += strlen(argv[i]);
		*p++ = ' ';
	}
	strcpy(p, argv[i]);
	alpm_logaction(config->handle, "Running '%s'\n", cl_text);
	free(cl_text);
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

	/* we support reading targets from stdin if a cmdline parameter is '-' */
	if(!isatty(fileno(stdin)) && alpm_list_find_str(pm_targets, "-")) {
		char line[PATH_MAX];
		int i = 0;

		/* remove the '-' from the list */
		pm_targets = alpm_list_remove_str(pm_targets, "-", NULL);

		while(i < PATH_MAX && (line[i] = (char)fgetc(stdin)) != EOF) {
			if(isspace((unsigned char)line[i])) {
				/* avoid adding zero length arg when multiple spaces separate args */
				if(i > 0) {
					line[i] = '\0';
					pm_targets = alpm_list_add(pm_targets, strdup(line));
					i = 0;
				}
			} else {
				i++;
			}
		}
		/* check for buffer overflow */
		if(i >= PATH_MAX) {
			pm_printf(PM_LOG_ERROR, _("buffer overflow detected in arg parsing\n"));
			cleanup(EXIT_FAILURE);
		}

		/* end of stream -- check for data still in line buffer */
		if(i > 0) {
			line[i] = '\0';
			pm_targets = alpm_list_add(pm_targets, strdup(line));
		}
		if(!freopen(ctermid(NULL), "r", stdin)) {
			pm_printf(PM_LOG_ERROR, _("failed to reopen stdin for reading: (%s)\n"),
					strerror(errno));
		}
	}

	/* parse the config file */
	ret = parseconfig(config->configfile);
	if(ret != 0) {
		cleanup(ret);
	}

	/* noask is meant to be non-interactive */
	if(config->noask) {
		config->noconfirm = 1;
	}

	/* set up the print operations */
	if(config->print && !config->op_s_clean) {
		config->noconfirm = 1;
		config->flags |= PM_TRANS_FLAG_NOCONFLICTS;
		config->flags |= PM_TRANS_FLAG_NOLOCK;
		/* Display only errors */
		config->logmask &= ~PM_LOG_WARNING;
	}

#if defined(HAVE_GETEUID) && !defined(CYGWIN)
	/* check if we have sufficient permission for the requested operation */
	if(myuid > 0 && needs_root()) {
		pm_printf(PM_LOG_ERROR, _("you cannot perform this operation unless you are root.\n"));
		cleanup(EXIT_FAILURE);
	}
#endif

	if(config->verbose > 0) {
		alpm_list_t *i;
		printf("Root      : %s\n", alpm_option_get_root(config->handle));
		printf("Conf File : %s\n", config->configfile);
		printf("DB Path   : %s\n", alpm_option_get_dbpath(config->handle));
		printf("Cache Dirs: ");
		for(i = alpm_option_get_cachedirs(config->handle); i; i = alpm_list_next(i)) {
			printf("%s  ", (char *)alpm_list_getdata(i));
		}
		printf("\n");
		printf("Lock File : %s\n", alpm_option_get_lockfile(config->handle));
		printf("Log File  : %s\n", alpm_option_get_logfile(config->handle));
		printf("GPG Dir   : %s\n", alpm_option_get_gpgdir(config->handle));
		list_display("Targets   :", pm_targets);
	}

	/* Log commandline */
	if(needs_root()) {
		cl_to_log(argc, argv);
	}

	/* start the requested operation */
	switch(config->op) {
		case PM_OP_DATABASE:
			ret = pacman_database(pm_targets);
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
	return EXIT_SUCCESS;
}

/* vim: set ts=2 sw=2 noet: */
