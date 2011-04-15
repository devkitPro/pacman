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
#include <sys/stat.h>
#include <sys/utsname.h> /* uname */
#include <locale.h> /* setlocale */
#include <errno.h>
#include <glob.h>
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

static void setarch(const char *arch)
{
	if(strcmp(arch, "auto") == 0) {
		struct utsname un;
		uname(&un);
		pm_printf(PM_LOG_DEBUG, "config: Architecture: %s\n", un.machine);
		alpm_option_set_arch(un.machine);
	} else {
		pm_printf(PM_LOG_DEBUG, "config: Architecture: %s\n", arch);
		alpm_option_set_arch(arch);
	}
}

/** Free the resources.
 *
 * @param ret the return value
 */
static void cleanup(int ret) {
	/* free alpm library resources */
	if(alpm_release() == -1) {
		pm_printf(PM_LOG_ERROR, "%s\n", alpm_strerrorlast());
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
	if(!init) {
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

		/* Set GnuPG's home directory.  This is not relative to rootdir, even if
		 * rootdir is defined. Reasoning: gpgdir contains configuration data. */
		if(config->gpgdir) {
			ret = alpm_option_set_signaturedir(config->gpgdir);
			if(ret != 0) {
				pm_printf(PM_LOG_ERROR, _("problem setting gpgdir '%s' (%s)\n"),
						config->gpgdir, alpm_strerrorlast());
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

#define check_optarg() if(!optarg) { return 1; }

typedef int (*fn_add) (const char *s);

static int parsearg_util_addlist(fn_add fn)
{
	alpm_list_t *list = NULL, *item = NULL; /* lists for splitting strings */

	check_optarg();
	list = strsplit(optarg, ',');
	for(item = list; item; item = alpm_list_next(item)) {
		fn((char *)alpm_list_getdata(item));
	}
	FREELIST(list);
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
		case OP_ARCH: check_optarg(); setarch(optarg); break;
		case OP_ASK:
			check_optarg();
			config->noask = 1;
			config->ask = (unsigned int)atoi(optarg);
			break;
		case OP_CACHEDIR:
			check_optarg();
			if(alpm_option_add_cachedir(optarg) != 0) {
				pm_printf(PM_LOG_ERROR, _("problem adding cachedir '%s' (%s)\n"),
						optarg, alpm_strerrorlast());
				return 1;
			}
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
			parsearg_util_addlist(alpm_option_add_ignorepkg);
			break;
		case OP_IGNOREGROUP:
			parsearg_util_addlist(alpm_option_add_ignoregrp);
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

/* helper for being used with setrepeatingoption */
static int option_add_holdpkg(const char *name) {
	config->holdpkg = alpm_list_add(config->holdpkg, strdup(name));
	return 0;
}

/* helper for being used with setrepeatingoption */
static int option_add_syncfirst(const char *name) {
	config->syncfirst = alpm_list_add(config->syncfirst, strdup(name));
	return 0;
}

/* helper for being used with setrepeatingoption */
static int option_add_cleanmethod(const char *value) {
	if(strcmp(value, "KeepInstalled") == 0) {
		config->cleanmethod |= PM_CLEAN_KEEPINST;
	} else if(strcmp(value, "KeepCurrent") == 0) {
		config->cleanmethod |= PM_CLEAN_KEEPCUR;
	} else {
		pm_printf(PM_LOG_ERROR, _("invalid value for 'CleanMethod' : '%s'\n"),
				value);
	}
	return 0;
}

/** Add repeating options such as NoExtract, NoUpgrade, etc to libalpm
 * settings. Refactored out of the parseconfig code since all of them did
 * the exact same thing and duplicated code.
 * @param ptr a pointer to the start of the multiple options
 * @param option the string (friendly) name of the option, used for messages
 * @param optionfunc a function pointer to an alpm_option_add_* function
 */
static void setrepeatingoption(char *ptr, const char *option,
		int (*optionfunc)(const char *))
{
	char *q;

	while((q = strchr(ptr, ' '))) {
		*q = '\0';
		(*optionfunc)(ptr);
		pm_printf(PM_LOG_DEBUG, "config: %s: %s\n", option, ptr);
		ptr = q;
		ptr++;
	}
	(*optionfunc)(ptr);
	pm_printf(PM_LOG_DEBUG, "config: %s: %s\n", option, ptr);
}

static char *get_filename(const char *url) {
	char *filename = strrchr(url, '/');
	if(filename != NULL) {
		filename++;
	}
	return filename;
}

static char *get_destfile(const char *path, const char *filename) {
	char *destfile;
	/* len = localpath len + filename len + null */
	size_t len = strlen(path) + strlen(filename) + 1;
	destfile = calloc(len, sizeof(char));
	snprintf(destfile, len, "%s%s", path, filename);

	return destfile;
}

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
		pm_printf(PM_LOG_ERROR, _("could not change directory to %s (%s)\n"), cwd, strerror(errno));
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

static int _parse_options(const char *key, char *value,
		const char *file, int linenum)
{
	if(value == NULL) {
		/* options without settings */
		if(strcmp(key, "UseSyslog") == 0) {
			alpm_option_set_usesyslog(1);
			pm_printf(PM_LOG_DEBUG, "config: usesyslog\n");
		} else if(strcmp(key, "ILoveCandy") == 0) {
			config->chomp = 1;
			pm_printf(PM_LOG_DEBUG, "config: chomp\n");
		} else if(strcmp(key, "VerbosePkgLists") == 0) {
			config->verbosepkglists = 1;
			pm_printf(PM_LOG_DEBUG, "config: verbosepkglists\n");
		} else if(strcmp(key, "UseDelta") == 0) {
			alpm_option_set_usedelta(1);
			pm_printf(PM_LOG_DEBUG, "config: usedelta\n");
		} else if(strcmp(key, "TotalDownload") == 0) {
			config->totaldownload = 1;
			pm_printf(PM_LOG_DEBUG, "config: totaldownload\n");
		} else if(strcmp(key, "CheckSpace") == 0) {
			alpm_option_set_checkspace(1);
		} else {
			pm_printf(PM_LOG_WARNING,
					_("config file %s, line %d: directive '%s' in section '%s' not recognized.\n"),
					file, linenum, key, "options");
		}
	} else {
		/* options with settings */
		if(strcmp(key, "NoUpgrade") == 0) {
			setrepeatingoption(value, "NoUpgrade", alpm_option_add_noupgrade);
		} else if(strcmp(key, "NoExtract") == 0) {
			setrepeatingoption(value, "NoExtract", alpm_option_add_noextract);
		} else if(strcmp(key, "IgnorePkg") == 0) {
			setrepeatingoption(value, "IgnorePkg", alpm_option_add_ignorepkg);
		} else if(strcmp(key, "IgnoreGroup") == 0) {
			setrepeatingoption(value, "IgnoreGroup", alpm_option_add_ignoregrp);
		} else if(strcmp(key, "HoldPkg") == 0) {
			setrepeatingoption(value, "HoldPkg", option_add_holdpkg);
		} else if(strcmp(key, "SyncFirst") == 0) {
			setrepeatingoption(value, "SyncFirst", option_add_syncfirst);
		} else if(strcmp(key, "Architecture") == 0) {
			if(!alpm_option_get_arch()) {
				setarch(value);
			}
		} else if(strcmp(key, "DBPath") == 0) {
			/* don't overwrite a path specified on the command line */
			if(!config->dbpath) {
				config->dbpath = strdup(value);
				pm_printf(PM_LOG_DEBUG, "config: dbpath: %s\n", value);
			}
		} else if(strcmp(key, "CacheDir") == 0) {
			if(alpm_option_add_cachedir(value) != 0) {
				pm_printf(PM_LOG_ERROR, _("problem adding cachedir '%s' (%s)\n"),
						value, alpm_strerrorlast());
				return 1;
			}
			pm_printf(PM_LOG_DEBUG, "config: cachedir: %s\n", value);
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
			alpm_option_set_fetchcb(download_with_xfercommand);
			pm_printf(PM_LOG_DEBUG, "config: xfercommand: %s\n", value);
		} else if(strcmp(key, "CleanMethod") == 0) {
			setrepeatingoption(value, "CleanMethod", option_add_cleanmethod);
		} else if(strcmp(key, "VerifySig") == 0) {
			if(strcmp(value, "Always") == 0) {
				alpm_option_set_default_sigverify(PM_PGP_VERIFY_ALWAYS);
			} else if(strcmp(value, "Optional") == 0) {
				alpm_option_set_default_sigverify(PM_PGP_VERIFY_OPTIONAL);
			} else if(strcmp(value, "Never") == 0) {
				alpm_option_set_default_sigverify(PM_PGP_VERIFY_NEVER);
			} else {
				pm_printf(PM_LOG_ERROR, _("invalid value for 'VerifySig' : '%s'\n"), value);
				return 1;
			}
			pm_printf(PM_LOG_DEBUG, "config: setting default VerifySig: %s\n", value);
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
	const char *arch = alpm_option_get_arch();
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

	if(alpm_db_setserver(db, server) != 0) {
		/* pm_errno is set by alpm_db_setserver */
		pm_printf(PM_LOG_ERROR, _("could not add server URL to database '%s': %s (%s)\n"),
				dbname, server, alpm_strerrorlast());
		free(server);
		return 1;
	}

	free(server);
	return 0;
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
		return 1;
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
			/* if we are not looking at the options section, register a db */
			if(strcmp(section, "options") != 0) {
				db = alpm_db_register_sync(section);
				if(db == NULL) {
					pm_printf(PM_LOG_ERROR, _("could not register '%s' database (%s)\n"),
							section, alpm_strerrorlast());
					ret = 1;
					goto cleanup;
				}
			}
			continue;
		}

		/* directive */
		char *key, *value;
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
		if(section == NULL) {
			pm_printf(PM_LOG_ERROR, _("config file %s, line %d: All directives must belong to a section.\n"),
					file, linenum);
			ret = 1;
			goto cleanup;
		}
		/* Include is allowed in both options and repo sections */
		if(strcmp(key, "Include") == 0) {
			if(value == NULL) {
				pm_printf(PM_LOG_ERROR, _("config file %s, line %d: directive '%s' needs a value\n"),
						file, linenum, key);
				ret = 1;
				goto cleanup;
			}
			/* Ignore include failures... assume non-critical */
			int globret;
			glob_t globbuf;
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
					for(size_t gindex = 0; gindex < globbuf.gl_pathc; gindex++) {
						pm_printf(PM_LOG_DEBUG, "config file %s, line %d: including %s\n",
								file, linenum, globbuf.gl_pathv[gindex]);
						_parseconfig(globbuf.gl_pathv[gindex], section, db);
					}
				break;
			}
			globfree(&globbuf);
			continue;
		}
		if(strcmp(section, "options") == 0) {
			/* we are either in options ... */
			if((ret = _parse_options(key, value, file, linenum)) != 0) {
				goto cleanup;
			}
		} else {
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
				if(strcmp(value, "Always") == 0) {
					ret = alpm_db_set_pgp_verify(db, PM_PGP_VERIFY_ALWAYS);
				} else if(strcmp(value, "Optional") == 0) {
					ret = alpm_db_set_pgp_verify(db, PM_PGP_VERIFY_OPTIONAL);
				} else if(strcmp(value, "Never") == 0) {
					ret = alpm_db_set_pgp_verify(db, PM_PGP_VERIFY_NEVER);
				} else {
					pm_printf(PM_LOG_ERROR, _("invalid value for 'VerifySig' : '%s'\n"), value);
					ret = 1;
					goto cleanup;
				}
				if(ret != 0) {
					pm_printf(PM_LOG_ERROR, _("could not add pgp verify option to database '%s': %s (%s)\n"),
							alpm_db_get_name(db), value, alpm_strerrorlast());
					goto cleanup;
				}
				pm_printf(PM_LOG_DEBUG, "config: VerifySig for %s: %s\n",alpm_db_get_name(db), value);
			} else {
				pm_printf(PM_LOG_WARNING,
						_("config file %s, line %d: directive '%s' in section '%s' not recognized.\n"),
						file, linenum, key, section);
			}
		}

	}

cleanup:
	fclose(fp);
	if(section){
		free(section);
	}
	/* call setlibpaths here to ensure we have called it at least once */
	setlibpaths();
	pm_printf(PM_LOG_DEBUG, "config: finished parsing %s\n", file);
	return ret;
}

/** Parse a configuration file.
 * @param file path to the config file.
 * @return 0 on success, non-zero on error
 */
static int parseconfig(const char *file)
{
	/* call the real parseconfig function with a null section & db argument */
	return _parseconfig(file, NULL, NULL);
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
	if(!cl_text)
		return;
	char *p = cl_text;
	for(i = 0; i<argc-1; i++) {
		strcpy(p, argv[i]);
		p += strlen(argv[i]);
		*p++ = ' ';
	}
	strcpy(p, argv[i]);
	alpm_logaction("Running '%s'\n", cl_text);
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
	alpm_option_set_signaturedir(GPGDIR);
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

	/* set TotalDownload callback if option enabled */
	if(config->totaldownload) {
		alpm_option_set_totaldlcb(cb_dl_total);
	}

	/* noask is meant to be non-interactive */
	if(config->noask) {
		config->noconfirm = 1;
	}

	/* set up the print operations */
	if(config->print) {
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
		printf("Root      : %s\n", alpm_option_get_root());
		printf("Conf File : %s\n", config->configfile);
		printf("DB Path   : %s\n", alpm_option_get_dbpath());
		printf("Cache Dirs: ");
		for(i = alpm_option_get_cachedirs(); i; i = alpm_list_next(i)) {
			printf("%s  ", (char *)alpm_list_getdata(i));
		}
		printf("\n");
		printf("Lock File : %s\n", alpm_option_get_lockfile());
		printf("Log File  : %s\n", alpm_option_get_logfile());
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
