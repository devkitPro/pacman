/*
 *  package.h
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
#ifndef _PM_PACKAGE_H
#define _PM_PACKAGE_H

void dump_pkg_full(PM_PKG *pkg, int level);
void dump_pkg_sync(PM_PKG *pkg, char *treename);

void dump_pkg_files(PM_PKG *pkg);
void dump_pkg_changelog(char *clfile, char *pkgname);

int split_pkgname(char *target, char *name, char *version);

#define FREEPKG(p) { alpm_pkg_free(p); p = NULL; }

#endif /* _PM_PACKAGE_H */

/* vim: set ts=2 sw=2 noet: */
