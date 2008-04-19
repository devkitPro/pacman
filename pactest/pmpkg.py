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
import stat
import shutil
import tarfile

from util import *


class pmpkg:
    """Package object.
    
    Object holding data from an ArchLinux package.
    """

    def __init__(self, name, version = "1.0-1"):
        self.path = "" #the path of the generated package
        # desc
        self.name = name
        self.version = version
        self.desc = ""
        self.groups = []
        self.url = ""
        self.license = []
        self.arch = ""
        self.builddate = ""
        self.installdate = ""
        self.packager = ""
        self.size = 0
        self.csize = 0
        self.reason = 0
        self.md5sum = ""      # sync only
        self.replaces = []    # sync only (will be moved to depends)
        self.force = 0        # sync only (will be moved to depends)
        # depends
        self.depends = []
        self.optdepends = []
        self.conflicts = []
        self.provides = []
        # files
        self.files = []
        self.backup = []
        # install
        self.install = {
            "pre_install": "",
            "post_install": "",
            "pre_remove": "",
            "post_remove": "",
            "pre_upgrade": "",
            "post_upgrade": ""
        }
        self.checksum = {
            "desc": "",
            "depends": "",
            "files": "",
            "install": ""
        }
        self.mtime = {
            "desc": (0, 0, 0),
            "depends": (0, 0, 0),
            "files": (0, 0, 0),
            "install": (0, 0, 0)
        }

    def __str__(self):
        s = ["%s" % self.fullname()]
        s.append("description: %s" % self.desc)
        s.append("url: %s" % self.url)
        s.append("depends: %s" % " ".join(self.depends))
        s.append("files: %s" % " ".join(self.files))
        s.append("reason: %d" % self.reason)
        return "\n".join(s)

    def fullname(self):
        """Long name of a package.
        
        Returns a string formatted as follows: "pkgname-pkgver".
        """
        return "%s-%s" % (self.name, self.version)

    def filename(self):
        """File name of a package, including its extension.
        
        Returns a string formatted as follows: "pkgname-pkgver.PKG_EXT_PKG".
        """
        return "%s%s" % (self.fullname(), PM_EXT_PKG)

    def install_files(self, root):
        """Install files in the filesystem located under "root".
        
        Files are created with content generated automatically.
        """
        [mkfile(os.path.join(root, f), f) for f in self.files]

    def makepkg(self, path):
        """Creates an ArchLinux package archive.
        
        A package archive is generated in the location 'path', based on the data
        from the object.
        """
        self.path = os.path.join(path, self.filename())

        curdir = os.getcwd()
        tmpdir = tempfile.mkdtemp()
        os.chdir(tmpdir)

        # Generate package file system
        for f in self.files:
            mkfile(f, f)
            self.size += os.stat(getfilename(f))[stat.ST_SIZE]

        # .PKGINFO
        data = ["pkgname = %s" % self.name]
        data.append("pkgver = %s" % self.version)
        data.append("pkgdesc = %s" % self.desc)
        data.append("url = %s" % self.url)
        data.append("builddate = %s" % self.builddate)
        data.append("packager = %s" % self.packager)
        data.append("size = %s" % self.size)
        if self.arch:
            data.append("arch = %s" % self.arch)
        for i in self.license:
            data.append("license = %s" % i)
        for i in self.replaces:
            data.append("replaces = %s" % i)
        for i in self.groups:
            data.append("group = %s" % i)
        for i in self.depends:
            data.append("depend = %s" % i)
        for i in self.optdepends:
            data.append("optdepend = %s" % i)
        for i in self.conflicts:
            data.append("conflict = %s" % i)
        for i in self.provides:
            data.append("provides = %s" % i)
        for i in self.backup:
            data.append("backup = %s" % i)
        mkfile(".PKGINFO", "\n".join(data))

        # .INSTALL
        if len(self.install.values()) > 0:
            mkinstallfile(".INSTALL", self.install)

        # safely create the dir
        mkdir(os.path.dirname(self.path))

        # Generate package archive
        tar = tarfile.open(self.path, "w:gz")

        # package files
        for root, dirs, files in os.walk('.'):
            for d in dirs:
                tar.add(os.path.join(root, d), recursive=False)
            for f in files:
                tar.add(os.path.join(root, f))

        tar.close()

        os.chdir(curdir)
        shutil.rmtree(tmpdir)


if __name__ == "__main__":
    pkg = pmpkg("dummy")
    print pkg
# vim: set ts=4 sw=4 et:
