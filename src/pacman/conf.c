/*
 *  conf.c
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
#include <string.h>
#include <limits.h>

#include <alpm.h>
/* pacman */
#include "util.h"
#include "log.h"
#include "list.h"
#include "sync.h"
#include "download.h"

#define min(X, Y)  ((X) < (Y) ? (X) : (Y))

extern char *pmo_dbpath;
extern list_t *pmo_holdpkg;
extern char *pmo_proxyhost;
extern unsigned short pmo_proxyport;
extern char *pmo_xfercommand;
extern unsigned short pmo_nopassiveftp;

extern list_t *pmc_syncs;

int parseconfig(char *file)
{
	FILE *fp = NULL;
	char line[PATH_MAX+1];
	char *ptr = NULL;
	char *key = NULL;
	int linenum = 0;
	char section[256] = "";
	sync_t *sync = NULL;

	fp = fopen(file, "r");
	if(fp == NULL) {
		perror(file);
		return(1);
	}

	while(fgets(line, PATH_MAX, fp)) {
		linenum++;
		strtrim(line);
		if(strlen(line) == 0 || line[0] == '#') {
			continue;
		}
		if(line[0] == '[' && line[strlen(line)-1] == ']') {
			/* new config section */
			ptr = line;
			ptr++;
			strncpy(section, ptr, min(255, strlen(ptr)-1));
			section[min(255, strlen(ptr)-1)] = '\0';
			vprint("config: new section '%s'\n", section);
			if(!strlen(section)) {
				ERR(NL, "config: line %d: bad section name\n", linenum);
				return(1);
			}
			if(!strcmp(section, "local")) {
				ERR(NL, "config: line %d: '%s' is reserved and cannot be used as a package tree\n",
					linenum, section);
				return(1);
			}
			if(strcmp(section, "options")) {
				list_t *i;
				int found = 0;
				for(i = pmc_syncs; i && !found; i = i->next) {
					sync = (sync_t *)i->data;
					if(!strcmp(sync->treename, section)) {
						found = 1;
					}
				}
				if(!found) {
					/* start a new sync record */
					sync = (sync_t *)malloc(sizeof(sync_t));
					if(sync == NULL) {
						ERR(NL, "could not allocate %d bytes\n", sizeof(sync_t));
						return(1);
					}
					sync->treename = strdup(section);
					sync->servers = NULL;
					pmc_syncs = list_add(pmc_syncs, sync);
				}
			}
		} else {
			/* directive */
			ptr = line;
			key = strsep(&ptr, "=");
			if(key == NULL) {
				ERR(NL, "config: line %d: syntax error\n", linenum);
				return(1);
			}
			strtrim(key);
			key = strtoupper(key);
			if(!strlen(section) && strcmp(key, "INCLUDE")) {
				ERR(NL, "config: line %d: all directives must belong to a section\n", linenum);
				return(1);
			}
			if(ptr == NULL) {
				if(!strcmp(key, "NOPASSIVEFTP")) {
					pmo_nopassiveftp = 1;
					vprint("config: nopassiveftp\n");
				} else if(!strcmp(key, "USESYSLOG")) {
					if(alpm_set_option(PM_OPT_USESYSLOG, (long)1) == -1) {
						ERR(NL, "failed to set option USESYSLOG (%s)\n", alpm_strerror(pm_errno));
						return(1);
					}
					vprint("config: usesyslog\n");
				} else {
					ERR(NL, "config: line %d: syntax error\n", linenum);
					return(1);
				}
			} else {
				strtrim(ptr);
				if(!strcmp(key, "INCLUDE")) {
					char conf[PATH_MAX];
					strncpy(conf, ptr, PATH_MAX);
					vprint("config: including %s\n", conf);
					parseconfig(conf);
				} else if(!strcmp(section, "options")) {
					if(!strcmp(key, "NOUPGRADE")) {
						char *p = ptr;
						char *q;
						while((q = strchr(p, ' '))) {
							*q = '\0';
							if(alpm_set_option(PM_OPT_NOUPGRADE, (long)p) == -1) {
								ERR(NL, "failed to set option NOUPGRADE (%s)\n", alpm_strerror(pm_errno));
								return(1);
							}
							vprint("config: noupgrade: %s\n", p);
							p = q;
							p++;
						}
						if(alpm_set_option(PM_OPT_NOUPGRADE, (long)p) == -1) {
							ERR(NL, "failed to set option NOUPGRADE (%s)\n", alpm_strerror(pm_errno));
							return(1);
						}
						vprint("config: noupgrade: %s\n", p);
					} else if(!strcmp(key, "IGNOREPKG")) {
						char *p = ptr;
						char *q;
						while((q = strchr(p, ' '))) {
							*q = '\0';
							if(alpm_set_option(PM_OPT_IGNOREPKG, (long)p) == -1) {
								ERR(NL, "failed to set option IGNOREPKG (%s)\n", alpm_strerror(pm_errno));
								return(1);
							}
							vprint("config: ignorepkg: %s\n", p);
							p = q;
							p++;
						}
						if(alpm_set_option(PM_OPT_IGNOREPKG, (long)p) == -1) {
							ERR(NL, "failed to set option IGNOREPKG (%s)\n", alpm_strerror(pm_errno));
							return(1);
						}
						vprint("config: ignorepkg: %s\n", p);
					} else if(!strcmp(key, "HOLDPKG")) {
						char *p = ptr;
						char *q;
						while((q = strchr(p, ' '))) {
							*q = '\0';
							pmo_holdpkg = list_add(pmo_holdpkg, strdup(p));
							vprint("config: holdpkg: %s\n", p);
							p = q;
							p++;
						}
						pmo_holdpkg = list_add(pmo_holdpkg, strdup(p));
						vprint("config: holdpkg: %s\n", p);
					} else if(!strcmp(key, "DBPATH")) {
						/* shave off the leading slash, if there is one */
						if(*ptr == '/') {
							ptr++;
						}
						FREE(pmo_dbpath);
						pmo_dbpath = strdup(ptr);
						vprint("config: dbpath: %s\n", ptr);
					} else if (!strcmp(key, "LOGFILE")) {
						if(alpm_set_option(PM_OPT_LOGFILE, (long)ptr) == -1) {
							ERR(NL, "failed to set option LOGFILE (%s)\n", alpm_strerror(pm_errno));
							return(1);
						}
						vprint("config: log file: %s\n", ptr);
					} else if (!strcmp(key, "XFERCOMMAND")) {
						FREE(pmo_xfercommand);
						pmo_xfercommand = strndup(ptr, PATH_MAX);
						vprint("config: xfercommand: %s\n", pmo_xfercommand);
					} else if (!strcmp(key, "PROXYSERVER")) {
						char *p;
						if(pmo_proxyhost) {
							FREE(pmo_proxyhost);
						}
						p = strstr(ptr, "://");
						if(p) {
							p += 3;
							if(p == NULL || *p == '\0') {
								ERR(NL, "config: line %d: bad server location\n", linenum);
								return(1);
							}
							ptr = p;
						}
						pmo_proxyhost = strndup(ptr, PATH_MAX);
						vprint("config: proxyserver: %s\n", pmo_proxyhost);
					} else if (!strcmp(key, "PROXYPORT")) {
						pmo_proxyport = (unsigned short)atoi(ptr);
						vprint("config: proxyport: %u\n", pmo_proxyport);
					} else {
						ERR(NL, "config: line %d: syntax error\n", linenum);
						return(1);
					}
				} else {
					if(!strcmp(key, "SERVER")) {
						/* parse our special url */
						server_t *server;
						char *p;

						if((server = (server_t *)malloc(sizeof(server_t))) == NULL) {
							ERR(NL, "could not allocate %d bytes\n", sizeof(server_t));
							return(1);
						}
						server->server = server->path = NULL;
						server->protocol = NULL;

						p = strstr(ptr, "://");
						if(p == NULL) {
							ERR(NL, "config: line %d: bad server location\n", linenum);
							return(1);
						}
						*p = '\0';
						p++; p++; p++;
						if(p == NULL || *p == '\0') {
							ERR(NL, "config: line %d: bad server location\n", linenum);
							return(1);
						}
						server->protocol = strdup(ptr);
						if(!strcmp(server->protocol, "ftp") || !strcmp(server->protocol, "http")) {
							char *slash;
							/* split the url into domain and path */
							slash = strchr(p, '/');
							if(slash == NULL) {
								/* no path included, default to / */
								server->path = strdup("/");
							} else {
								/* add a trailing slash if we need to */
								if(slash[strlen(slash)-1] == '/') {
									server->path = strdup(slash);
								} else {
									if((server->path = (char *)malloc(strlen(slash)+2)) == NULL) {
										ERR(NL, "could not allocate %d bytes\n", sizeof(strlen(slash+2)));
										return(1);
									}
									sprintf(server->path, "%s/", slash);
								}
								*slash = '\0';
							}
							server->server = strdup(p);
						} else if(!strcmp(server->protocol, "file")){
							/* add a trailing slash if we need to */
							if(p[strlen(p)-1] == '/') {
								server->path = strdup(p);
							} else {
								server->path = (char *)malloc(strlen(p)+2);
								if(server->path == NULL) {
									ERR(NL, "could not allocate %d bytes\n", sizeof(strlen(p+2)));
									return(1);
								}
								sprintf(server->path, "%s/", p);
							}
						} else {
							ERR(NL, "config: line %d: protocol %s is not supported\n", linenum, ptr);
							return(1);
						}
						/* add to the list */
						vprint("config: %s: server: %s %s %s\n", section, server->protocol, server->server, server->path);
						sync->servers = list_add(sync->servers, server);
					} else {
						ERR(NL, "config: line %d: syntax error\n", linenum);
						return(1);
					}
				}
				line[0] = '\0';
			}
		}
	}
	fclose(fp);

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
