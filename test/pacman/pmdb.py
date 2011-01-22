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
import tarfile

import pmpkg
import util

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
    file_set = set()
    for f in files:
        name = util.getfilename(f)
        file_set.add(name)
        while "/" in name:
            [name, tmp] = name.rsplit("/", 1)
            file_set.add(name + "/")
    return sorted(file_set)

def _mkbackuplist(backup):
    """
    """
    return ["%s\t%s" % (util.getfilename(i), util.mkmd5sum(i)) for i in backup]

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


class pmdb(object):
    """Database object
    """

    def __init__(self, treename, root):
        self.treename = treename
        self.pkgs = []
        self.option = {}
        if self.treename == "local":
            self.dbdir = os.path.join(root, util.PM_DBPATH, treename)
        else:
            self.dbdir = os.path.join(root, util.PM_SYNCDBPATH, treename)
            self.dbfile = os.path.join(root, util.PM_SYNCDBPATH, treename + ".db")

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
        path = self.dbdir
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
            elif line == "%DEPENDS%":
                pkg.depends = _getsection(fd)
            elif line == "%OPTDEPENDS%":
                pkg.optdepends = _getsection(fd)
            elif line == "%CONFLICTS%":
                pkg.conflicts = _getsection(fd)
            elif line == "%PROVIDES%":
                pkg.provides = _getsection(fd)
        fd.close()

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

        # install
        filename = os.path.join(path, "install")

        return pkg

    #
    # db_write is used to add both 'local' and 'sync' db entries
    #
    def db_write(self, pkg):
        """
        """
        path = os.path.join(self.dbdir, pkg.fullname())
        util.mkdir(path)

        # desc
        # for local db entries: name, version, desc, groups, url, license,
        #                       arch, builddate, installdate, packager,
        #                       size, reason, depends, conflicts, provides
        # for sync entries: name, version, desc, groups, csize, md5sum,
        #                   replaces, force, depends, conflicts, provides
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
        if pkg.depends:
            data.append(_mksection("DEPENDS", pkg.depends))
        if pkg.optdepends:
            data.append(_mksection("OPTDEPENDS", pkg.optdepends))
        if pkg.conflicts:
            data.append(_mksection("CONFLICTS", pkg.conflicts))
        if pkg.provides:
            data.append(_mksection("PROVIDES", pkg.provides))
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
            if pkg.csize:
                data.append(_mksection("CSIZE", pkg.csize))
            if pkg.md5sum:
                data.append(_mksection("MD5SUM", pkg.md5sum))
        if data:
            data.append("")
        filename = os.path.join(path, "desc")
        util.mkfile(filename, "\n".join(data))

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
            util.mkfile(filename, "\n".join(data))

        # install
        if self.treename == "local":
            empty = 1
            for value in pkg.install.values():
                if value:
                    empty = 0
            if not empty:
                filename = os.path.join(path, "install")
                util.mkinstallfile(filename, pkg.install)

    def gensync(self):
        """
        """

        if not self.dbfile:
            return
        curdir = os.getcwd()
        os.chdir(self.dbdir)

        # Generate database archive
        tar = tarfile.open(self.dbfile, "w:gz")
        for i in os.listdir("."):
            tar.add(i)
        tar.close()

        os.chdir(curdir)

# vim: set ts=4 sw=4 et:
