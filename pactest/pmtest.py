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


import os
import shutil
import time

import pmrule
import pmdb
import pmfile
from pmpkg import pmpkg
from util import *


class pmtest:
    """Test object
    """

    def __init__(self, name, root):
        self.name = name
        self.root = root

    def __str__(self):
        return "name = %s\n" \
               "root = %s" % (self.name, self.root)

    def addpkg2db(self, treename, pkg):
        """
        """
        if not treename in self.db:
            self.db[treename] = pmdb.pmdb(treename, os.path.join(self.root, PM_DBPATH))
        self.db[treename].pkgs.append(pkg)

    def addpkg(self, pkg):
        """
        """
        self.localpkgs.append(pkg)

    def addrule(self, rulename):
        """
        """
        rule = pmrule.pmrule(rulename)
        self.rules.append(rule)

    def load(self):
        """
        """

        # Reset test parameters
        self.result = {
            "success": 0,
            "fail": 0
        }
        self.args = ""
        self.retcode = 0
        self.db = {
            "local": pmdb.pmdb("local", os.path.join(self.root, PM_DBPATH))
        }
        self.localpkgs = []
        self.filesystem = []

        self.description = ""
        self.option = {
            "noupgrade": [],
            "ignorepkg": [],
            "noextract": []
        }

        # Test rules
        self.rules = []
        self.files = []
        
        if os.path.isfile(self.name):
            execfile(self.name)
        else:
            err("file %s does not exist!" % self.name)

    def generate(self):
        """
        """

        print "==> Generating test environment"

        # Cleanup leftover files from a previous test session
        if os.path.isdir(self.root):
            shutil.rmtree(self.root)
        vprint("\t%s" % self.root)

        # Create directory structure
        vprint("    Creating directory structure:")
        dbdir = os.path.join(self.root, PM_DBPATH)
        cachedir = os.path.join(self.root, PM_CACHEDIR)
        syncdir = os.path.join(self.root, SYNCREPO)
        tmpdir = os.path.join(self.root, TMPDIR)
        logdir = os.path.join(self.root, os.path.dirname(LOGFILE))
        etcdir = os.path.join(self.root, os.path.dirname(PACCONF))
        for dir in [dbdir, cachedir, syncdir, tmpdir, logdir, etcdir]:
            if not os.path.isdir(dir):
                vprint("\t%s" % dir[len(self.root)+1:])
                os.makedirs(dir, 0755)

        # Configuration file
        vprint("    Creating configuration file")
        vprint("\t%s" % PACCONF)
        mkcfgfile(PACCONF, self.root, self.option, self.db)

        # Creating packages
        vprint("    Creating package archives")
        for pkg in self.localpkgs:
            vprint("\t%s" % os.path.join(TMPDIR, pkg.filename()))
            pkg.makepkg(tmpdir)
        for key, value in self.db.iteritems():
            if key == "local":
                continue
            for pkg in value.pkgs:
                archive = pkg.filename()
                vprint("\t%s" % os.path.join(PM_CACHEDIR, archive))
                pkg.makepkg(cachedir)
                pkg.md5sum = getmd5sum(os.path.join(cachedir, archive))
                pkg.csize = os.stat(os.path.join(cachedir, archive))[stat.ST_SIZE]

        # Populating databases
        vprint("    Populating databases")
        for key, value in self.db.iteritems():
            for pkg in value.pkgs:
                vprint("\t%s/%s" % (key, pkg.fullname()))
                if key == "local":
                    pkg.installdate = time.ctime()
                value.db_write(pkg)

        # Creating sync database archives
        vprint("    Creating sync database archives")
        for key, value in self.db.iteritems():
            if key == "local":
                continue
            archive = value.treename + PM_EXT_DB
            vprint("\t" + os.path.join(SYNCREPO, archive))
            value.gensync(os.path.join(syncdir, value.treename))

        # Filesystem
        vprint("    Populating file system")
        for pkg in self.db["local"].pkgs:
            vprint("\tinstalling %s" % pkg.fullname())
            pkg.install_files(self.root)
        for f in self.filesystem:
            vprint("\t%s" % f)
            mkfile(os.path.join(self.root, f), f)

        # Done.
        vprint("    Taking a snapshot of the file system")
        for roots, dirs, files in os.walk(self.root):
            for i in files:
                filename = os.path.join(roots, i)
                f = pmfile.pmfile(self.root, filename.replace(self.root + "/", ""))
                self.files.append(f)
                vprint("\t%s" % f.name)

    def run(self, pacman):
        """
        """

        if os.path.isfile(PM_LOCK):
            print "\tERROR: another pacman session is on-going -- skipping"
            return

        print "==> Running test"
        vprint("\tpacman %s" % self.args)

        cmd = ["fakeroot"]
        if pacman["gdb"]:
            cmd.append("libtool gdb --args")
        if pacman["valgrind"]:
            cmd.append("valgrind --tool=memcheck --leak-check=full --show-reachable=yes")
        if not pacman["manual-confirm"]:
            cmd.append("--noconfirm")
        cmd.append("%s --config=%s --root=%s" \
                   % (pacman["bin"], os.path.join(self.root, PACCONF), self.root))
        if pacman["debug"]:
            cmd.append("--debug=%s" % pacman["debug"])
        cmd.append("%s" % self.args)
        if not pacman["gdb"] and not pacman["valgrind"] and not pacman["nolog"]: 
            cmd.append(">%s 2>&1" % os.path.join(self.root, LOGFILE))
        dbg(" ".join(cmd))

        # Change to the tmp dir before running pacman, so that local package
        # archives are made available more easily.
        curdir = os.getcwd()
        tmpdir = os.path.join(self.root, TMPDIR)
        os.chdir(tmpdir)

        t0 = time.time()
        self.retcode = os.system(" ".join(cmd))
        t1 = time.time()
        vprint("\ttime elapsed: %ds" % (t1-t0))

        if self.retcode == None:
            self.retcode = 0
        else:
            self.retcode /= 256
        dbg("retcode = %s" % self.retcode)
        os.chdir(curdir)

        # Check if pacman failed because of bad permissions
        if self.retcode and not pacman["nolog"] \
           and grep(os.path.join(self.root, LOGFILE),
                    "you cannot perform this operation unless you are root"):
            print "\tERROR: pacman support for fakeroot is not disabled"
        # Check if the lock is still there
        if os.path.isfile(PM_LOCK):
            print "\tERROR: %s not removed" % PM_LOCK
            os.unlink(PM_LOCK)
        # Look for a core file
        if os.path.isfile(os.path.join(self.root, TMPDIR, "core")):
            print "\tERROR: pacman dumped a core file"

    def check(self):
        """
        """

        print "==> Checking rules"

        for i in self.rules:
            success = i.check(self.root, self.retcode, self.db["local"], self.files)
            if success == 1:
                msg = " OK "
                self.result["success"] += 1
            elif success == 0:
                msg = "FAIL"
                self.result["fail"] += 1
            else:
                msg = "SKIP"
            print "\t[%s] %s" % (msg, i.rule)
            i.result = success


if __name__ == "__main__":
    test = pmtest("test1", "./root")
    print test
# vim: set ts=4 sw=4 et:
