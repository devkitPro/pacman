/*
 *  sync.c
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
#include "download.h"
#include "list.h"
#include "package.h"
#include "db.h"
#include "sync.h"
#include "pacman.h"

extern char *pmo_root;
extern char *pmo_dbpath;

extern unsigned short pmo_noconfirm;
extern unsigned short pmo_d_resolve;
extern unsigned short pmo_q_info;
extern unsigned short pmo_q_list;
extern unsigned short pmo_s_upgrade;
extern unsigned short pmo_s_downloadonly;
extern unsigned short pmo_s_printuris;
extern unsigned short pmo_s_sync;
extern unsigned short pmo_s_search;
extern unsigned short pmo_s_clean;
extern unsigned short pmo_group;
extern unsigned char  pmo_flags;

extern PM_DB *db_local;
extern list_t *pmc_syncs;

extern int maxcols;

static int sync_cleancache(int level)
{
		if(level == 1) {
			/* incomplete cleanup: we keep latest packages and partial downloads */
			DIR *dir;
			struct dirent *ent;
			list_t *cache = NULL;
			list_t *clean = NULL;
			list_t *i, *j;

			printf("removing old packages from cache... ");
			dir = opendir("/var/cache/pacman/pkg");
			if(dir == NULL) {
				fprintf(stderr, "error: could not access cache directory\n");
				return(1);
			}
			rewinddir(dir);
			while((ent = readdir(dir)) != NULL) {
				if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
					continue;
				}
				cache = list_add(cache, strdup(ent->d_name));
			}
			closedir(dir);

			for(i = cache; i; i = i->next) {
				char *str = i->data;
				char name[256], version[64];

				if(strstr(str, PM_EXT_PKG) == NULL) {
					clean = list_add(clean, strdup(str));
					continue;
				}
				/* we keep partially downloaded files */
				if(strstr(str, PM_EXT_PKG".part")) {
					continue;
				}
				if(split_pkgname(str, name, version) != 0) {
					clean = list_add(clean, strdup(str));
					continue;
				}
				for(j = i->next; j; j = j->next) {
					char *s = j->data;
					char n[256], v[64];

					if(strstr(s, PM_EXT_PKG) == NULL) {
						continue;
					}
					if(strstr(s, PM_EXT_PKG".part")) {
						continue;
					}
					if(split_pkgname(s, n, v) != 0) {
						continue;
					}
					if(!strcmp(name, n)) {
						char *ptr = (alpm_pkg_vercmp(version, v) < 0) ? str : s;
						if(!list_is_strin(ptr, clean)) {
							clean = list_add(clean, strdup(ptr));
						}
					}
				}
			}
			FREELIST(cache);

			for(i = clean; i; i = i->next) {
				char path[PATH_MAX];

				snprintf(path, PATH_MAX, "%s%s/%s", pmo_root, CACHEDIR, (char *)i->data);
				unlink(path);
			}
			FREELIST(clean);
		} else {
			/* ORE
			// full cleanup
			mode_t oldmask;
			char path[PATH_MAX];

			snprintf(path, PATH_MAX, "%s%s", pmo_root, CACHEDIR);

			printf("removing all packages from cache... ");
			if(rmrf(path)) {
				fprintf(stderr, "error: could not remove cache directory\n");
				return(1);
			}

			oldmask = umask(0000);
			if(makepath(path)) {
				fprintf(stderr, "error: could not create new cache directory\n");
				return(1);
			}
			umask(oldmask);*/
		}
		printf("done.\n");
		return(0);
}

static int sync_synctree(list_t *syncs)
{
	char *root, *dbpath;
	list_t *i;
	int ret = 0;

	alpm_get_option(PM_OPT_ROOT, (long *)&root);
	alpm_get_option(PM_OPT_DBPATH, (long *)&dbpath);

	for(i = syncs; i; i = i->next) {
		char path[PATH_MAX];
		list_t *files = NULL;
		sync_t *sync = (sync_t *)i->data;

		/* build a one-element list */
		snprintf(path, PATH_MAX, "%s"PM_EXT_DB, sync->treename);
		files = list_add(files, strdup(path));

		snprintf(path, PATH_MAX, "%s%s", root, dbpath);

		if(downloadfiles(sync->servers, path, files)) {
			fprintf(stderr, "failed to synchronize %s\n", sync->treename);
			FREELIST(files);
			ret--;
			continue;
		}

		FREELIST(files);

		snprintf(path, PATH_MAX, "%s%s/%s"PM_EXT_DB, root, dbpath, sync->treename);
		if(alpm_db_update(sync->treename, path) == -1) {
			fprintf(stderr, "error: %s\n", alpm_strerror(pm_errno));
			ret--;
		}

		/* remove the .tar.gz */
		unlink(path);
	}

	return(ret);
}

static int sync_search(list_t *syncs, list_t *targets)
{
	list_t *i;

	for(i = syncs; i; i = i->next) {
		sync_t *sync = i->data;
		if(targets) {
			list_t *j;

			for(j = targets; j; j = j->next) {
				db_search(sync->db, sync->treename, j->data);
			}
		} else {
			PM_LIST *lp;

			for(lp = alpm_db_getpkgcache(sync->db); lp; lp = alpm_list_next(lp)) {
				PM_PKG *pkg = alpm_list_getdata(lp);

				printf("%s/%s %s\n    ", sync->treename, (char *)alpm_pkg_getinfo(pkg, PM_PKG_NAME), (char *)alpm_pkg_getinfo(pkg, PM_PKG_VERSION));
				indentprint(alpm_pkg_getinfo(pkg, PM_PKG_DESC), 4);
				printf("\n");
			}
		}
	}

	return(0);
}

static int sync_group(list_t *syncs, list_t *targets)
{
	list_t *i, *j;
	
	if(targets) {
		for(i = targets; i; i = i->next) {
			for(j = syncs; j; j = j->next) {
				sync_t *sync = j->data;
				PM_GRP *grp = alpm_db_readgrp(sync->db, i->data);

				if(grp) {
					printf("%s/%s\n", sync->treename, (char *)alpm_grp_getinfo(grp, PM_GRP_NAME));
					PM_LIST_display("   ", alpm_grp_getinfo(grp, PM_GRP_PKGNAMES));
				}
			}
		}
	} else {
		for(j = syncs; j; j = j->next) {
			sync_t *sync = j->data;
			PM_LIST *lp;

			for(lp = alpm_db_getpkgcache(sync->db); lp; lp = alpm_list_next(lp)) {
				PM_GRP *grp = alpm_list_getdata(lp);

				printf("%s/%s\n", (char *)sync->treename, (char *)alpm_grp_getinfo(grp, PM_GRP_NAME));
				PM_LIST_display("   ", alpm_grp_getinfo(grp, PM_GRP_PKGNAMES));
			}
		}
	}

	return(0);
}

static int sync_info(list_t *syncs, list_t *targets)
{
	list_t *i, *j;

	if(targets) {
		for(i = targets; i; i = i->next) {
			int found = 0;

			for(j = syncs; j && !found; j = j->next) {
				sync_t *sync = j->data;
				PM_LIST *lp;

				for(lp = alpm_db_getpkgcache(sync->db); !found && lp; lp = alpm_list_next(lp)) {
					PM_PKG *pkg = alpm_list_getdata(lp);

					if(!strcmp(alpm_pkg_getinfo(pkg, PM_PKG_NAME), i->data)) {
						dump_pkg_sync(pkg, sync->treename);
						printf("\n");
						found = 1;
					}
				}
			}
			if(!found) {
				fprintf(stderr, "Package \"%s\" was not found.\n", (char *)i->data);
				break;
			}
		}
	} else {
		for(j = syncs; j; j = j->next) {
			sync_t *sync = j->data;
			PM_LIST *lp;
			
			for(lp = alpm_db_getpkgcache(sync->db); lp; lp = alpm_list_next(lp)) {
				dump_pkg_sync(alpm_list_getdata(lp), sync->treename);
				printf("\n");
			}
		}
	}

	return(0);
}

static int sync_list(list_t *syncs, list_t *targets)
{
	list_t *i, *treenames = NULL;

	if(targets) {
		for(i = targets; i; i = i->next) {
			list_t *j;
			sync_t *sync = NULL;

			for(j = syncs; j; j = j->next) {
				sync_t *s = j->data;

				if(strcmp(i->data, s->treename) == 0) {
					MALLOC(sync, sizeof(sync_t));
					sync->treename = i->data;
					sync->db = s->db;
				}
			}

			if(sync == NULL) {
				fprintf(stderr, "Repository \"%s\" was not found.\n\n", (char *)i->data);
				list_free(treenames);
				return(1);
			}

			treenames = list_add(treenames, sync);
		}
	} else {
		treenames = syncs;
	}

	for(i = treenames; i; i = i->next) {
		PM_LIST *lp;
		sync_t *sync = i->data;

		for(lp = alpm_db_getpkgcache(sync->db); lp; lp = alpm_list_next(lp)) {
			PM_PKG *pkg = alpm_list_getdata(lp);

			printf("%s %s %s\n", (char *)sync->treename, (char *)alpm_pkg_getinfo(pkg, PM_PKG_NAME), (char *)alpm_pkg_getinfo(pkg, PM_PKG_VERSION));
		}
	}

	if(targets) {
		list_free(treenames);
	}

	return(0);
}

int pacman_sync(list_t *targets)
{
	int allgood = 1, confirm = 0;
	int retval = 0;
	list_t *final = NULL;
	list_t *i, *j;
	PM_LIST *lp, *data;

	char ldir[PATH_MAX];
	int varcache = 1;
	int done = 0;
	int count = 0;
	sync_t *current = NULL;
	list_t *processed = NULL;
	list_t *files = NULL;

	if(pmc_syncs == NULL || !list_count(pmc_syncs)) {
		ERR(NL, "error: no usable package repositories configured.");
		return(1);
	}

	if(pmo_s_clean) {
		return(sync_cleancache(pmo_s_clean));
	}

	if(pmo_s_sync) {
		/* grab a fresh package list */
		MSG(NL, ":: Synchronizing package databases...\n");
		alpm_logaction("synchronizing package lists");
		sync_synctree(pmc_syncs);
	}

	/* open the database(s) */
	for(i = pmc_syncs; i; i = i->next) {
		sync_t *sync = i->data;
		if(alpm_db_register(sync->treename, &sync->db) == -1) {
			ERR(NL, "%s\n", alpm_strerror(pm_errno));
			return(1);
		}
	}

	if(pmo_s_search) {
		return(sync_search(pmc_syncs, targets));
	}

	if(pmo_group) {
		return(sync_group(pmc_syncs, targets));
	}

	if(pmo_q_info) {
		return(sync_info(pmc_syncs, targets));
	}

	if(pmo_q_list) {
		return(sync_list(pmc_syncs, targets));
	}

	if(pmo_s_upgrade) {
		/* ORE
		alpm_logaction(NULL, "starting full system upgrade");*/
		if(alpm_sync_sysupgrade(&data) == -1) {
			if(pm_errno == PM_ERR_UNRESOLVABLE_DEPS) {
				ERR(NL, "cannot resolve dependencies\n");
				for(lp = alpm_list_first(data); lp; lp = alpm_list_next(lp)) {
					pmdepmissing_t *miss = alpm_list_getdata(lp);
					ERR(NL, "	%s: \"%s\" is not in the package set\n", miss->target, miss->depend.name);
				}
				alpm_list_free(data);
			} else {
				ERR(NL, "%s\n", alpm_strerror(pm_errno));
			}
			return(1);
		}

		/* check if pacman itself is one of the packages to upgrade.  If so, we
		 * we should upgrade ourselves first and then re-exec as the new version.
		 *
		 * this can prevent some of the "syntax error" problems users can have
		 * when sysupgrade'ing with an older version of pacman.
		 */
		for(lp = alpm_list_first(data); lp; lp = alpm_list_next(lp)) {
			PM_SYNC *sync = alpm_list_getdata(lp);

			if(!strcmp("pacman", alpm_pkg_getinfo(alpm_sync_getinfo(sync, PM_SYNC_SYNCPKG), PM_PKG_NAME))) {
				ERR(NL, "\n:: pacman has detected a newer version of the \"pacman\" package.\n");
				ERR(NL, ":: It is recommended that you allow pacman to upgrade itself\n");
				ERR(NL, ":: first, then you can re-run the operation with the newer version.\n");
				ERR(NL, "::\n");
				if(yesno(":: Upgrade pacman first? [Y/n] ")) {
					alpm_list_free(data);
					data = NULL;
				}
			}
		}

		for(lp = alpm_list_first(data); lp; lp = alpm_list_next(lp)) {
			PM_SYNC *sync = alpm_list_getdata(lp);
			PM_PKG *lpkg, *spkg;
			char *spkgname, *spkgver, *lpkgname, *lpkgver;

			lpkg = alpm_sync_getinfo(sync, PM_SYNC_LOCALPKG);
			lpkgname = alpm_pkg_getinfo(lpkg, PM_PKG_NAME);
			lpkgver = alpm_pkg_getinfo(lpkg, PM_PKG_VERSION);

			spkg = alpm_sync_getinfo(sync, PM_SYNC_SYNCPKG);
			spkgname = alpm_pkg_getinfo(spkg, PM_PKG_NAME);
			spkgver = alpm_pkg_getinfo(spkg, PM_PKG_VERSION);

			switch((int)alpm_sync_getinfo(sync, PM_SYNC_TYPE)) {
				case PM_SYSUPG_REPLACE:
					if(yesno(":: Replace %s with %s from \"%s\"? [Y/n] ", lpkgname, spkgname, NULL/*dbs->db->treename*/)) {
						DBG("adding '%s-%s' to replaces candidates\n", spkgname, spkgver);
						final = list_add(final, spkg);
					}

					break;
				case PM_SYSUPG_UPGRADE:
					DBG("Upgrade %s (%s => %s)\n", lpkgname, lpkgver, spkgver);
					final = list_add(final, spkg);
					break;
				default:
					break;
			}
		}
		alpm_list_free(data);
	} else {
		/* process targets */
		for(i = targets; i; i = i->next) {
			char *treename;
			char *targ;
			char *targline;
			PM_PKG *local = NULL;

			targline = strdup((char *)i->data);
			targ = index(targline, '/');
			if(targ) {
				*targ = '\0';
				targ++;
				treename = targline;
			} else {
				targ = targline;
				treename = NULL;
			}

			if(treename == NULL) {
				for(j = pmc_syncs; j && !local; j = j->next) {
					sync_t *sync = j->data;
					local = alpm_db_readpkg(sync->db, targ);
				}
			} else {
				for(j = pmc_syncs; j && !local; j = j->next) {
					sync_t *sync = j->data;
					if(strcmp(sync->treename, treename) == 0) {
						local = alpm_db_readpkg(sync->db, targ);
					}
				}
			}

			if(local == NULL) {
				PM_GRP *grp = NULL;
				/* target not found: check if it's a group */
				for(j = pmc_syncs; j && !grp; j = j->next) {
					sync_t *sync = j->data;
					grp = alpm_db_readgrp(sync->db, targ);
					if(grp) {
						PM_LIST *k, *pkgs;
						MSG(NL, ":: group %s:\n", targ);

						pkgs = alpm_grp_getinfo(grp, PM_GRP_PKGNAMES);
						PM_LIST_display("   ", pkgs);

						if(yesno("    Install whole content? [Y/n] ")) {
							for(k = alpm_list_first(pkgs); k; k = alpm_list_next(k)) {
								targets = list_add(targets, strdup(alpm_list_getdata(k)));
							}
						} else {
							for(k = alpm_list_first(pkgs); k; k = alpm_list_next(k)) {
								char *pkgname = alpm_list_getdata(k);
								if(yesno(":: Install %s from group %s? [Y/n] ", pkgname, targ)) {
									targets = list_add(targets, strdup(pkgname));
								}
							}
						}
					}
				}
				if(grp == NULL) {
					ERR(NL, "package \"%s\" not found", targ);
					return(1);
				}
			}
			if(treename) {
				FREE(targline);
			}

		}

		if(!pmo_s_downloadonly && !pmo_s_printuris) {
			/* this is an upgrade, compare versions and determine if it is necessary */
			for(i = targets; i; i = i->next) {
				int cmp;
				PM_PKG *local, *sync;
				char *lpkgname, *lpkgver, *spkgver;

				local = alpm_db_readpkg(db_local, i->data);
				lpkgname = alpm_pkg_getinfo(local, PM_PKG_NAME);
				lpkgver = alpm_pkg_getinfo(local, PM_PKG_VERSION);

				sync = alpm_db_readpkg(db_local, i->data);
				spkgver = alpm_pkg_getinfo(sync, PM_PKG_VERSION);

				cmp = alpm_pkg_vercmp(lpkgver, spkgver);
				if(cmp > 0) {
					/* local version is newer - get confirmation first */
					if(!yesno(":: %s-%s: local version is newer.  Upgrade anyway? [Y/n] ", lpkgname, lpkgver)) {
						/* ORE
						char *data = list_remove(targets, lpkgname);
						free(data);*/
					}
				} else if(cmp == 0) {
					/* versions are identical */
					if(!yesno(":: %s-%s: is up to date.  Upgrade anyway? [Y/n] ", lpkgname, lpkgver)) {
						/* ORE
						char *data = list_remove(targets, lpkgname);
						free(data);*/
					}
				}
			}
		}
	}

	/* Step 1: create a new transaction */
	if(alpm_trans_init(PM_TRANS_TYPE_SYNC, pmo_flags, NULL) == -1) {
		ERR(NL, "failed to init transaction (%s)\n", alpm_strerror(pm_errno));
		retval = 1;
		goto cleanup;
	}
	/* and add targets to it */
	for(i = targets; i; i = i->next) {
		if(alpm_trans_addtarget(i->data) == -1) {
			ERR(NL, "failed to add target '%s' (%s)\n", (char *)i->data, alpm_strerror(pm_errno));
			retval = 1;
			goto cleanup;
		}
	}

	PM_LIST_display("target :", alpm_trans_getinfo(PM_TRANS_TARGETS));
	/* ORE
	TBD */

	/* Step 2: "compute" the transaction based on targets and flags */
	if(alpm_trans_prepare(&data) == -1) {
		ERR(NL, "failed to prepare transaction (%s)\n", alpm_strerror(pm_errno));
		return(1);
	}

	/* list targets */
	if(final && !pmo_s_printuris) {
		list_t *list = NULL;
		char *str;
		unsigned long totalsize = 0;
		double mb;
		/* ORE
		for(i = rmtargs; i; i = i->next) {
			list = list_add(list, strdup(i->data));
		}
		for(i = final; i; i = i->next) {
			syncpkg_t *s = (syncpkg_t*)i->data;
			for(j = s->replaces; j; j = j->next) {
				pkginfo_t *p = (pkginfo_t*)j->data;
				list = list_add(list, strdup(p->name));
			}
		}
		if(list) {
			printf("\nRemove:  ");
			str = buildstring(list);
			indentprint(str, 9);
			printf("\n");
			FREELIST(list);
			FREE(str);
		}*/
		/* ORE
		for(i = final; i; i = i->next) {
			MALLOC(str, strlen(s->pkg->name)+strlen(s->pkg->version)+2);
			sprintf(str, "%s-%s", s->pkg->name, s->pkg->version);
			list = list_add(list, str);
			totalsize += s->pkg->size;
		}*/
		mb = (double)(totalsize / 1048576.0);
		/* round up to 0.1 */
		if(mb < 0.1) {
			mb = 0.1;
		}
		printf("\nTargets: ");
		str = buildstring(list);
		indentprint(str, 9);
		printf("\n\nTotal Package Size:   %.1f MB\n", mb);
		FREELIST(list);
		FREE(str);
	}

	/* get confirmation */
	if(pmo_s_downloadonly) {
		if(pmo_noconfirm) {
			MSG(NL, "\nBeginning download...\n");
			confirm = 1;
		} else {
			confirm = yesno("\nProceed with download? [Y/n] ");
		}
	} else {
		/* don't get any confirmation if we're called from makepkg */
		if(pmo_d_resolve || pmo_s_printuris) {
			confirm = 1;
		} else {
			if(pmo_noconfirm) {
				MSG(NL, "\nBeginning upgrade process...\n");
				confirm = 1;
			} else {
				confirm = yesno("\nProceed with upgrade? [Y/n] ");
			}
		}
	}
	if(!confirm) {
		retval = 1;
		goto cleanup;
	}

	/* ORE
	group sync records by repository and download */

	snprintf(ldir, PATH_MAX, "%svar/cache/pacman/pkg", pmo_root);

	while(!done) {
		if(current) {
			processed = list_add(processed, current);
			current = NULL;
		}
		for(i = final; i; i = i->next) {
			if(current == NULL) {
				/* we're starting on a new repository */
			}
			/*if(current && !strcmp(current->treename, sync->dbs->sync->treename)) {
			}*/
		}

		if(files) {
			if(pmo_s_printuris) {
				server_t *server = (server_t*)current->servers->data;
				for(j = files; j; j = j->next) {
					if(!strcmp(server->protocol, "file")) {
						MSG(NL, "%s://%s%s\n", server->protocol, server->path,
							(char *)j->data);
					} else {
						MSG(NL, "%s://%s%s%s\n", server->protocol,
							server->server, server->path, (char *)j->data);
					}
				}
			} else {
				struct stat buf;

				MSG(NL, "\n:: Retrieving packages from %s...\n", current->treename);
				fflush(stdout);
				if(stat(ldir, &buf)) {
					mode_t oldmask;

					/* no cache directory.... try creating it */
					/* ORE
					 * alpm_logaction(stderr, "warning: no %s cache exists.  creating...", ldir);*/
					oldmask = umask(0000);
					if(makepath(ldir)) {				
						/* couldn't mkdir the cache directory, so fall back to /tmp and unlink
						 * the package afterwards.
						 */
						/* ORE
						 * logaction(stderr, "warning: couldn't create package cache, using /tmp instead");*/
						snprintf(ldir, PATH_MAX, "/tmp");
						varcache = 0;
					}
					umask(oldmask);
				}
				if(downloadfiles(current->servers, ldir, files)) {
					ERR(NL, "failed to retrieve some files from %s\n", current->treename);
					retval = 1;
					goto cleanup;
				}
			}

			count += list_count(files);
			FREELIST(files);
		}
		if(count == list_count(final)) {
			done = 1;
		}
	}
	printf("\n");

	/* Check integrity of files */
	MSG(NL, "checking package integrity... ");

	allgood = 1;
	for(i = final; i; i = i->next) {
		char /*str[PATH_MAX],*/ pkgname[PATH_MAX];
		char *md5sum1, *md5sum2;

		snprintf(pkgname, PATH_MAX, "%s-%s"PM_EXT_PKG, "", "");

		md5sum1 = NULL;
		md5sum2 = NULL;

		if(strcmp(md5sum1, md5sum2) != 0) {
			if(allgood) {
				printf("\n");
			}
			ERR(NL, "error: archive %s is corrupted\n", "");
			allgood = 0;
		}

		FREE(md5sum2);

	}
	if(!allgood) {
		retval = 1;
		goto cleanup;
	}
	MSG(CL, "done.\n");

	/* Step 3: actually perform the installation */
	if(!pmo_s_downloadonly) {
		if(alpm_trans_commit(&data) == -1) {
			ERR(NL, "failed to commit transaction (%s)\n", alpm_strerror(pm_errno));
			retval = 1;
			goto cleanup;
		}
	}

	if(!varcache && !pmo_s_downloadonly) {
		/* delete packages */
		for(i = files; i; i = i->next) {
			unlink(i->data);
		}
	}

cleanup:

	return(retval);

}

/* vim: set ts=2 sw=2 noet: */
