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
    return ["%s\t%s" % (util.getfilename(i), util.mkmd5sum(i)) for i in backup]

def _getsection(fd):
    i = []
    while 1:
        line = fd.readline().strip("\n")
        if not line:
            break
        i.append(line)
    return i

def make_section(data, title, values):
    if not values:
        return
    data.append("%%%s%%" % title)
    if isinstance(values, (list, tuple)):
        data.extend(str(item) for item in values)
    else:
        # just a single value
        data.append(str(values))
    data.append('\n')


class pmdb(object):
    """Database object
    """

    def __init__(self, treename, root):
        self.treename = treename
        self.pkgs = []
        self.option = {}
        if self.treename == "local":
            self.dbdir = os.path.join(root, util.PM_DBPATH, treename)
            self.dbfile = None
            self.is_local = True
        else:
            self.dbdir = os.path.join(root, util.PM_SYNCDBPATH, treename)
            # TODO: we should be doing this, don't need a sync db dir
            #self.dbdir = None
            self.dbfile = os.path.join(root, util.PM_SYNCDBPATH, treename + ".db")
            self.is_local = False

    def __str__(self):
        return "%s" % self.treename

    def getverify(self):
        for value in ("Always", "Never", "Optional"):
            if value in self.treename:
                return value
        return "Never"

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
                try:
                    pkg.reason = int(fd.readline().strip("\n"))
                except ValueError:
                    pkg.reason = -1
                    raise
            elif line == "%SIZE%" or line == "%CSIZE%":
                try:
                    pkg.size = int(fd.readline().strip("\n"))
                except ValueError:
                    pkg.size = -1
                    raise
            elif line == "%MD5SUM%":
                pkg.md5sum = fd.readline().strip("\n")
            elif line == "%PGPSIG%":
                pkg.pgpsig = fd.readline().strip("\n")
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
        path = os.path.join(self.dbdir, pkg.fullname())
        util.mkdir(path)

        # desc/depends type entries
        data = []
        make_section(data, "NAME", pkg.name)
        make_section(data, "VERSION", pkg.version)
        make_section(data, "DESC", pkg.desc)
        make_section(data, "GROUPS", pkg.groups)
        make_section(data, "LICENSE", pkg.license)
        make_section(data, "ARCH", pkg.arch)
        make_section(data, "BUILDDATE", pkg.builddate)
        make_section(data, "PACKAGER", pkg.packager)
        make_section(data, "DEPENDS", pkg.depends)
        make_section(data, "OPTDEPENDS", pkg.optdepends)
        make_section(data, "CONFLICTS", pkg.conflicts)
        make_section(data, "PROVIDES", pkg.provides)
        make_section(data, "URL", pkg.url)
        if self.is_local:
            make_section(data, "INSTALLDATE", pkg.installdate)
            make_section(data, "SIZE", pkg.size)
            make_section(data, "REASON", pkg.reason)
        else:
            make_section(data, "FILENAME", pkg.filename())
            make_section(data, "REPLACES", pkg.replaces)
            make_section(data, "CSIZE", pkg.csize)
            make_section(data, "ISIZE", pkg.isize)
            make_section(data, "MD5SUM", pkg.md5sum)
            make_section(data, "PGPSIG", pkg.pgpsig)

        filename = os.path.join(path, "desc")
        util.mkfile(filename, "\n".join(data))

        # files and install
        if self.is_local:
            data = []
            make_section(data, "FILES", _mkfilelist(pkg.files))
            make_section(data, "BACKUP", _mkbackuplist(pkg.backup))
            filename = os.path.join(path, "files")
            util.mkfile(filename, "\n".join(data))

            if any(pkg.install.values()):
                filename = os.path.join(path, "install")
                util.mkinstallfile(filename, pkg.install)

    def gensync(self):
        if not self.dbfile:
            return
        curdir = os.getcwd()
        os.chdir(self.dbdir)

        tar = tarfile.open(self.dbfile, "w:gz")
        for i in os.listdir("."):
            tar.add(i)
        tar.close()

        os.chdir(curdir)

# vim: set ts=4 sw=4 et:
