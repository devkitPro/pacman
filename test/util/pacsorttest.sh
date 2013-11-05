#!/bin/bash
#
# pacsorttest - a test suite for pacsort
#
#   Copyright (c) 2013 by Pacman Development Team <pacman-dev@archlinux.org>
#   Copyright (c) 2011 by Dan McGee <dan@archlinux.org>
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
bin=${1:-${PMTEST_UTIL_DIR}pacsort}
# holds counts of tests
total=23
run=0
failure=0

if ! type -p "$bin" &>/dev/null; then
	echo "Bail out! pacsort binary ($bin) could not be located"
	exit 1
fi

# args:
# runtest input expected test_description optional_opts
runtest() {
	# run the test
	((run++))
	out=$(diff -u <(printf "$1" | $bin $4) <(printf "$2"))
	if [[ $? -eq 0 ]]; then
		echo "ok $run - $3"
	else
		((failure++))
		echo "not ok $run - $3"
		while read line; do
			echo "    # $line"
		done <<<"$out"
	fi
}

echo "# Running pacsort tests..."

echo "1..$total"

# BEGIN TESTS

in="1\n2\n3\n4\n"
runtest $in $in "already ordered"

in="4\n2\n3\n1\n"
ex="1\n2\n3\n4\n"
runtest $in $ex "easy reordering"

in="1\n2\n3\n4"
ex="1\n2\n3\n4\n"
runtest $in $ex "add trailing newline"

in="1\n2\n4\n3"
ex="1\n2\n3\n4\n"
runtest $in $ex "add trailing newline"

in="1.0-1\n1.0\n1.0-2\n1.0\n"
runtest $in $in "stable sort"

in="firefox-18.0-2-x86_64.pkg.tar.xz\nfirefox-18.0.1-1-x86_64.pkg.tar.xz\n"
runtest $in $in "filename sort" "--files"

in="firefox-18.0-2\nfirefox-18.0.1-1-x86_64.pkg.tar.xz\n"
runtest $in $in "filename sort with invalid filename" "--files"

in="firefox-18.0-2-x86_64.pkg.tar.xz\n/path2/firefox-18.0.1-1-x86_64.pkg.tar.xz\n"
runtest $in $in "filename sort maybe with leading paths" "--files"

in="/path1/firefox-18.0-2-x86_64.pkg.tar.xz\n/path2/firefox-18.0.1-1-x86_64.pkg.tar.xz\n"
runtest $in $in "filename sort with different leading paths" "--files"

in="/path2/firefox-18.0-2-x86_64.pkg.tar.xz\n/path1/path2/firefox-18.0.1-1-x86_64.pkg.tar.xz\n"
runtest $in $in "filename sort with uneven leading path components" "--files"

in="firefox-18.0-2-i686.pkg.tar.xz\nfirefox-18.0.1-1-x86_64.pkg.tar.gz\n"
runtest $in $in "filename sort with different extensions" "--files"

# generate some long input/expected for the next few tests
declare normal reverse names_normal names_reverse
for ((i=1; i<600; i++)); do
	normal="${normal}${i}\n"
	reverse="${reverse}$((600 - ${i}))\n"
	fields="${fields}colA bogus$((600 - ${i})) ${i}\n"
	fields_reverse="${fields_reverse}colA bogus${i} $((600 - ${i}))\n"
	separator="${separator}colA|bogus$((600 - ${i}))|${i}\n"
	separator_reverse="${separator_reverse}colA|bogus${i}|$((600 - ${i}))\n"
done

runtest $normal $normal "really long input"
runtest $reverse $normal "really long input"
runtest $reverse $reverse "really long input, reversed" "-r"
runtest $normal $reverse "really long input, reversed" "-r"

runtest "$fields" "$fields" "really long input, sort key" "-k3"
runtest "$fields_reverse" "$fields" "really long input, sort key" "-k3"
runtest "$fields_reverse" "$fields_reverse" "really long input, sort key, reversed" "-k 3 -r"
runtest "$fields" "$fields_reverse" "really long input, sort key, reversed" "-k 3 -r"

runtest "$separator" "$separator" "really long input, sort key, separator" "-k3 -t|"
runtest "$separator_reverse" "$separator" "really long input, sort key, separator" "-k3 -t|"
runtest "$separator_reverse" "$separator_reverse" "really long input, sort key, separator, reversed" "-k 3 -t| -r"
runtest "$separator" "$separator_reverse" "really long input, sort key, separator, reversed" "-k 3 -t| -r"

#END TESTS

if [[ $failure -eq 0 ]]; then
	echo "# All $run tests successful"
	exit 0
fi

echo "# $failure of $run tests failed"
exit 1
