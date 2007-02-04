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
#include <libintl.h>

#include <alpm.h>
/* pacman */
#include "util.h"
#include "log.h"
#include "trans.h"
#include "conf.h"

#define LOG_STR_LEN 256

extern config_t *config;

static int prevpercent=0; /* for less progressbar output */

/* refactored function from cb_trans_evt */
static void retrieve_local(void *data1, void *data2)
{
	const unsigned int maxcols = getcols();
	char out[PATH_MAX];
	unsigned int i;

	MSG(NL, " %s [", (char*)data1);
	STRNCPY(out, (char*)data2, maxcols-42);
	MSG(CL, "%s", out);
	for(i = strlen(out); i < maxcols-43; i++) {
		MSG(CL, " ");
	}
	fputs(_("] 100%    LOCAL "), stdout);
}

/* Callback to handle transaction events
 */
void cb_trans_evt(pmtransevt_t event, void *data1, void *data2)
{
	char str[LOG_STR_LEN] = "";

	switch(event) {
		case PM_TRANS_EVT_CHECKDEPS_START:
			pm_fprintf(stderr, NL, _("checking dependencies... "));
			break;
		case PM_TRANS_EVT_FILECONFLICTS_START:
			if(config->noprogressbar) {
			MSG(NL, _("checking for file conflicts... "));
			}
			break;
		case PM_TRANS_EVT_CLEANUP_START:
			pm_fprintf(stderr, NL, _("cleaning up... "));
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
			}
			break;
		case PM_TRANS_EVT_CHECKDEPS_DONE:
		case PM_TRANS_EVT_CLEANUP_DONE:
		case PM_TRANS_EVT_RESOLVEDEPS_DONE:
		case PM_TRANS_EVT_INTERCONFLICTS_DONE:
			pm_fprintf(stderr, CL, _("done.\n"));
			break;
		case PM_TRANS_EVT_EXTRACT_DONE:
			if(config->noprogressbar) {
				MSG(CL, _("done.\n"));
			}
			break;
		case PM_TRANS_EVT_ADD_START:
			if(config->noprogressbar) {
				MSG(NL, _("installing %s... "), alpm_pkg_get_name(data1));
			}
			break;
		case PM_TRANS_EVT_ADD_DONE:
			if(config->noprogressbar) {
				MSG(CL, _("done.\n"));
			}
			snprintf(str, LOG_STR_LEN, _("installed %s (%s)"),
			         alpm_pkg_get_name(data1),
			         alpm_pkg_get_version(data1));
			alpm_logaction(str);
			break;
		case PM_TRANS_EVT_REMOVE_START:
			if(config->noprogressbar) {
			MSG(NL, _("removing %s... "), alpm_pkg_get_name(data1));
			}
			break;
		case PM_TRANS_EVT_REMOVE_DONE:
			if(config->noprogressbar) {
			    MSG(CL, _("done.\n"));
			}
			snprintf(str, LOG_STR_LEN, _("removed %s (%s)"),
			         alpm_pkg_get_name(data1),
			         alpm_pkg_get_version(data1));
			alpm_logaction(str);
			break;
		case PM_TRANS_EVT_UPGRADE_START:
			if(config->noprogressbar) {
				MSG(NL, _("upgrading %s... "), alpm_pkg_get_name(data1));
			}
			break;
		case PM_TRANS_EVT_UPGRADE_DONE:
			if(config->noprogressbar) {
				MSG(CL, _("done.\n"));
			}
			snprintf(str, LOG_STR_LEN, _("upgraded %s (%s -> %s)"),
			         (char *)alpm_pkg_get_name(data1),
			         (char *)alpm_pkg_get_version(data2),
			         (char *)alpm_pkg_get_version(data1));
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
			MSG(NL, "%s/%s\n", (char*)data1, (char*)data2);
			break;
		case PM_TRANS_EVT_RETRIEVE_START:
			MSG(NL, _(":: Retrieving packages from %s...\n"), (char*)data1);
			fflush(stdout);
			break;
		case PM_TRANS_EVT_RETRIEVE_LOCAL:
			retrieve_local(data1, data2);
			break;
	}
}

void cb_trans_conv(pmtransconv_t event, void *data1, void *data2,
                   void *data3, int *response)
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
				         alpm_pkg_get_name(data1),
				         alpm_pkg_get_name(data2));
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
				snprintf(str, LOG_STR_LEN, _(":: %s is designated as a HoldPkg. Remove anyway? [Y/n] "),
				         alpm_pkg_get_name(data1));
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
				         alpm_pkg_get_name(data1),
				         (char *)data3,
				         alpm_pkg_get_name(data2));
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
				         alpm_pkg_get_name(data1),
				         alpm_pkg_get_version(data1));
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
				         alpm_pkg_get_name(data1),
				         alpm_pkg_get_version(data1));
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

void cb_trans_progress(pmtransprog_t event, char *pkgname, const int percent,
                       const int howmany, const int remain)
{
	float timediff;

	/* size of line to allocate for text printing (e.g. not progressbar) */
	const int infolen = 50;
	int i, digits, textlen, pkglen;
	char *ptr = NULL;

	if(config->noprogressbar) {
		return;
	}

	if(percent == 0) {
		set_output_padding(1); /* turn on output padding with ' ' */
		timediff = get_update_timediff(1);
	} else {
		timediff = get_update_timediff(0);
	}

	if(percent > 0 && percent < 100 && !timediff) {
		/* only update the progress bar when
		 * a) we first start
		 * b) we end the progress
		 * c) it has been long enough since the last call
		 */
		return;
	}

	/* if no pkgname, percent is too high or unchanged, then return */
	if(!pkgname || percent == prevpercent) {
		return;
	}

	prevpercent=percent;
	switch (event) {
		case PM_TRANS_PROGRESS_ADD_START:
			ptr = _("installing");
			break;
		case PM_TRANS_PROGRESS_UPGRADE_START:
			ptr = _("upgrading");
			break;
		case PM_TRANS_PROGRESS_REMOVE_START:
			ptr = _("removing");
			break;
		case PM_TRANS_PROGRESS_CONFLICTS_START:
			ptr = _("checking for file conflicts");
			break;
	}

	/* find # of digits in package counts to scale output */
	digits = 1;
	i = howmany;
	while((i /= 10)) {
		++digits;
	}

	/* determine room left for non-digits text [not ( 1/12) part] */
	textlen = infolen - 3 - (2 * digits);
	/* room left for package name */
	pkglen = textlen - strlen(ptr) - 1;

	switch (event) {
		case PM_TRANS_PROGRESS_ADD_START:
		case PM_TRANS_PROGRESS_UPGRADE_START:
		case PM_TRANS_PROGRESS_REMOVE_START:
			/* TODO clean up so digits and pkglen aren't passed twice */
			printf("(%*d/%*d) %s %-*.*s", digits, remain, digits, howmany,
			       ptr, pkglen, pkglen, pkgname);
			break;
		case PM_TRANS_PROGRESS_CONFLICTS_START:
			printf("(%*d/%*d) %-*s", digits, remain, digits, howmany,
			       textlen, ptr);
			break;
	}

	/* call refactored fill progress function */
	fill_progress(percent, getcols() - infolen);

	if(percent >= 100) {
		set_output_padding(0); /* restore padding */
	}

}

/* vim: set ts=2 sw=2 noet: */
