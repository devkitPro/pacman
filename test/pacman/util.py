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
import re
import hashlib


# ALPM
PM_ROOT     = "/"
PM_DBPATH   = "var/lib/pacman"
PM_SYNCDBPATH = "var/lib/pacman/sync"
PM_LOCK     = "var/lib/pacman/db.lck"
PM_CACHEDIR = "var/cache/pacman/pkg"
PM_EXT_PKG  = ".pkg.tar.gz"

# Pacman
PACCONF     = "etc/pacman.conf"

# Pactest
TMPDIR      = "tmp"
SYNCREPO    = "var/pub"
LOGFILE     = "var/log/pactest.log"

verbose = 0

def vprint(msg):
    if verbose:
        print msg

#
# Methods to generate files
#

def mkfile(name, data = ""):
    isdir = 0
    islink = 0
    setperms = 0
    filename = name
    link = ""
    perms = ""

    if filename[-1] == "*":
        filename = filename.rstrip("*")
    if filename.find(" -> ") != -1:
        islink = 1
        filename, link = filename.split(" -> ")
    elif filename.find("|") != -1:
        setperms = 1
        filename, perms = filename.split("|")
    if filename[-1] == "/":
        isdir = 1

    if isdir:
        path = filename
    else:
        path = os.path.dirname(filename)
    if path and not os.path.isdir(path):
        os.makedirs(path, 0755)

    if isdir:
        return
    if islink:
        curdir = os.getcwd()
        if path:
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
        if setperms:
            os.chmod(filename, int(perms, 8))

def mkcfgfile(filename, root, option, db):
    # Options
    data = ["[options]"]
    for key, value in option.iteritems():
        data.extend(["%s = %s" % (key, j) for j in value])

    # Repositories
    # sort by repo name so tests can predict repo order, rather than be
    # subjects to the whims of python dict() ordering
    for key in sorted(db.iterkeys()):
        if key != "local":
            value = db[key]
            data.append("[%s]\n" \
                    "VerifySig = %s\n" \
                    "Server = file://%s" \
                     % (value.treename, value.getverify(), \
                        os.path.join(root, SYNCREPO, value.treename)))
            for optkey, optval in value.option.iteritems():
                data.extend(["%s = %s" % (optkey, j) for j in optval])

    mkfile(os.path.join(root, filename), "\n".join(data))


#
# MD5 helpers
#

def getmd5sum(filename):
    if not os.path.isfile(filename):
        return ""
    fd = open(filename, "rb")
    checksum = hashlib.md5()
    while 1:
        block = fd.read(32 * 1024)
        if not block:
            break
        checksum.update(block)
    fd.close()
    return checksum.hexdigest()

def mkmd5sum(data):
    checksum = hashlib.md5()
    checksum.update("%s\n" % data)
    return checksum.hexdigest()


#
# Miscellaneous
#

def which(filename):
    path = os.environ["PATH"].split(':')
    for p in path:
        f = os.path.join(p, filename)
        if os.access(f, os.F_OK):
            return f
    return None

def grep(filename, pattern):
    pat = re.compile(pattern)
    myfile = open(filename, 'r')
    for line in myfile:
        if pat.search(line):
            myfile.close()
            return True
    myfile.close()
    return False

def mkdir(path):
    if os.path.isdir(path):
        return
    elif os.path.isfile(path):
        raise OSError("'%s' already exists and is not a directory" % path)
    os.makedirs(path, 0755)

# vim: set ts=4 sw=4 et:
