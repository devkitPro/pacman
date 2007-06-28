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
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
#  USA.


import sys
import os
import md5
import stat


# ALPM
PM_ROOT     = "/"
PM_DBPATH   = "var/lib/pacman"
PM_LOCK     = "var/lib/pacman/db.lck"
PM_CACHEDIR = "var/cache/pacman/pkg"
PM_EXT_PKG  = ".pkg.tar.gz"
PM_EXT_DB   = ".db.tar.gz"
PM_PACNEW   = ".pacnew"
PM_PACORIG  = ".pacorig"
PM_PACSAVE  = ".pacsave"

# Pacman
PACCONF     = "etc/pacman.conf"

# Pactest
TMPDIR      = "tmp"
SYNCREPO    = "var/pub"
LOGFILE     = "var/log/pactest.log"

verbose = 0

def err(msg):
    print "error: " + msg
    sys.exit(1)

def vprint(msg):
    if verbose:
        print msg

#
# Methods to generate files
#

def getfilename(name):
    """
    """
    filename = ""
    link = ""
    if name.find(" -> ") != -1:
        filename, link = name.split(" -> ")
    elif name[-1] == "*":
        filename = name.rstrip("*")
    else:
        filename = name
    return filename

def mkfile(name, data = ""):
    """
    """

    isaltered = 0
    isdir = 0
    islink = 0
    link = ""
    filename = ""

    if name.find(" -> ") != -1:
        islink = 1
        filename, link = name.split(" -> ")
    elif name[-1] == "*":
        isaltered = 1
        filename = name.rstrip("*")
    else:
        filename = name
    if name[-1] == "/":
        isdir = 1

    if isdir:
        path = filename
    else:
        path = os.path.dirname(filename)
    try:
        if path and not os.path.isdir(path):
            os.makedirs(path, 0755)
    except:
        error("mkfile: could not create directory hierarchy '%s'" % path)

    if isdir:
        return
    if islink:
        curdir = os.getcwd()
        os.chdir(path)
        os.symlink(link, os.path.basename(filename))
        os.chdir(curdir)
    else:
        fd = file(filename, "w")
        if data:
            fd.write(data)
            if data[-1] != "\n":
                fd.write("\n")
        fd.close()

def mkdescfile(filename, pkg):
    """
    """

    data = []

    # desc
    #data.append("pkgname = %s" % pkg.name)
    #data.append("pkgver = %s" % pkg.version)
    if pkg.desc:
        data.append("pkgdesc = %s" % pkg.desc)
    if pkg.url:
        data.append("url = %s" % pkg.url)
    if pkg.builddate:
        data.append("builddate = %s" % pkg.builddate)
    if pkg.packager:
        data.append("packager = %s" % pkg.packager)
    if pkg.size:
        data.append("size = %s" % pkg.size)
    if pkg.arch:
        data.append("arch = %s" % pkg.arch)
    for i in pkg.groups:
        data.append("group = %s" % i)
    for i in pkg.license:
        data.append("license = %s" % i)
    if pkg.md5sum:
        data.append("md5sum = %s" % pkg.md5sum)

    # depends
    for i in pkg.replaces:
        data.append("replaces = %s" % i)
    for i in pkg.depends:
        data.append("depend = %s" % i)
    for i in pkg.conflicts:
        data.append("conflict = %s" % i)
    for i in pkg.provides:
        data.append("provides = %s" % i)
    for i in pkg.backup:
        data.append("backup = %s" % i)
    if pkg.force:
        data.append("force = 1")

    mkfile(filename, "\n".join(data))

def mkinstallfile(filename, install):
    """
    """
    data = []
    for key, value in install.iteritems():
        if value:
            data.append("%s() {\n%s\n}" % (key, value))
            
    mkfile(filename, "\n".join(data))

def mkcfgfile(filename, root, option, db):
    """
    """
    # Options
    data = ["[options]"]
    for key, value in option.iteritems():
        data.extend(["%s = %s" % (key, j) for j in value])

    # Repositories
    data.extend(["[%s]\n" \
                 "server = file://%s\n" \
                 % (value.treename, \
                    os.path.join(root, SYNCREPO, value.treename)) \
                 for key, value in db.iteritems() if key != "local"])

    mkfile(os.path.join(root, filename), "\n".join(data))


#
# MD5 helpers
#

def getmd5sum(filename):
    """
    """
    fd = open(filename, "rb")
    checksum = md5.new()
    while 1:
        block = fd.read(1048576)
        if not block:
            break
        checksum.update(block)
    fd.close()
    digest = checksum.digest()
    return "%02x"*len(digest) % tuple(map(ord, digest))

def mkmd5sum(data):
    """
    """
    checksum = md5.new()
    checksum.update("%s\n" % data)
    digest = checksum.digest()
    return "%02x"*len(digest) % tuple(map(ord, digest))


#
# Mtime helpers
#

def getmtime(filename):
    """
    """
    st = os.stat(filename)
    return st[stat.ST_ATIME], st[stat.ST_MTIME], st[stat.ST_CTIME]

def diffmtime(mt1, mt2):
    """ORE: TBD
    """
    return not mt1 == mt2


#
# Miscellaneous
#

def grep(filename, pattern):
    lines = file(filename, 'r').readlines()
    for line in lines:
        if not line: break
        if line.find(pattern) != -1:
            return True
    return False

def mkdir(dir):
    if os.path.isdir(dir):
        return
    elif os.path.isfile(dir):
        raise OSError("'%s' already exists and is not a directory" % dir)
    else:
        parent, thisdir = os.path.split(dir)
        if parent: mkdir(parent) #recurse to make all parents
        vprint("making dir %s" % thisdir)
        if thisdir: os.mkdir(dir)

if __name__ == "__main__":
    pass
# vim: set ts=4 sw=4 et:
