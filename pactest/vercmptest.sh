#!/bin/sh
#
# vercmptest - a test suite for the vercmp/libalpm program
#
#   Copyright (c) 2008 by Dan McGee <dan@archlinux.org>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.

# default binary if one was not specified as $1
bin='vercmp'
# holds count of failed tests
failure=0

# args:
# fail ver1 ver2 ret expected
fail() {
	echo "unexpected failure:"
	echo "  ver1: $1 ver2: $2 ret: $3 expected: $4"
	failure=$(expr $failure + 1)
}

# args:
# runtest ver1 ver2 expected
runtest() {
	ret=$($bin $1 $2)
	[ $ret -eq $3 ] || fail $1 $2 $ret $3
}

# use first arg as our binary if specified
[ -n "$1" ] && bin="$1"

# BEGIN TESTS

# all similar length, no pkgrel
runtest 1.5.0 1.5.0  0
runtest 1.5.0 1.5.1 -1
runtest 1.5.1 1.5.0  1

# mixed length
runtest 1.5   1.5.1 -1
runtest 1.5.1 1.5    1

# with pkgrel, simple
runtest 1.5.0-1 1.5.0-1  0
runtest 1.5.0-1 1.5.0-2 -1
runtest 1.5.0-1 1.5.1-1 -1
runtest 1.5.0-2 1.5.1-1 -1

# with pkgrel, mixed lengths
runtest 1.5-1   1.5.1-1 -1
runtest 1.5-2   1.5.1-1 -1
runtest 1.5-2   1.5.1-2 -1

# mixed pkgrel inclusion
runtest 1.5   1.5-1 0
runtest 1.5-1 1.5   0

#END TESTS

if [ $failure -eq 0 ]; then
	echo "All tests successful"
	exit 0
fi

echo "$failure failed tests"
exit 1
