/*
 *  trans.c
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <math.h>
#include <libintl.h>

#include <alpm.h>
/* pacman */
#include "util.h"
#include "log.h"
#include "trans.h"
#include "list.h"
#include "conf.h"

#define LOG_STR_LEN 256

extern config_t *config;
extern unsigned int maxcols;

int prevpercent=0; /* for less progressbar output */

/* Callback to handle transaction events
 */
void cb_trans_evt(unsigned char event, void *data1, void *data2)
{
	char str[LOG_STR_LEN] = "";
	char out[PATH_MAX];
	int i;

	switch(event) {
		case PM_TRANS_EVT_CHECKDEPS_START:
			pm_fprintf(stderr, NL, _("checking dependencies... "));
		break;
		case PM_TRANS_EVT_FILECONFLICTS_START:
			if(config->noprogressbar) {
			MSG(NL, _("checking for file conflicts... "));
			}
		break;
		case PM_TRANS_EVT_RESOLVEDEPS_START:
			pm_fprintf(stderr, NL, _("resolving dependencies... "));
		break;
		case PM_TRANS_EVT_INTERCONFLICTS_START:
			pm_fprintf(stderr, NL, _("looking for inter-conflicts... "));
		break;
		case PM_TRANS_EVT_FILECONFLICTS_DONE:
			if(config->noprogressbar) {
				MSG(CL, _("done.\n"));
			} else {
				MSG(NL, "");
			}
		break;
		case PM_TRANS_EVT_CHECKDEPS_DONE:
		case PM_TRANS_EVT_RESOLVEDEPS_DONE:
		case PM_TRANS_EVT_INTERCONFLICTS_DONE:
			pm_fprintf(stderr, CL, _("done.\n"));
		break;
		case PM_TRANS_EVT_EXTRACT_DONE:
			if(!config->noprogressbar) {
				MSG(NL, "");
			}
		break;
		case PM_TRANS_EVT_ADD_START:
			if(config->noprogressbar) {
				MSG(NL, _("installing %s... "), (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME));
			}
		break;
		case PM_TRANS_EVT_ADD_DONE:
			if(config->noprogressbar) {
				MSG(CL, _("done.\n"));
			}
			snprintf(str, LOG_STR_LEN, _("installed %s (%s)"),
			         (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME),
			         (char *)alpm_pkg_getinfo(data1, PM_PKG_VERSION));
			alpm_logaction(str);
		break;
		case PM_TRANS_EVT_REMOVE_START:
			if(config->noprogressbar) {
			MSG(NL, _("removing %s... "), (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME));
			}
		break;
		case PM_TRANS_EVT_REMOVE_DONE:
			if(config->noprogressbar) {
			    MSG(CL, _("done.\n"));
			} else {
			    MSG(NL, "");
			}
			snprintf(str, LOG_STR_LEN, _("removed %s (%s)"),
			         (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME),
			         (char *)alpm_pkg_getinfo(data1, PM_PKG_VERSION));
			alpm_logaction(str);
		break;
		case PM_TRANS_EVT_UPGRADE_START:
			if(config->noprogressbar) {
				MSG(NL, _("upgrading %s... "), (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME));
			}
		break;
		case PM_TRANS_EVT_UPGRADE_DONE:
			if(config->noprogressbar) {
				MSG(CL, _("done.\n"));
			}
			snprintf(str, LOG_STR_LEN, _("upgraded %s (%s -> %s)"),
			         (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME),
			         (char *)alpm_pkg_getinfo(data2, PM_PKG_VERSION),
			         (char *)alpm_pkg_getinfo(data1, PM_PKG_VERSION));
			alpm_logaction(str);
		break;
		case PM_TRANS_EVT_INTEGRITY_START:
			MSG(NL, _("checking package integrity... "));
		break;
		case PM_TRANS_EVT_INTEGRITY_DONE:
			MSG(CL, _("done.\n"));
		break;
		case PM_TRANS_EVT_SCRIPTLET_INFO:
			MSG(NL, "%s\n", (char*)data1);
		break;
		case PM_TRANS_EVT_SCRIPTLET_START:
			MSG(NL, (char*)data1);
			MSG(CL, "...");
		break;
		case PM_TRANS_EVT_SCRIPTLET_DONE:
			if(!(long)data1) {
				MSG(CL, _(" done.\n"));
			} else {
				MSG(CL, _(" failed.\n"));
			}
		break;
		case PM_TRANS_EVT_PRINTURI:
			MSG(NL, "%s%s\n", (char*)data1, (char*)data2);
		break;
		case PM_TRANS_EVT_RETRIEVE_START:
			MSG(NL, _("\n:: Retrieving packages from %s...\n"), (char*)data1);
			fflush(stdout);
		break;
		case PM_TRANS_EVT_RETRIEVE_LOCAL:
			MSG(NL, " %s [", (char*)data1);
			STRNCPY(out, (char*)data2, maxcols-42);
			MSG(CL, "%s", out);
			for(i = strlen(out); i < maxcols-43; i++) {
				MSG(CL, " ");
			}
			fputs(_("] 100%    LOCAL "), stdout);
		break;
	}
}

void cb_trans_conv(unsigned char event, void *data1, void *data2, void *data3, int *response)
{
	char str[LOG_STR_LEN] = "";

	switch(event) {
		case PM_TRANS_CONV_INSTALL_IGNOREPKG:
			if(config->noask) {
				if(config->ask & PM_TRANS_CONV_INSTALL_IGNOREPKG) {
					*response = 1;
				} else {
					*response = 0;
				}
			} else {
				snprintf(str, LOG_STR_LEN, _(":: %s requires %s, but it is in IgnorePkg. Install anyway? [Y/n] "),
				         (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME),
				         (char *)alpm_pkg_getinfo(data2, PM_PKG_NAME));
				*response = yesno(str);
			}
		break;
		case PM_TRANS_CONV_REMOVE_HOLDPKG:
			if(config->noask) {
				if(config->ask & PM_TRANS_CONV_REMOVE_HOLDPKG) {
					*response = 1;
				} else {
					*response = 0;
				}
			} else {
				snprintf(str, LOG_STR_LEN, _(":: %s is designated as a HoldPkg.  Remove anyway? [Y/n] "),
				         (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME));
				*response = yesno(str);
			}
		break;
		case PM_TRANS_CONV_REPLACE_PKG:
			if(config->noask) {
				if(config->ask & PM_TRANS_CONV_REPLACE_PKG) {
					*response = 1;
				} else {
					*response = 0;
				}
			} else {
				snprintf(str, LOG_STR_LEN, _(":: Replace %s with %s/%s? [Y/n] "),
				         (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME),
				         (char *)data3,
				         (char *)alpm_pkg_getinfo(data2, PM_PKG_NAME));
				*response = yesno(str);
			}
		break;
		case PM_TRANS_CONV_CONFLICT_PKG:
			if(config->noask) {
				if(config->ask & PM_TRANS_CONV_CONFLICT_PKG) {
					*response = 1;
				} else {
					*response = 0;
				}
			} else {
				snprintf(str, LOG_STR_LEN, _(":: %s conflicts with %s. Remove %s? [Y/n] "),
				         (char *)data1,
				         (char *)data2,
				         (char *)data2);
				*response = yesno(str);
			}
		break;
		case PM_TRANS_CONV_LOCAL_NEWER:
			if(config->noask) {
				if(config->ask & PM_TRANS_CONV_LOCAL_NEWER) {
					*response = 1;
				} else {
					*response = 0;
				}
			} else {
				if(!config->op_s_downloadonly) {
					snprintf(str, LOG_STR_LEN, _(":: %s-%s: local version is newer. Upgrade anyway? [Y/n] "),
				         (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME),
				         (char *)alpm_pkg_getinfo(data1, PM_PKG_VERSION));
					*response = yesno(str);
				} else {
					*response = 1;
				}
			}
		break;
		case PM_TRANS_CONV_LOCAL_UPTODATE:
			if(config->noask) {
				if(config->ask & PM_TRANS_CONV_LOCAL_UPTODATE) {
					*response = 1;
				} else {
					*response = 0;
				}
			} else {
				if(!config->op_s_downloadonly) {
					snprintf(str, LOG_STR_LEN, _(":: %s-%s: local version is up to date. Upgrade anyway? [Y/n] "),
				         (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME),
				         (char *)alpm_pkg_getinfo(data1, PM_PKG_VERSION));
					*response = yesno(str);
				} else {
					*response = 1;
				}
			}
		break;
		case PM_TRANS_CONV_CORRUPTED_PKG:
			if(config->noask) {
				if(config->ask & PM_TRANS_CONV_CORRUPTED_PKG) {
					*response = 1;
				} else {
					*response = 0;
				}
			} else {
				if(!config->noconfirm) {
					snprintf(str, LOG_STR_LEN, _(":: Archive %s is corrupted. Do you want to delete it? [Y/n] "),
				         (char *)data1);
					*response = yesno(str);
				} else {
					*response = 1;
				}
			}
		break;
	}
}

void cb_trans_progress(unsigned char event, char *pkgname, int percent, int howmany, int remain)
{
	int i, hash;
	unsigned int maxpkglen, progresslen = maxcols - 57;
	char *addstr, *upgstr, *removestr, *conflictstr, *ptr;
	addstr = strdup(_("installing"));
	upgstr = strdup(_("upgrading"));
	removestr = strdup(_("removing"));
	conflictstr = strdup(_("checking for file conflicts"));

	if(config->noprogressbar) {
		return;
	}

	if (!pkgname)
		return;
	if (percent > 100)
		return;
	if(percent == prevpercent)
		return;

	prevpercent=percent;
	switch (event) {
		case PM_TRANS_PROGRESS_ADD_START:
			ptr = addstr;
		break;

		case PM_TRANS_PROGRESS_UPGRADE_START:
			ptr = upgstr;
		break;

		case PM_TRANS_PROGRESS_REMOVE_START:
			ptr = removestr;
		break;

		case PM_TRANS_PROGRESS_CONFLICTS_START:
			ptr = conflictstr;
		break;
	}
	hash=percent*progresslen/100;

	// if the package name is too long, then slice the ending
	maxpkglen=46-strlen(ptr)-(3+2*(int)log10(howmany));
	if(strlen(pkgname)>maxpkglen)
		pkgname[maxpkglen]='\0';

	switch (event) {
	case PM_TRANS_PROGRESS_ADD_START:
	case PM_TRANS_PROGRESS_UPGRADE_START:
	case PM_TRANS_PROGRESS_REMOVE_START:
		putchar('(');
		for(i=0;i<(int)log10(howmany)-(int)log10(remain);i++)
			putchar(' ');
		printf("%d/%d) %s %s ", remain, howmany, ptr, pkgname);
		if (strlen(pkgname)<maxpkglen)
			for (i=maxpkglen-strlen(pkgname)-1; i>0; i--)
				putchar(' ');
		break;

	case PM_TRANS_PROGRESS_CONFLICTS_START:
		printf("%s (", ptr);
		for(i=0;i<(int)log10(howmany)-(int)log10(remain);i++)
			putchar(' ');
		printf("%d/%d) ", remain, howmany);
		for (i=maxpkglen; i>0; i--)
			putchar(' ');
		break;
	}

	printf("[");
	for (i = progresslen; i > 0; i--) {
		if (i >= progresslen - hash)
			printf("#");
		else
			printf("-");
	}
	MSG(CL, "] %3d%%\r", percent);
	FREE(addstr);
	FREE(upgstr);
	FREE(removestr);
}

/* vim: set ts=2 sw=2 noet: */
