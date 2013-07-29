#! /usr/bin/python2
#
#  Copyright (c) 2006 by Aurelien Foret <orelien@chez.com>
#  Copyright (c) 2006-2013 Pacman Developmet Team <pacman-dev@archlinux.org>
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

import pmtest
import tap


class pmenv(object):
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
        tap.plan(len(self.testcases))
        for t in self.testcases:
            tap.diag("==========" * 8)
            tap.diag("Running '%s'" % t.testname)

            t.load()
            t.generate(self.pacman)
            t.run(self.pacman)

            tap.diag("==> Checking rules")
            tap.todo = t.expectfailure
            tap.subtest(lambda: t.check(), t.description)

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
            success = t.result["success"]
            fail = t.result["fail"]
            rules = len(t.rules)
            if fail == 0:
                result = "[PASS]"
            else:
                result = "[FAIL]"
            tap.diag("%s %s Rules: OK = %2u  FAIL = %2u" \
                    % (result, t.testname.ljust(34), success, fail))
            if fail != 0:
                # print test description if test failed
                tap.diag("       " + t.description)

        tap.diag("==========" * 8)
        tap.diag("Results")
        tap.diag("----------" * 8)
        tap.diag(" Passed:")
        for test in tpassed:
            _printtest(test)
        tap.diag("----------" * 8)
        tap.diag(" Expected Failures:")
        for test in texpectedfail:
            _printtest(test)
        tap.diag("----------" * 8)
        tap.diag(" Unexpected Passes:")
        for test in tunexpectedpass:
            _printtest(test)
        tap.diag("----------" * 8)
        tap.diag(" Failed:")
        for test in tfailed:
            _printtest(test)
        tap.diag("----------" * 8)

        total = len(self.testcases)
        tap.diag("Total            = %3u" % total)
        if total:
            tap.diag("Pass             = %3u (%6.2f%%)" % (self.passed,
                float(self.passed) * 100 / total))
            tap.diag("Expected Fail    = %3u (%6.2f%%)" % (self.expectedfail,
                float(self.expectedfail) * 100 / total))
            tap.diag("Unexpected Pass  = %3u (%6.2f%%)" % (self.unexpectedpass,
                float(self.unexpectedpass) * 100 / total))
            tap.diag("Fail             = %3u (%6.2f%%)" % (self.failed,
                float(self.failed) * 100 / total))

# vim: set ts=4 sw=4 et:
