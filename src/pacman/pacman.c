/*
 *  pacman.c
 * 
 *  Copyright (c) 2002-2005 by Judd Vinet <jvinet@zeroflux.org>
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

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <mcheck.h> /* debug */

#include <alpm.h>
/* pacman */
#include "list.h"
#include "util.h"
#include "download.h"
#include "conf.h"
#include "package.h"
#include "add.h"
#include "remove.h"
#include "upgrade.h"
#include "query.h"
#include "sync.h"
#include "pacman.h"

/* command line options */
char *pmo_root       = NULL;
char *pmo_dbpath     = NULL;
char *pmo_configfile = NULL;
unsigned short pmo_op         = PM_OP_MAIN;
unsigned short pmo_verbose    = 0;
unsigned short pmo_version    = 0;
unsigned short pmo_help       = 0;
unsigned short pmo_upgrade    = 0;
unsigned short pmo_noconfirm  = 0;
unsigned short pmo_d_vertest  = 0;
unsigned short pmo_d_resolve  = 0;
unsigned short pmo_q_isfile   = 0;
unsigned short pmo_q_info     = 0;
unsigned short pmo_q_list     = 0;
unsigned short pmo_q_orphans  = 0;
unsigned short pmo_q_owns     = 0;
unsigned short pmo_q_search   = 0;
unsigned short pmo_s_upgrade  = 0;
unsigned short pmo_s_downloadonly = 0;
unsigned short pmo_s_printuris    = 0;
unsigned short pmo_s_sync     = 0;
unsigned short pmo_s_search   = 0;
unsigned short pmo_s_clean    = 0;
unsigned short pmo_group      = 0;
unsigned char  pmo_flags      = 0;
/* configuration file option */
list_t        *pmo_holdpkg      = NULL;
char          *pmo_proxyhost    = NULL;
unsigned short pmo_proxyport    = 0;
char          *pmo_xfercommand  = NULL;
unsigned short pmo_nopassiveftp = 0;

PM_DB *db_local;
/* list of (sync_t *) structs for sync locations */
list_t *pmc_syncs = NULL;
/* list of targets specified on command line */
list_t *pm_targets  = NULL;

int maxcols = 80;

int neednl = 0; /* for cleaner message output */

int main(int argc, char *argv[])
{
	int ret = 0;
	char *cenv = NULL;

	/* debug */
	mtrace();

	cenv = getenv("COLUMNS");
	if(cenv != NULL) {
		maxcols = atoi(cenv);
	}

	if(argc < 2) {
		usage(PM_OP_MAIN, basename(argv[0]));
		return(0);
	}

	/* set signal handlers */
	signal(SIGINT, cleanup);
	signal(SIGTERM, cleanup);

	/* parse the command line */
	ret = parseargs(argc, argv);
	if(ret != 0) {
		exit(ret);
	}

	if(pmo_root == NULL) {
		pmo_root = strdup("/");
	}

	/* initialize pm library */
	if(alpm_initialize(pmo_root) == -1) {
		ERR(NL, "failed to initilize alpm library (%s)\n", alpm_strerror(pm_errno));
		cleanup(1);
	}

	/* add a trailing '/' if there isn't one */
	if(pmo_root[strlen(pmo_root)-1] != '/') {
		char *ptr;
		MALLOC(ptr, strlen(pmo_root)+2);
		strcpy(ptr, pmo_root);
		strcat(ptr, "/");
		FREE(pmo_root);
		pmo_root = ptr;
	}

	if(pmo_configfile == NULL) {
		pmo_configfile = strdup(PACCONF);
	}
	if(parseconfig(pmo_configfile) == -1) {
		cleanup(1);
	}
	if(pmo_dbpath == NULL) {
		pmo_dbpath = strdup("var/lib/pacman");
	}

	/* set library parameters */
	if(pmo_verbose == 1) {
	if(alpm_set_option(PM_OPT_LOGMASK, (long)0xFF) == -1) {
		ERR(NL, "failed to set option LOGMASK (%s)\n", alpm_strerror(pm_errno));
		cleanup(1);
	}
	if(alpm_set_option(PM_OPT_LOGCB, (long)cb_log) == -1) {
		ERR(NL, "failed to set option LOGCB (%s)\n", alpm_strerror(pm_errno));
		cleanup(1);
	}
	}
	if(alpm_set_option(PM_OPT_DBPATH, (long)pmo_dbpath) == -1) {
		ERR(NL, "failed to set option DBPATH (%s)\n", alpm_strerror(pm_errno));
		cleanup(1);
	}
	
	if(pmo_verbose > 1) {
		printf("Root  : %s\n", pmo_root);
		printf("DBPath: %s\n", pmo_dbpath);
		list_display("Targets:", pm_targets);
	}

	/* Opening local database */
	if(alpm_db_register("local", &db_local) == -1) {
		ERR(NL, "could not register 'local' database (%s)\n", alpm_strerror(pm_errno));
		cleanup(1);
	}

	/* start the requested operation */
	switch(pmo_op) {
		case PM_OP_ADD:     ret = pacman_add(pm_targets);     break;
		case PM_OP_REMOVE:  ret = pacman_remove(pm_targets);  break;
		case PM_OP_UPGRADE: ret = pacman_upgrade(pm_targets); break;
		case PM_OP_QUERY:   ret = pacman_query(pm_targets);   break;
		case PM_OP_SYNC:    ret = pacman_sync(pm_targets);    break;
		case PM_OP_DEPTEST: ret = pacman_deptest(pm_targets); break;
		case PM_OP_MAIN:    ret = 0; break;
		default:
			ERR(NL, "no operation specified (use -h for help)\n\n");
			ret = 1;
	}

	cleanup(ret);
	/* not reached */
	return(0);
}

void cleanup(int signum)
{
	list_t *lp;

	/* free pm library resources */
	if(alpm_release() == -1) {
		ERR(NL, "%s\n", alpm_strerror(pm_errno));
	}

	/* free memory */
	for(lp = pmc_syncs; lp; lp = lp->next) {
		sync_t *sync = lp->data;
		list_t *i;
		for(i = sync->servers; i; i = i->next) {
			server_t *server = i->data;
			FREE(server->protocol);
			FREE(server->server);
			FREE(server->path);
		}
		FREELIST(sync->servers);
		FREE(sync->treename);
	}
	FREELIST(pmc_syncs);
	FREE(pmo_root);
	FREE(pmo_dbpath);
	FREE(pmo_configfile);
	FREE(pmo_proxyhost);
	FREE(pmo_xfercommand);

	FREELIST(pm_targets);

	/* debug */
	muntrace();

	fflush(stdout);

	exit(signum);
}

/* Callback to handle notifications from the library
 */
void cb_log(unsigned short level, char *msg)
{
	char str[8] = "";

	switch(level) {
		case PM_LOG_DEBUG:
			sprintf(str, "DEBUG");
		break;
		case PM_LOG_ERROR:
			sprintf(str, "ERROR");
		break;
		case PM_LOG_WARNING:
			sprintf(str, "WARNING");
		break;
		case PM_LOG_FLOW1:
			sprintf(str, "FLOW1");
		break;
		case PM_LOG_FLOW2:
			sprintf(str, "FLOW2");
		break;
		case PM_LOG_FUNCTION:
			sprintf(str, "FUNCTION");
		break;
		default:
			sprintf(str, "???");
		break;
	}

	if(strlen(str) > 0) {
		MSG(NL, "%s: %s\n", str, msg);
	}
}

/* Callback to handle transaction events
 */
void cb_trans(unsigned short event, void *data1, void *data2)
{
	char str[256] = "";

	switch(event) {
		case PM_TRANS_EVT_DEPS_START:
			MSG(NL, "checking dependencies... ");
		break;
		case PM_TRANS_EVT_CONFLICTS_START:
			MSG(NL, "checking for file conflicts... ");
		break;
		case PM_TRANS_EVT_DEPS_DONE:
		case PM_TRANS_EVT_CONFLICTS_DONE:
			MSG(CL, "done.\n");
		break;
		case PM_TRANS_EVT_ADD_START:
			MSG(NL, "installing %s... ", (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME));
		break;
		case PM_TRANS_EVT_ADD_DONE:
			MSG(CL, "done.\n");
			snprintf(str, 256, "installed %s (%s)",
			                   (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME),
			                   (char *)alpm_pkg_getinfo(data1, PM_PKG_VERSION));
			alpm_logaction(str);
		break;
		case PM_TRANS_EVT_REMOVE_START:
			MSG(NL, "removing %s... ", (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME));
		break;
		case PM_TRANS_EVT_REMOVE_DONE:
			MSG(CL, "done.\n");
			snprintf(str, 256, "removed %s (%s)",
			         (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME),
			         (char *)alpm_pkg_getinfo(data1, PM_PKG_VERSION));
			alpm_logaction(str);
		break;
		case PM_TRANS_EVT_UPGRADE_START:
			MSG(NL, "upgrading %s... ", (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME));
		break;
		case PM_TRANS_EVT_UPGRADE_DONE:
			MSG(CL, "done.\n");
			snprintf(str, 256, "upgraded %s (%s -> %s)",
			                   (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME),
			                   (char *)alpm_pkg_getinfo(data1, PM_PKG_VERSION),
			                   (char *)alpm_pkg_getinfo(data2, PM_PKG_VERSION));
			alpm_logaction(str);
		break;
	}
}

int pacman_deptest(list_t *targets)
{
	PM_LIST *lp, *data;
	PM_PKG *dummy;

	if(pmo_d_vertest) {
		/* ORE
		if(targets && targets->data && targets->next && targets->next->data) {
			int ret = rpmvercmp(targets->data, targets->next->data);
			printf("%d\n", ret);
			return(ret);
		}*/
		return(0);
	}

	/* we create a transaction to hold a dummy package to be able to use
	 * pm_trans_prepare() */
	/* ORE
	trans = alpm_trans_new(PM_TRANS_ADD, 0);
	if(trans == NULL) {
		FREEPKG(dummy);
		ERR(NL, "error: can't allocate %d bytes\n", sizeof(pm_pkginfo_t));
		exit(1);
	}

	dummy = (pm_pkginfo_t *)malloc(sizeof(pm_pkginfo_t));
	if(dummy == NULL) {
		ERR(NL, "error: can't allocate %d bytes\n", sizeof(pm_pkginfo_t));
		exit(1);
	}
	sprintf(dummy->name, "_dummy_");
	sprintf(dummy->version, "1.0-1");

	for(i = targets; i; i = i->next) {
		if(i->data == NULL) continue;
		dummy->depends = list_add(dummy->depends, strdup(i->data)));
	}

	trans->targets = list_add(trans->targets, strdup(dummy->name));*/

	if(alpm_trans_prepare(&data) == -1) {
		int ret = 126;
		list_t *synctargs = NULL;
		switch(pm_errno) {
			case PM_ERR_UNSATISFIED_DEPS:
				for(lp = alpm_list_first(data); lp; lp = alpm_list_next(lp)) {
					pmdepmissing_t *miss = alpm_list_getdata(lp);
					if(!pmo_d_resolve) {
						MSG(NL, "requires: %s", miss->depend.name);
						switch(miss->depend.mod) {
							case PM_DEP_EQ: MSG(CL, "=%s", miss->depend.version);  break;
							case PM_DEP_GE: MSG(CL, ">=%s", miss->depend.version); break;
							case PM_DEP_LE: MSG(CL, "<=%s", miss->depend.version); break;
						}
						MSG(CL, "\n");
					}
					synctargs = list_add(synctargs, strdup(miss->depend.name));
				}
				alpm_list_free(data);
			break;
			case PM_ERR_CONFLICTING_DEPS:
				/* we can't auto-resolve conflicts */
				for(lp = alpm_list_first(data); lp; lp = alpm_list_next(lp)) {
					pmdepmissing_t *miss = alpm_list_getdata(lp);
					MSG(NL, "conflict: %s", miss->depend.name);
				}
				ret = 127;
				alpm_list_free(data);
			break;
			default:
				ret = 127;
			break;
		}
		/* attempt to resolve missing dependencies */
		/* TODO: handle version comparators (eg, glibc>=2.2.5) */
		if(ret == 126 && synctargs != NULL) {
			if(!pmo_d_resolve || pacman_sync(synctargs) != 0) {
				/* error (or -D not used) */
				ret = 127;
			}
		}
		FREELIST(synctargs);
		FREEPKG(dummy);
		return(ret);
	}

	FREEPKG(dummy);

	return(0);
}

/* Parse command-line arguments for each operation
 *     argc: argc
 *     argv: argv
 *     
 * Returns: 0 on success, 1 on error
 */
int parseargs(int argc, char **argv)
{
	int opt;
	int option_index = 0;
	static struct option opts[] =
	{
		{"add",        no_argument,       0, 'A'},
		{"remove",     no_argument,       0, 'R'},
		{"upgrade",    no_argument,       0, 'U'},
		{"freshen",    no_argument,       0, 'F'},
		{"query",      no_argument,       0, 'Q'},
		{"sync",       no_argument,       0, 'S'},
		{"deptest",    no_argument,       0, 'T'},
		{"vertest",    no_argument,       0, 'Y'},
		{"resolve",    no_argument,       0, 'D'},
		{"root",       required_argument, 0, 'r'},
		{"dbpath",     required_argument, 0, 'b'},
		{"verbose",    no_argument,       0, 'v'},
		{"version",    no_argument,       0, 'V'},
		{"help",       no_argument,       0, 'h'},
		{"search",     no_argument,       0, 's'},
		{"clean",      no_argument,       0, 'c'},
		{"force",      no_argument,       0, 'f'},
		{"nodeps",     no_argument,       0, 'd'},
		{"orphans",    no_argument,       0, 'e'},
		{"nosave",     no_argument,       0, 'n'},
		{"owns",       no_argument,       0, 'o'},
		{"list",       no_argument,       0, 'l'},
		{"file",       no_argument,       0, 'p'},
		{"info",       no_argument,       0, 'i'},
		{"sysupgrade", no_argument,       0, 'u'},
		{"downloadonly", no_argument,     0, 'w'},
		{"refresh",    no_argument,       0, 'y'},
		{"dbonly",     no_argument,       0, 'k'},
		{"cascade",    no_argument,       0, 'c'},
		{"recursive",  no_argument,       0, 's'},
		{"groups",     no_argument,       0, 'g'},
		{"noconfirm",  no_argument,       0, 1000},
		{"config",     required_argument, 0, 1001},
		{0, 0, 0, 0}
	};
	char root[256];

	while((opt = getopt_long(argc, argv, "ARUFQSTDYr:b:vkhscVfnoldepiuwyg", opts, &option_index))) {
		if(opt < 0) {
			break;
		}
		switch(opt) {
			case 0:   break;
			case 1000: pmo_noconfirm = 1; break;
			case 1001: pmo_configfile = strndup(optarg, PATH_MAX); break;
			case 'A': pmo_op = (pmo_op != PM_OP_MAIN ? 0 : PM_OP_ADD);     break;
			case 'R': pmo_op = (pmo_op != PM_OP_MAIN ? 0 : PM_OP_REMOVE);  break;
			case 'U': pmo_op = (pmo_op != PM_OP_MAIN ? 0 : PM_OP_UPGRADE); break;
			case 'F': pmo_op = (pmo_op != PM_OP_MAIN ? 0 : PM_OP_UPGRADE); pmo_flags |= PM_TRANS_FLAG_FRESHEN; break;
			case 'Q': pmo_op = (pmo_op != PM_OP_MAIN ? 0 : PM_OP_QUERY);   break;
			case 'S': pmo_op = (pmo_op != PM_OP_MAIN ? 0 : PM_OP_SYNC);    break;
			case 'T': pmo_op = (pmo_op != PM_OP_MAIN ? 0 : PM_OP_DEPTEST); break;
			case 'Y': pmo_op = (pmo_op != PM_OP_MAIN ? 0 : PM_OP_DEPTEST); pmo_d_vertest = 1; break;
			case 'D': pmo_op = (pmo_op != PM_OP_MAIN ? 0 : PM_OP_DEPTEST); pmo_d_resolve = 1; break;
			case 'h': pmo_help = 1; break;
			case 'V': pmo_version = 1; break;
			case 'b': pmo_dbpath = strdup(optarg); break;
			case 'c': pmo_s_clean++; pmo_flags |= PM_TRANS_FLAG_CASCADE; break;
			case 'd': pmo_flags |= PM_TRANS_FLAG_NODEPS; break;
			case 'e': pmo_q_orphans = 1; break;
			case 'f': pmo_flags |= PM_TRANS_FLAG_FORCE; break;
			case 'g': pmo_group = 1; break;
			case 'i': pmo_q_info++; break;
			case 'k': pmo_flags |= PM_TRANS_FLAG_DBONLY; break;
			case 'l': pmo_q_list = 1; break;
			case 'n': pmo_flags |= PM_TRANS_FLAG_NOSAVE; break;
			case 'p': pmo_q_isfile = 1; break;
			case 'o': pmo_q_owns = 1; break;
			case 'r':
				if(realpath(optarg, root) == NULL) {
					perror("bad root path");
				return(1);
				}
				pmo_root = strdup(root);
			break;
			case 's': pmo_s_search = 1; pmo_q_search = 1; pmo_flags |= PM_TRANS_FLAG_RECURSE; break;
			case 'u': pmo_s_upgrade = 1; break;
			case 'v': pmo_verbose++; break;
			case 'w': pmo_s_downloadonly = 1; break;
			case 'y': pmo_s_sync = 1; break;
			case '?': return(1);
			default:  return(1);
		}
	}

	if(pmo_op == 0) {
		ERR(NL, "only one operation may be used at a time\n\n");
		return(1);
	}

	if(pmo_help) {
		usage(pmo_op, basename(argv[0]));
		return(2);
	}
	if(pmo_version) {
		version();
		return(2);
	}

	while(optind < argc) {
		/* add the target to our target array */
		char *s = strdup(argv[optind]);
		pm_targets = list_add(pm_targets, s);
		optind++;
	}

	return(0);
}

/* Display usage/syntax for the specified operation.
 *     op:     the operation code requested
 *     myname: basename(argv[0])
 */
void usage(int op, char *myname)
{
	if(op == PM_OP_MAIN) {
		printf("usage:  %s {-h --help}\n", myname);
		printf("        %s {-V --version}\n", myname);
		printf("        %s {-A --add}     [options] <file>\n", myname);
		printf("        %s {-R --remove}  [options] <package>\n", myname);
		printf("        %s {-U --upgrade} [options] <file>\n", myname);
		printf("        %s {-F --freshen} [options] <file>\n", myname);
		printf("        %s {-Q --query}   [options] [package]\n", myname);
		printf("        %s {-S --sync}    [options] [package]\n", myname);
		printf("\nuse '%s --help' with other options for more syntax\n\n", myname);
	} else {
		if(op == PM_OP_ADD) {
			printf("usage:  %s {-A --add} [options] <file>\n", myname);
			printf("options:\n");
			printf("  -d, --nodeps        skip dependency checks\n");
			printf("  -f, --force         force install, overwrite conflicting files\n");
		} else if(op == PM_OP_REMOVE) {
			printf("usage:  %s {-R --remove} [options] <package>\n", myname);
			printf("options:\n");
			printf("  -c, --cascade       remove packages and all packages that depend on them\n");
			printf("  -d, --nodeps        skip dependency checks\n");
			printf("  -k, --dbonly        only remove database entry, do not remove files\n");
			printf("  -n, --nosave        remove configuration files as well\n");
			printf("  -s, --recursive     remove dependencies also (that won't break packages)\n");
		} else if(op == PM_OP_UPGRADE) {
			if(pmo_flags & PM_TRANS_FLAG_FRESHEN) {
				printf("usage:  %s {-F --freshen} [options] <file>\n", myname);
			} else {
				printf("usage:  %s {-U --upgrade} [options] <file>\n", myname);
			}
			printf("options:\n");
			printf("  -d, --nodeps        skip dependency checks\n");
			printf("  -f, --force         force install, overwrite conflicting files\n");
		} else if(op == PM_OP_QUERY) {
			printf("usage:  %s {-Q --query} [options] [package]\n", myname);
			printf("options:\n");
			printf("  -e, --orphans       list all packages that were explicitly installed\n");
			printf("                      and are not required by any other packages\n");
			printf("  -g, --groups        view all members of a package group\n");
			printf("  -i, --info          view package information\n");
			printf("  -l, --list          list the contents of the queried package\n");
			printf("  -o, --owns <file>   query the package that owns <file>\n");
			printf("  -p, --file          pacman will query the package file [package] instead of\n");
			printf("                      looking in the database\n");
			printf("  -s, --search        search locally-installed packages for matching strings\n");
		} else if(op == PM_OP_SYNC) {
			printf("usage:  %s {-S --sync} [options] [package]\n", myname);
			printf("options:\n");
			printf("  -c, --clean         remove old packages from cache directory (use -cc for all)\n");
			printf("  -d, --nodeps        skip dependency checks\n");
			printf("  -f, --force         force install, overwrite conflicting files\n");
			printf("  -g, --groups        view all members of a package group\n");
			printf("  -s, --search        search remote repositories for matching strings\n");
			printf("  -u, --sysupgrade    upgrade all packages that are out of date\n");
			printf("  -w, --downloadonly  download packages but do not install/upgrade anything\n");
			printf("  -y, --refresh       download fresh package databases from the server\n");
		}
		printf("      --config <path> set an alternate configuration file\n");
		printf("      --noconfirm     do not ask for anything confirmation\n");
		printf("  -v, --verbose       be verbose\n");
		printf("  -r, --root <path>   set an alternate installation root\n");
		printf("  -b, --dbpath <path> set an alternate database location\n");
	}
}

/* Version
 */
void version()
{
	printf("\n");
	printf(" .--.                  Pacman v%s\n", PM_VERSION);
	printf("/ _.-' .-.  .-.  .-.   Copyright (C) 2002-2003 Judd Vinet <jvinet@zeroflux.org>\n");
	printf("\\  '-. '-'  '-'  '-'  \n");
	printf(" '--'                  This program may be freely redistributed under\n");
	printf("                       the terms of the GNU General Public License\n\n");
}

/*
 * Misc functions
 */

/* Condense a list of strings into one long (space-delimited) string
 */
char *buildstring(list_t *strlist)
{
	char *str;
	int size = 1;
	list_t *lp;

	for(lp = strlist; lp; lp = lp->next) {
		size += strlen(lp->data) + 1;
	}
	str = (char *)malloc(size);
	if(str == NULL) {
		ERR(NL, "failed to allocated %d bytes\n", size);
	}
	str[0] = '\0';
	for(lp = strlist; lp; lp = lp->next) {
		strcat(str, lp->data);
		strcat(str, " ");
	}
	/* shave off the last space */
	str[strlen(str)-1] = '\0';

	return(str);
}

/* Check verbosity option and, if set, print the
 * string to stdout
 */
void vprint(char *fmt, ...)
{
	va_list args;

	if(pmo_verbose > 1) {
		if(neednl == 1) {
			fprintf(stdout, "\n");
			neednl = 0;
		}
		va_start(args, fmt);
		pm_fprintf(stdout, NL, fmt, args);
		va_end(args);
	}
}

void pm_fprintf(FILE *file, unsigned short line, char *fmt, ...)
{
	va_list args;

	char str[256];

	if(neednl == 1 && line == NL) {
		fprintf(stdout, "\n");
		neednl = 0;
	}

	va_start(args, fmt);
	vsnprintf(str, 256, fmt, args);
	va_end(args);

	fprintf(file, str);
	fflush(file);

	neednl = (str[strlen(str)-1] == 10) ? 0 : 1;
}

/* vim: set ts=2 sw=2 noet: */
