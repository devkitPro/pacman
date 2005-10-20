/*
 *  trans.c
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include <alpm.h>
/* pacman */
#include "util.h"
#include "log.h"
#include "trans.h"

#define LOG_STR_LEN 256

/* Callback to handle transaction events
 */
void cb_trans_evt(unsigned char event, void *data1, void *data2)
{
	char str[LOG_STR_LEN] = "";

	switch(event) {
		case PM_TRANS_EVT_CHECKDEPS_START:
			MSG(NL, "checking dependencies... ");
		break;
		case PM_TRANS_EVT_FILECONFLICTS_START:
			MSG(NL, "checking for file conflicts... ");
		break;
		case PM_TRANS_EVT_RESOLVEDEPS_START:
			MSG(NL, "resolving dependencies... ");
		break;
		case PM_TRANS_EVT_INTERCONFLICTS_START:
			MSG(NL, "looking for inter-conflicts... ");
		break;
		case PM_TRANS_EVT_CHECKDEPS_DONE:
		case PM_TRANS_EVT_FILECONFLICTS_DONE:
		case PM_TRANS_EVT_RESOLVEDEPS_DONE:
		case PM_TRANS_EVT_INTERCONFLICTS_DONE:
			MSG(CL, "done.\n");
		break;
		case PM_TRANS_EVT_ADD_START:
			MSG(NL, "installing %s... ", (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME));
		break;
		case PM_TRANS_EVT_ADD_DONE:
			MSG(CL, "done.\n");
			snprintf(str, LOG_STR_LEN, "installed %s (%s)",
			                   (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME),
			                   (char *)alpm_pkg_getinfo(data1, PM_PKG_VERSION));
			alpm_logaction(str);
		break;
		case PM_TRANS_EVT_REMOVE_START:
			MSG(NL, "removing %s... ", (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME));
		break;
		case PM_TRANS_EVT_REMOVE_DONE:
			MSG(CL, "done.\n");
			snprintf(str, LOG_STR_LEN, "removed %s (%s)",
			         (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME),
			         (char *)alpm_pkg_getinfo(data1, PM_PKG_VERSION));
			alpm_logaction(str);
		break;
		case PM_TRANS_EVT_UPGRADE_START:
			MSG(NL, "upgrading %s... ", (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME));
		break;
		case PM_TRANS_EVT_UPGRADE_DONE:
			MSG(CL, "done.\n");
			snprintf(str, LOG_STR_LEN, "upgraded %s (%s -> %s)",
			                   (char *)alpm_pkg_getinfo(data1, PM_PKG_NAME),
			                   (char *)alpm_pkg_getinfo(data1, PM_PKG_VERSION),
			                   (char *)alpm_pkg_getinfo(data2, PM_PKG_VERSION));
			alpm_logaction(str);
		break;
	}
}

void cb_trans_conv(unsigned char event, void *data1, void *data2, void *data3, int *response)
{
	char str[LOG_STR_LEN] = "";

	switch(event) {
		case PM_TRANS_CONV_INSTALL_IGNOREPKG:
			snprintf(str, LOG_STR_LEN, ":: %s requires %s, but it is in IgnorePkg.  Install anyway? [Y/n] ",
					(char *)alpm_pkg_getinfo(data1, PM_PKG_NAME),
					(char *)alpm_pkg_getinfo(data2, PM_PKG_NAME));
			*response = yesno(str);
		break;
		case PM_TRANS_CONV_REPLACE_PKG:
			snprintf(str, LOG_STR_LEN, ":: Replace %s with %s/%s? [Y/n] ",
					(char *)alpm_pkg_getinfo(data1, PM_PKG_NAME),
					(char *)data3,
					(char *)alpm_pkg_getinfo(data2, PM_PKG_NAME));
			*response = yesno(str);
		break;
		case PM_TRANS_CONV_LOCAL_NEWER:
			snprintf(str, LOG_STR_LEN, ":: %s-%s: local version is newer. Upgrade anyway? [Y/n] ",
					(char *)alpm_pkg_getinfo(data1, PM_PKG_NAME),
					(char *)alpm_pkg_getinfo(data1, PM_PKG_VERSION));
			*response = yesno(str);
		break;
		case PM_TRANS_CONV_LOCAL_UPTODATE:
			snprintf(str, LOG_STR_LEN, ":: %s-%s: local version is up to date. Upgrade anyway? [Y/n] ",
					(char *)alpm_pkg_getinfo(data1, PM_PKG_NAME),
					(char *)alpm_pkg_getinfo(data1, PM_PKG_VERSION));
			*response = yesno(str);
		break;
	}
}

/* vim: set ts=2 sw=2 noet: */
