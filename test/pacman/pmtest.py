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
import os.path
import shutil
import stat
import time

import pmrule
import pmdb
import pmfile
import util
from util import vprint

class pmtest(object):
    """Test object
    """

    def __init__(self, name, root):
        self.name = name
        self.testname = os.path.basename(name).replace('.py', '')
        self.root = root
        self.cachepkgs = True

    def __str__(self):
        return "name = %s\n" \
               "testname = %s\n" \
               "root = %s" % (self.name, self.testname, self.root)

    def addpkg2db(self, treename, pkg):
        """
        """
        if not treename in self.db:
            self.db[treename] = pmdb.pmdb(treename, self.root)
        self.db[treename].pkgs.append(pkg)

    def addpkg(self, pkg):
        """
        """
        self.localpkgs.append(pkg)

    def findpkg(self, name, version, allow_local=False):
        """Find a package object matching the name and version specified in
        either sync databases or the local package collection. The local database
        is allowed to match if allow_local is True."""
        for db in self.db.itervalues():
            if db.treename == "local" and not allow_local:
                continue
            pkg = db.getpkg(name)
            if pkg and pkg.version == version:
                return pkg
        for pkg in self.localpkgs:
            if pkg.name == name and pkg.version == version:
                return pkg

        return None

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
            "local": pmdb.pmdb("local", self.root)
        }
        self.localpkgs = []
        self.createlocalpkgs = False
        self.filesystem = []

        self.description = ""
        self.option = {}

        # Test rules
        self.rules = []
        self.files = []
        self.expectfailure = False
        
        if os.path.isfile(self.name):
            # all tests expect this to be available
            from pmpkg import pmpkg
            execfile(self.name)
        else:
            raise IOError("file %s does not exist!" % self.name)

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
        dbdir = os.path.join(self.root, util.PM_DBPATH)
        cachedir = os.path.join(self.root, util.PM_CACHEDIR)
        syncdir = os.path.join(self.root, util.SYNCREPO)
        tmpdir = os.path.join(self.root, util.TMPDIR)
        logdir = os.path.join(self.root, os.path.dirname(util.LOGFILE))
        etcdir = os.path.join(self.root, os.path.dirname(util.PACCONF))
        bindir = os.path.join(self.root, "bin")
        sys_dirs = [dbdir, cachedir, syncdir, tmpdir, logdir, etcdir, bindir]
        for sys_dir in sys_dirs:
            if not os.path.isdir(sys_dir):
                vprint("\t%s" % sys_dir[len(self.root)+1:])
                os.makedirs(sys_dir, 0755)
        # Only the dynamically linked binary is needed for fakechroot
        shutil.copy("/bin/sh", bindir)

        # Configuration file
        vprint("    Creating configuration file")
        util.mkcfgfile(util.PACCONF, self.root, self.option, self.db)

        # Creating packages
        vprint("    Creating package archives")
        for pkg in self.localpkgs:
            vprint("\t%s" % os.path.join(util.TMPDIR, pkg.filename()))
            pkg.makepkg(tmpdir)
        for key, value in self.db.iteritems():
            if key == "local" and not self.createlocalpkgs:
                continue
            for pkg in value.pkgs:
                vprint("\t%s" % os.path.join(util.PM_CACHEDIR, pkg.filename()))
                if self.cachepkgs:
                    pkg.makepkg(cachedir)
                else:
                    pkg.makepkg(os.path.join(syncdir, value.treename))
                pkg.md5sum = util.getmd5sum(pkg.path)
                pkg.csize = os.stat(pkg.path)[stat.ST_SIZE]

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
            vprint("\t" + value.treename)
            value.gensync()
            serverpath = os.path.join(syncdir, value.treename)
            util.mkdir(serverpath)
            shutil.copy(value.dbfile, serverpath)

        # Filesystem
        vprint("    Populating file system")
        for pkg in self.db["local"].pkgs:
            vprint("\tinstalling %s" % pkg.fullname())
            for f in pkg.files:
                vprint("\t%s" % f)
                path = os.path.join(self.root, f)
                util.mkfile(path, f)
                if os.path.isfile(path):
                    os.utime(path, (355, 355))
        for f in self.filesystem:
            vprint("\t%s" % f)
            path = os.path.join(self.root, f)
            util.mkfile(path, f)
            if os.path.isfile(path):
                os.utime(path, (355, 355))

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

        if os.path.isfile(util.PM_LOCK):
            print "\tERROR: another pacman session is on-going -- skipping"
            return

        print "==> Running test"
        vprint("\tpacman %s" % self.args)

        cmd = [""]
        if os.geteuid() != 0:
            fakeroot = util.which("fakeroot")
            if not fakeroot:
                print "WARNING: fakeroot not found!"
            else:
                cmd.append("fakeroot")

            fakechroot = util.which("fakechroot")
            if fakechroot:
                cmd.append("fakechroot")

        if pacman["gdb"]:
            cmd.append("libtool execute gdb --args")
        if pacman["valgrind"]:
            cmd.append("valgrind -q --tool=memcheck --leak-check=full --show-reachable=yes --suppressions=%s/valgrind.supp" % os.getcwd())
        cmd.append("\"%s\" --config=\"%s\" --root=\"%s\" --dbpath=\"%s\" --cachedir=\"%s\"" \
                   % (pacman["bin"],
                       os.path.join(self.root, util.PACCONF),
                       self.root,
                       os.path.join(self.root, util.PM_DBPATH),
                       os.path.join(self.root, util.PM_CACHEDIR)))
        if not pacman["manual-confirm"]:
            cmd.append("--noconfirm")
        if pacman["debug"]:
            cmd.append("--debug=%s" % pacman["debug"])
        cmd.append("%s" % self.args)
        if not pacman["gdb"] and not pacman["valgrind"] and not pacman["nolog"]: 
            cmd.append(">\"%s\" 2>&1" % os.path.join(self.root, util.LOGFILE))
        vprint("\trunning: %s" % " ".join(cmd))

        # Change to the tmp dir before running pacman, so that local package
        # archives are made available more easily.
        curdir = os.getcwd()
        tmpdir = os.path.join(self.root, util.TMPDIR)
        os.chdir(tmpdir)

        time_start = time.time()
        self.retcode = os.system(" ".join(cmd))
        time_end = time.time()
        vprint("\ttime elapsed: %ds" % (time_end - time_start))

        if self.retcode == None:
            self.retcode = 0
        else:
            self.retcode /= 256
        vprint("\tretcode = %s" % self.retcode)
        os.chdir(curdir)

        # Check if the lock is still there
        if os.path.isfile(util.PM_LOCK):
            print "\tERROR: %s not removed" % util.PM_LOCK
            os.unlink(util.PM_LOCK)
        # Look for a core file
        if os.path.isfile(os.path.join(self.root, util.TMPDIR, "core")):
            print "\tERROR: pacman dumped a core file"

    def check(self):
        """
        """

        print "==> Checking rules"

        for i in self.rules:
            success = i.check(self)
            if success == 1:
                msg = " OK "
                self.result["success"] += 1
            elif success == 0:
                msg = "FAIL"
                self.result["fail"] += 1
            else:
                msg = "SKIP"
            print "\t[%s] %s" % (msg, i)

# vim: set ts=4 sw=4 et:
