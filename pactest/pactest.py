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


import getopt
import sys
import glob
import os

import pmenv
import util


__author__ = "Aurelien FORET"
__version__ = "0.3"


def usage(retcode):
    """
    """
    print "Usage: %s [options] [[--test=<path/to/testfile.py>] ...]\n\n" % __file__
    sys.exit(retcode)

if __name__ == "__main__":
    env = pmenv.pmenv()
    testcases = []

    try:
        opts, args = getopt.getopt(sys.argv[1:],
                                   "d:hp:t:v",
                                   ["debug=", "gdb", "help", "pacman=", "test=", "valgrind", "verbose", "nolog"])
    except getopt.GetoptError:
        usage(1)

    for (cmd, param) in opts:
        if cmd == "-v" or cmd == "--verbose":
            util.verbose += 1
        elif cmd == "-d" or cmd == "--debug":
            env.pacman["debug"] = int(param)
        elif cmd == "-t" or cmd == "--test":
            testcases.extend(glob.glob(param))
        elif cmd == "-p" or cmd == "--pacman":
            env.pacman["bin"] = os.path.abspath(param)
        elif cmd == "-h" or cmd == "--help":
            usage(0)
        elif cmd == "--nolog":
            env.pacman["nolog"] = 1
        elif cmd == "--gdb":
            env.pacman["gdb"] = 1
        elif cmd == "--valgrind":
            env.pacman["valgrind"] = 1

    for i in testcases:
        env.addtest(i)

    env.run()
    env.results()
# vim: set ts=4 sw=4 et:
