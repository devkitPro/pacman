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

import util

class pmfile(object):
    """File object
    """

    def __init__(self, root, name):
        self.name = name
        self.root = root
        self.filename = os.path.join(self.root, self.name)

        self.checksum = util.getmd5sum(self.filename)
        self.mtime = util.getmtime(self.filename)

    def __str__(self):
        return "%s (%s / %lu)" % (self.name, self.checksum, self.mtime)

    def ismodified(self):
        """
        """
        checksum = util.getmd5sum(self.filename)
        mtime = util.getmtime(self.filename)

        util.vprint("\tismodified(%s)" % self.name)
        util.vprint("\t\told: %s / %s" % (self.checksum, self.mtime))
        util.vprint("\t\tnew: %s / %s" % (checksum, mtime))

        if self.checksum != checksum \
           or (self.mtime[1], self.mtime[2]) != (mtime[1], mtime[2]):
            return 1

        return 0

# vim: set ts=4 sw=4 et:
