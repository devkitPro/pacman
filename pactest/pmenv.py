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
import os.path
import time

import pmtest


class pmenv:
    """Environment object
    """

    def __init__(self, root = "root"):
        self.root = os.path.abspath(root)
        self.pacman = {
            "bin": "pacman",
            "debug": 0,
            "gdb": 0,
            "valgrind": 0,
            "nolog": 0
        }
        self.testcases = []

    def __str__(self):
        return "root = %s\n" \
               "pacman = %s" \
               % (self.root, self.pacman)

    def addtest(self, testcase):
        """
        """
        if not os.path.isfile(testcase):
            err("file %s not found" % testcase)
            return
        test = pmtest.pmtest(testcase, self.root)
        self.testcases.append(test)

    def run(self):
        """
        """

        for t in self.testcases:
            print "=========="*8
            print "Running '%s'" % t.testname

            t.load()
            print t.description
            print "----------"*8

            t.generate()
            # Hack for mtimes consistency
            modified = 0
            for i in t.rules:
                if i.rule.find("MODIFIED") != -1:
                    modified = 1
            if modified:
                time.sleep(3)

            t.run(self.pacman)

            t.check()
            print "==> Test result"
            if t.result["fail"] == 0:
                print "\tPASSED"
            else:
                print "\tFAILED"
            print

    def results(self):
        """
        """
        passed = 0
        tpassed = []
        tfailed = []
        for test in self.testcases:
            fail = test.result["fail"]
            if fail == 0:
                passed += 1
                tpassed.append(test)
            else:
                tfailed.append(test)

        def _printtest(t):
            success = test.result["success"]
            fail = test.result["fail"]
            rules = len(test.rules)
            if fail == 0:
                result = "[PASSED]"
            else:
                result = "[FAILED]"
            print result,
            print "%s Rules: OK = %2u  FAIL = %2u  SKIP = %2u" \
                    % (test.testname.ljust(32), success, fail, \
                       rules - (success + fail))
            if fail != 0:
                # print test description if test failed
                print "   ", test.description

        print "=========="*8
        print "Results"
        print "----------"*8
        for test in tpassed: _printtest(test)
        print "----------"*8
        for test in tfailed: _printtest(test)
        print "----------"*8

        total = len(self.testcases)
        failed = total - passed
        print "TOTAL  = %3u" % total
        if total:
            print "PASSED = %3u (%6.2f%%)" % (passed, float(passed) * 100 / total)
            print "FAILED = %3u (%6.2f%%)" % (failed, float(failed) * 100 / total)
        print ""

if __name__ == "__main__":
    env = pmenv("/tmp")
    print env
# vim: set ts=4 sw=4 et:
