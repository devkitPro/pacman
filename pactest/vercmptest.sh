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
# holds counts of tests
total=0
failure=0

# args:
# pass ver1 ver2 ret expected
pass() {
	echo "test: ver1: $1 ver2: $2 ret: $3 expected: $4"
	echo "  --> pass"
}

# args:
# fail ver1 ver2 ret expected
fail() {
	echo "test: ver1: $1 ver2: $2 ret: $3 expected: $4"
	echo "  ==> FAILURE"
	failure=$(expr $failure + 1)
}

# args:
# runtest ver1 ver2 expected
runtest() {
	# run the test
	ret=$($bin $1 $2)
	func='pass'
	[ $ret -eq $3 ] || func='fail'
	$func $1 $2 $ret $3
	total=$(expr $total + 1)
	# and run its mirror case just to be sure
	reverse=0
	[ $3 -eq 1 ] && reverse=-1
	[ $3 -eq -1 ] && reverse=1
	ret=$($bin $2 $1)
	func='pass'
	[ $ret -eq $reverse ] || func='fail'
	$func $1 $2 $ret $reverse
	total=$(expr $total + 1)
}

# use first arg as our binary if specified
[ -n "$1" ] && bin="$1"

# BEGIN TESTS

# all similar length, no pkgrel
runtest 1.5.0 1.5.0  0
runtest 1.5.1 1.5.0  1

# mixed length
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
runtest 1.1-1 1.1   0
runtest 1.0-1 1.1  -1
runtest 1.1-1 1.0   1

#END TESTS

echo
if [ $failure -eq 0 ]; then
	echo "All $total tests successful"
	exit 0
fi

echo "$failure of $total tests failed"
exit 1
