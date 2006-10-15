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

from util import *


class pmfile:
	"""File object
	"""

	def __init__(self, root, name):
		self.name = name
		self.root = root

		filename = os.path.join(self.root, self.name)
		self.checksum = getmd5sum(filename)
		self.mtime = getmtime(filename)

	def __str__(self):
		return "%s (%s / %lu)" % (self.name, self.checksum, self.mtime)

	def ismodified(self):
		"""
		"""

		retval = 0

		filename = os.path.join(self.root, self.name)
		checksum = getmd5sum(filename)
		mtime = getmtime(filename)

		if debug:
			print "ismodified(%s)" % self.name
			print "old: %s / %s" % (self.checksum, self.mtime)
			print "new: %s / %s" % (checksum, mtime)

		if not self.checksum == checksum \
		   or not (self.mtime[1], self.mtime[2]) == (mtime[1], mtime[2]):
			retval = 1

		return retval


if __name__ == "__main__":
	f = pmfile("/tmp", "foobar")
	print f
