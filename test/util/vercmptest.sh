#!/bin/bash
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
bin=${1:-${PMTEST_UTIL_DIR}vercmp}
# holds counts of tests
total=92
run=0
failure=0

# use first arg as our binary if specified
if ! type -p "$bin" &>/dev/null; then
	echo "Bail out! vercmp binary ($bin) could not be located"
	exit 1
fi

# args:
# pass ver1 ver2 ret expected
pass() {
	echo "ok $run - ver1: $1 ver2: $2 ret: $3"
}

# args:
# fail ver1 ver2 ret expected
fail() {
	echo "not ok $run - test: ver1: $1 ver2: $2 ret: $3 expected: $4"
	((failure++))
}

# args:
# runtest ver1 ver2 expected
runtest() {
	# run the test
	((run++))
	ret=$($bin $1 $2)
	func='pass'
	[[ -n $ret && $ret -eq $3 ]] || func='fail'
	$func $1 $2 $ret $3
	# and run its mirror case just to be sure
	((run++))
	reverse=0
	[[ $3 -eq 1 ]] && reverse=-1
	[[ $3 -eq -1 ]] && reverse=1
	ret=$($bin $2 $1)
	func='pass'
	[[ -n $ret && $ret -eq $reverse ]] || func='fail'
	$func $2 $1 $ret $reverse
}

echo "# Running vercmp tests..."

echo "1..$total"

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

# alphanumeric versions
runtest 1.5b-1  1.5-1  -1
runtest 1.5b    1.5    -1
runtest 1.5b-1  1.5    -1
runtest 1.5b    1.5.1  -1

# from the manpage
runtest 1.0a     1.0alpha -1
runtest 1.0alpha 1.0b     -1
runtest 1.0b     1.0beta  -1
runtest 1.0beta  1.0rc    -1
runtest 1.0rc    1.0      -1

# going crazy? alpha-dotted versions
runtest 1.5.a    1.5     1
runtest 1.5.b    1.5.a   1
runtest 1.5.1    1.5.b   1

# alpha dots and dashes
runtest 1.5.b-1  1.5.b   0
runtest 1.5-1    1.5.b  -1

# same/similar content, differing separators
runtest 2.0    2_0     0
runtest 2.0_a  2_0.a   0
runtest 2.0a   2.0.a  -1
runtest 2___a  2_a     1

# epoch included version comparisons
runtest 0:1.0    0:1.0   0
runtest 0:1.0    0:1.1  -1
runtest 1:1.0    0:1.0   1
runtest 1:1.0    0:1.1   1
runtest 1:1.0    2:1.1  -1

# epoch + sometimes present pkgrel
runtest 1:1.0    0:1.0-1  1
runtest 1:1.0-1  0:1.1-1  1

# epoch included on one version
runtest 0:1.0    1.0   0
runtest 0:1.0    1.1  -1
runtest 0:1.1    1.0   1
runtest 1:1.0    1.0   1
runtest 1:1.0    1.1   1
runtest 1:1.1    1.1   1

#END TESTS

if [[ $failure -eq 0 ]]; then
	echo "# All $run tests successful"
	exit 0
fi

echo "# $failure of $run tests failed"
exit 1
