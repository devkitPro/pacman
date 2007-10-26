/*
 *  testdb.c : Test a pacman local database for validity
 *
 *  Copyright (c) 2007 by Aaron Griffin <aaronmgriffin@gmail.com>
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>

#include <alpm.h>
#include <alpm_list.h>

int str_cmp(const void *s1, const void *s2)
{
  return(strcmp(s1, s2));
}

static void diffrqdby(const char *pkgname, alpm_list_t *oldrqdby, alpm_list_t *newrqdby)
{
  oldrqdby = alpm_list_msort(oldrqdby, alpm_list_count(oldrqdby), str_cmp);
  newrqdby = alpm_list_msort(newrqdby, alpm_list_count(newrqdby), str_cmp);

  alpm_list_t *i = oldrqdby;
  alpm_list_t *j = newrqdby;

  while(i || j) {
    char *s1 = NULL;
    char *s2 = NULL;
    int n;
    if(i && !j) {
      n = -1;
    } else if(!i && j) {
      n = 1;
    } else {
      s1 = i->data;
      s2 = j->data;
      n = strcmp(s1, s2);
    }
    if(n < 0) {
      s1 = i->data;
      printf("wrong requiredby for %s : %s\n", pkgname, s1);
      i = i->next;
    } else if (n > 0) {
      s2 = j->data;
      printf("missing requiredby for %s : %s\n", pkgname, s2);
      j = j->next;
    } else {
      i = i->next;
      j = j->next;
    }
  }
}

static void cleanup(int signum) {
  if(alpm_release() == -1) {
    fprintf(stderr, "error releasing alpm: %s\n", alpm_strerrorlast());
  }

  exit(signum);
}

void output_cb(pmloglevel_t level, char *fmt, va_list args)
{
  if(strlen(fmt)) {
    switch(level) {
      case PM_LOG_ERROR: printf("error: "); break;
      case PM_LOG_WARNING: printf("warning: "); break;
      default: return;
    }
    vprintf(fmt, args);
    printf("\n");
  }
}

static int db_test(char *dbpath)
{
  struct dirent *ent;
  char path[PATH_MAX];
  struct stat buf;
  int ret = 0;

  DIR *dir;
  
  if(!(dir = opendir(dbpath))) {
    fprintf(stderr, "error : %s : %s\n", dbpath, strerror(errno));
    return(1);
  }

  while ((ent = readdir(dir)) != NULL) {
    if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
      continue;
    }
    /* check for desc, depends, and files */
    snprintf(path, PATH_MAX, "%s/%s/desc", dbpath, ent->d_name);
    if(stat(path, &buf)) {
      printf("%s: description file is missing\n", ent->d_name);
      ret++;
    }
    snprintf(path, PATH_MAX, "%s/%s/depends", dbpath, ent->d_name);
    if(stat(path, &buf)) {
      printf("%s: dependency file is missing\n", ent->d_name);
      ret++;
    }
    snprintf(path, PATH_MAX, "%s/%s/files", dbpath, ent->d_name);
    if(stat(path, &buf)) {
      printf("%s: file list is missing\n", ent->d_name);
      ret++;
    }
  }
  return(ret);
}

int main(int argc, char **argv)
{
  int retval = 0; /* default = false */
  pmdb_t *db = NULL;
  char *dbpath;
  char localdbpath[PATH_MAX];
  alpm_list_t *i;

  if(argc == 1) {
    dbpath = DBPATH;
  } else if(argc == 3 && strcmp(argv[1], "-b") == 0) {
    dbpath = argv[2];
  } else {
    fprintf(stderr, "usage: %s -b <pacman db>\n", basename(argv[0]));
    return(1);
  }

  snprintf(localdbpath, PATH_MAX, "%s/local", dbpath);
  retval = db_test(localdbpath);
  if(retval) {
    return(retval);
  }

  if(alpm_initialize() == -1) {
    fprintf(stderr, "cannot initialize alpm: %s\n", alpm_strerrorlast());
    return(1);
  }

  /* let us get log messages from libalpm */
  alpm_option_set_logcb(output_cb);

  alpm_option_set_dbpath(dbpath);

  db = alpm_db_register_local();
  if(db == NULL) {
    fprintf(stderr, "error: could not register 'local' database (%s)\n",
        alpm_strerrorlast());
    cleanup(EXIT_FAILURE);
  }

  /* check dependencies */
	alpm_list_t *data;
  data = alpm_checkdeps(db, PM_TRANS_TYPE_ADD, alpm_db_getpkgcache(db));
  for(i = data; i; i = alpm_list_next(i)) {
    pmdepmissing_t *miss = alpm_list_getdata(i);
    pmdepend_t *dep = alpm_miss_get_dep(miss);
    char *depstring = alpm_dep_get_string(dep);
    printf("missing dependency for %s : %s\n", alpm_miss_get_target(miss),
        depstring);
    free(depstring);
  }

  /* check requiredby */
  for(i = alpm_db_getpkgcache(db); i; i = alpm_list_next(i)) {
    pmpkg_t *pkg = alpm_list_getdata(i);
    const char *pkgname = alpm_pkg_get_name(pkg);
    alpm_list_t *rqdby = alpm_pkg_compute_requiredby(pkg);
    diffrqdby(pkgname, alpm_pkg_get_requiredby(pkg), rqdby);
  }

  cleanup(retval);
}
