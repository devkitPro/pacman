#! /usr/bin/python
#
#  Copyright (c) 2006 by Aurelien Foret <orelien@chez.com>
# 
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.


import os
import tempfile
import shutil
import tarfile

import pmpkg
from util import *


def _mkfilelist(files):
    """Generate a list of files from the list supplied as an argument.
    
    Each path is decomposed to generate the list of all directories leading
    to the file.
    
    Example with 'usr/local/bin/dummy':
    The resulting list will be
        usr/
        usr/local/
        usr/local/bin/
        usr/local/bin/dummy
    """
    i = []
    for f in files:
        dir = getfilename(f)
        i.append(dir)
        while "/" in dir:
            [dir, tmp] = dir.rsplit("/", 1)
            if not dir + "/" in files:
                i.append(dir + "/")
    i.sort()
    return i

def _mkbackuplist(backup):
    """
    """
    return ["%s\t%s" % (getfilename(i), mkmd5sum(i)) for i in backup]

def _getsection(fd):
    """
    """
    i = []
    while 1:
        line = fd.readline().strip("\n")
        if not line:
            break
        i.append(line)
    return i

def _mksection(title, data):
    """
    """
    s = ""
    if isinstance(data, list):
        s = "\n".join(data)
    else:
        s = data
    return "%%%s%%\n" \
           "%s\n" % (title, s)


class pmdb:
    """Database object
    """

    def __init__(self, treename, dbdir):
        self.treename = treename
        self.dbdir = dbdir
        self.pkgs = []

    def __str__(self):
        return "%s" % self.treename

    def getpkg(self, name):
        """
        """
        for pkg in self.pkgs:
            if name == pkg.name:
                return pkg

    def db_read(self, name):
        """
        """

        path = os.path.join(self.dbdir, self.treename)
        if not os.path.isdir(path):
            return None

        dbentry = ""
        for roots, dirs, files in os.walk(path):
            for i in dirs:
                [pkgname, pkgver, pkgrel] = i.rsplit("-", 2)
                if pkgname == name:
                    dbentry = i
                    break
        if not dbentry:
            return None
        path = os.path.join(path, dbentry)

        [pkgname, pkgver, pkgrel] = dbentry.rsplit("-", 2)
        pkg = pmpkg.pmpkg(pkgname, pkgver + "-" + pkgrel)

        # desc
        filename = os.path.join(path, "desc")
        if not os.path.isfile(filename):
            print "invalid db entry found (desc missing) for pkg", pkgname
            return None
        fd = open(filename, "r")
        while 1:
            line = fd.readline()
            if not line:
                break
            line = line.strip("\n")
            if line == "%DESC%":
                pkg.desc = fd.readline().strip("\n")
            elif line == "%GROUPS%":
                pkg.groups = _getsection(fd)
            elif line == "%URL%":
                pkg.url = fd.readline().strip("\n")
            elif line == "%LICENSE%":
                pkg.license = _getsection(fd)
            elif line == "%ARCH%":
                pkg.arch = fd.readline().strip("\n")
            elif line == "%BUILDDATE%":
                pkg.builddate = fd.readline().strip("\n")
            elif line == "%INSTALLDATE%":
                pkg.installdate = fd.readline().strip("\n")
            elif line == "%PACKAGER%":
                pkg.packager = fd.readline().strip("\n")
            elif line == "%REASON%":
                pkg.reason = int(fd.readline().strip("\n"))
            elif line == "%SIZE%" or line == "%CSIZE%":
                pkg.size = int(fd.readline().strip("\n"))
            elif line == "%MD5SUM%":
                pkg.md5sum = fd.readline().strip("\n")
            elif line == "%REPLACES%":
                pkg.replaces = _getsection(fd)
            elif line == "%FORCE%":
                fd.readline()
                pkg.force = 1
        fd.close()
        pkg.checksum["desc"] = getmd5sum(filename)
        pkg.mtime["desc"] = getmtime(filename)

        # files
        filename = os.path.join(path, "files")
        if not os.path.isfile(filename):
            print "invalid db entry found (files missing) for pkg", pkgname
            return None
        fd = open(filename, "r")
        while 1:
            line = fd.readline()
            if not line:
                break
            line = line.strip("\n")
            if line == "%FILES%":
                while line:
                    line = fd.readline().strip("\n")
                    if line and line[-1] != "/":
                        pkg.files.append(line)
            if line == "%BACKUP%":
                pkg.backup = _getsection(fd)
        fd.close()
        pkg.checksum["files"] = getmd5sum(filename)
        pkg.mtime["files"] = getmtime(filename)

        # depends
        filename = os.path.join(path, "depends")
        if not os.path.isfile(filename):
            print "invalid db entry found (depends missing) for pkg", pkgname
            return None
        fd = file(filename, "r")
        while 1:
            line = fd.readline()
            if not line:
                break
            line = line.strip("\n")
            if line == "%DEPENDS%":
                pkg.depends = _getsection(fd)
            elif line == "%OPTDEPENDS%":
                pkg.optdepends = _getsection(fd)
            elif line == "%CONFLICTS%":
                pkg.conflicts = _getsection(fd)
            elif line == "%PROVIDES%":
                pkg.provides = _getsection(fd)
            # TODO this was going to be changed, but isn't anymore
            #elif line == "%REPLACES%":
            #    pkg.replaces = _getsection(fd)
            #elif line == "%FORCE%":
            #    fd.readline()
            #    pkg.force = 1
        fd.close()
        pkg.checksum["depends"] = getmd5sum(filename)
        pkg.mtime["depends"] = getmtime(filename)

        # install
        filename = os.path.join(path, "install")
        if os.path.isfile(filename):
            pkg.checksum["install"] = getmd5sum(filename)
            pkg.mtime["install"] = getmtime(filename)

        return pkg

    #
    # db_write is used to add both 'local' and 'sync' db entries
    #
    def db_write(self, pkg):
        """
        """

        if self.treename == "local":
            path = os.path.join(self.dbdir, self.treename, pkg.fullname())
        else:
            path = os.path.join(self.dbdir, "sync", self.treename, pkg.fullname())
        mkdir(path)

        # desc
        # for local db entries: name, version, desc, groups, url, license,
        #                       arch, builddate, installdate, packager,
        #                       size, reason
        # for sync entries: name, version, desc, groups, csize, md5sum,
        #                   replaces, force
        data = [_mksection("NAME", pkg.name)]
        data.append(_mksection("VERSION", pkg.version))
        if pkg.desc:
            data.append(_mksection("DESC", pkg.desc))
        if pkg.groups:
            data.append(_mksection("GROUPS", pkg.groups))
        if pkg.license:
            data.append(_mksection("LICENSE", pkg.license))
        if pkg.arch:
            data.append(_mksection("ARCH", pkg.arch))
        if pkg.builddate:
            data.append(_mksection("BUILDDATE", pkg.builddate))
        if pkg.packager:
            data.append(_mksection("PACKAGER", pkg.packager))
        if self.treename == "local":
            if pkg.url:
                data.append(_mksection("URL", pkg.url))
            if pkg.installdate:
                data.append(_mksection("INSTALLDATE", pkg.installdate))
            if pkg.size:
                data.append(_mksection("SIZE", pkg.size))
            if pkg.reason:
                data.append(_mksection("REASON", pkg.reason))
        else:
            data.append(_mksection("FILENAME", pkg.filename()))
            if pkg.replaces:
                data.append(_mksection("REPLACES", pkg.replaces))
            if pkg.force:
                data.append(_mksection("FORCE", ""))
            if pkg.csize:
                data.append(_mksection("CSIZE", pkg.csize))
            if pkg.md5sum:
                data.append(_mksection("MD5SUM", pkg.md5sum))
        if data:
            data.append("")
        filename = os.path.join(path, "desc")
        mkfile(filename, "\n".join(data))
        pkg.checksum["desc"] = getmd5sum(filename)
        pkg.mtime["desc"] = getmtime(filename)

        # files
        # for local entries, fields are: files, backup
        # for sync ones: none
        if self.treename == "local":
            data = []
            if pkg.files:
                data.append(_mksection("FILES", _mkfilelist(pkg.files)))
            if pkg.backup:
                data.append(_mksection("BACKUP", _mkbackuplist(pkg.backup)))
            if data:
                data.append("")
            filename = os.path.join(path, "files")
            mkfile(filename, "\n".join(data))
            pkg.checksum["files"] = getmd5sum(filename)
            pkg.mtime["files"] = getmtime(filename)

        # depends
        # for local db entries: depends, conflicts, provides
        # for sync ones: depends, conflicts, provides
        data = []
        if pkg.depends:
            data.append(_mksection("DEPENDS", pkg.depends))
        if pkg.optdepends:
            data.append(_mksection("OPTDEPENDS", pkg.optdepends))
        if pkg.conflicts:
            data.append(_mksection("CONFLICTS", pkg.conflicts))
        if pkg.provides:
            data.append(_mksection("PROVIDES", pkg.provides))
        #if self.treename != "local":
        #    if pkg.replaces:
        #        data.append(_mksection("REPLACES", pkg.replaces))
        #    if pkg.force:
        #        data.append(_mksection("FORCE", ""))
        if data:
            data.append("")
        filename = os.path.join(path, "depends")
        mkfile(filename, "\n".join(data))
        pkg.checksum["depends"] = getmd5sum(filename)
        pkg.mtime["depends"] = getmtime(filename)

        # install
        if self.treename == "local":
            empty = 1
            for value in pkg.install.values():
                if value:
                    empty = 0
            if not empty:
                filename = os.path.join(path, "install")
                mkinstallfile(filename, pkg.install)
                pkg.checksum["install"] = getmd5sum(filename)
                pkg.mtime["install"] = getmtime(filename)

    def gensync(self, path):
        """
        """

        curdir = os.getcwd()
        tmpdir = tempfile.mkdtemp()
        os.chdir(tmpdir)

        for pkg in self.pkgs:
            mkdescfile(pkg.fullname(), pkg)

        # Generate database archive
        mkdir(path)
        archive = os.path.join(path, "%s%s" % (self.treename, PM_EXT_DB))
        tar = tarfile.open(archive, "w:gz")
        for root, dirs, files in os.walk('.'):
            for d in dirs:
                tar.add(os.path.join(root, d), recursive=False)
            for f in files:
                tar.add(os.path.join(root, f))
        tar.close()

        os.chdir(curdir)
        shutil.rmtree(tmpdir)

    def ispkgmodified(self, pkg):
        """
        """

        modified = 0

        oldpkg = self.getpkg(pkg.name)
        if not oldpkg:
            return 0

        vprint("\toldpkg.checksum : %s" % oldpkg.checksum)
        vprint("\toldpkg.mtime    : %s" % oldpkg.mtime)

        for key in pkg.mtime.keys():
            if key == "install" \
               and oldpkg.mtime[key] == (0, 0, 0) \
               and pkg.mtime[key] == (0, 0, 0):
                continue
            if oldpkg.mtime[key][1:3] != pkg.mtime[key][1:3]:
                modified += 1

        return modified


if __name__ == "__main__":
    db = pmdb("local")
    print db
# vim: set ts=4 sw=4 et:
