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
import time

import pmtest


class pmenv:
    """Environment object
    """

    testcases = []
    passed = 0
    failed = 0
    expectedfail = 0
    unexpectedpass = 0

    def __init__(self, root = "root"):
        self.root = os.path.abspath(root)
        self.pacman = {
            "bin": "pacman",
            "debug": 0,
            "gdb": 0,
            "valgrind": 0,
            "nolog": 0
        }

    def __str__(self):
        return "root = %s\n" \
               "pacman = %s" \
               % (self.root, self.pacman)

    def addtest(self, testcase):
        """
        """
        if not os.path.isfile(testcase):
            raise IOError("test file %s not found" % testcase)
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
            for i in t.rules:
                if i.rule.find("MODIFIED") != -1:
                    time.sleep(1.5)
                    break

            t.run(self.pacman)

            t.check()
            print "==> Test result"
            if t.result["fail"] == 0:
                print "\tPASS"
            else:
                print "\tFAIL"
            print

    def results(self):
        """
        """
        tpassed = []
        tfailed = []
        texpectedfail = []
        tunexpectedpass = []
        for test in self.testcases:
            fail = test.result["fail"]
            if fail == 0 and not test.expectfailure:
                self.passed += 1
                tpassed.append(test)
            elif fail != 0 and test.expectfailure:
                self.expectedfail += 1
                texpectedfail.append(test)
            elif fail == 0: # and not test.expectfail
                self.unexpectedpass += 1
                tunexpectedpass.append(test)
            else:
                self.failed += 1
                tfailed.append(test)

        def _printtest(t):
            success = test.result["success"]
            fail = test.result["fail"]
            rules = len(test.rules)
            if fail == 0:
                result = "[PASS]"
            else:
                result = "[FAIL]"
            print result,
            print "%s Rules: OK = %2u  FAIL = %2u  SKIP = %2u" \
                    % (test.testname.ljust(34), success, fail, \
                       rules - (success + fail))
            if fail != 0:
                # print test description if test failed
                print "      ", test.description

        print "=========="*8
        print "Results"
        print "----------"*8
        print " Passed:"
        for test in tpassed: _printtest(test)
        print "----------"*8
        print " Expected Failures:"
        for test in texpectedfail: _printtest(test)
        print "----------"*8
        print " Unexpected Passes:"
        for test in tunexpectedpass: _printtest(test)
        print "----------"*8
        print " Failed:"
        for test in tfailed: _printtest(test)
        print "----------"*8

        total = len(self.testcases)
        print "Total            = %3u" % total
        if total:
            print "Pass             = %3u (%6.2f%%)" % (self.passed, float(self.passed) * 100 / total)
            print "Expected Fail    = %3u (%6.2f%%)" % (self.expectedfail, float(self.expectedfail) * 100 / total)
            print "Unexpected Pass  = %3u (%6.2f%%)" % (self.unexpectedpass, float(self.unexpectedpass) * 100 / total)
            print "Fail             = %3u (%6.2f%%)" % (self.failed, float(self.failed) * 100 / total)
        print ""

if __name__ == "__main__":
    pass

# vim: set ts=4 sw=4 et:
