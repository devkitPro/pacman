#! /usr/bin/python
#
#  pactest : run automated testing on the pacman binary
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

import os, sys, glob
from optparse import OptionParser

import pmenv
import util

__author__ = "Aurelien FORET"
__version__ = "0.4"

def resolveBinPath(option, opt_str, value, parser):
    setattr(parser.values, option.dest, os.path.abspath(value))

def globTests(option, opt_str, value, parser):
    idx=0
    globlist = []

    # maintain the idx so we can modify rargs
    while idx < len(parser.rargs) and \
            not parser.rargs[idx].startswith('-'):
        globlist += glob.glob(parser.rargs[idx])
        idx += 1

    parser.rargs = parser.rargs[idx:]
    setattr(parser.values, option.dest, globlist)

def createOptParser():
    testcases = []
    usage = "usage: %prog [options] [[--test <path/to/testfile.py>] ...]"
    description = "Runs automated tests on the pacman binary. Tests are " \
            "described using an easy python syntax, and several can be " \
            "ran at once."
    parser = OptionParser(usage = usage, description = description)

    parser.add_option("-v", "--verbose", action = "count",
                      dest = "verbose", default = 0,
                      help = "print verbose output")
    parser.add_option("-d", "--debug", type = "int",
                      dest = "debug", default = 0,
                      help = "set debug level for pacman")
    parser.add_option("-p", "--pacman", action = "callback",
                      callback = resolveBinPath, type = "string",
                      dest = "bin", default = "pacman",
                      help = "specify location of the pacman binary")
    parser.add_option("-t", "--test", action = "callback",
                      callback = globTests, dest = "testcases",
                      help = "specify test case(s)")
    parser.add_option("--nolog", action = "store_true",
                      dest = "nolog", default = False,
                      help = "do not log pacman messages")
    parser.add_option("--gdb", action = "store_true",
                      dest = "gdb", default = False,
                      help = "use gdb while calling pacman")
    parser.add_option("--valgrind", action = "store_true",
                      dest = "valgrind", default = False,
                      help = "use valgrind while calling pacman")
    parser.add_option("--manual-confirm", action = "store_true",
                      dest = "manualconfirm", default = False,
                      help = "do not use --noconfirm for pacman calls")
    return parser

 
if __name__ == "__main__":
    # instantiate env and parser objects 
    env = pmenv.pmenv()
    parser = createOptParser()
    (opts, args) = parser.parse_args()

    # add parsed options to env object
    util.verbose = opts.verbose
    env.pacman["debug"] = opts.debug
    env.pacman["bin"] = opts.bin
    env.pacman["nolog"] = opts.nolog
    env.pacman["gdb"] = opts.gdb
    env.pacman["valgrind"] = opts.valgrind
    env.pacman["manual-confirm"] = opts.manualconfirm

    if opts.testcases is None or len(opts.testcases) == 0:
        print "no tests defined, nothing to do"
        sys.exit(2)
    else:
        for i in opts.testcases:
            env.addtest(i)

        # run tests and print overall results
        env.run()
        env.results()

        if env.failed > 0:
            sys.exit(1)

# vim: set ts=4 sw=4 et:
