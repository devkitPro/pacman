#!/bin/bash
#
# pacsorttest - a test suite for pacsort
#
#   Copyright (c) 2013-2016 by Pacman Development Team <pacman-dev@archlinux.org>
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

source "$(dirname "$0")"/../tap.sh || exit 1

# default binary if one was not specified as $1
bin=${1:-${PMTEST_UTIL_DIR}pacsort}

if ! type -p "$bin" &>/dev/null; then
	tap_bail "pacsort binary ($bin) could not be located"
	exit 1
fi

# args:
# runtest input expected test_description optional_opts
tap_runtest() {
	# run the test
	tap_diff <(printf "$1" | $bin $4) <(printf "$2") "$3"
}

# args:
# check_return_value input expected_return_value test_description optional_opts
tap_check_return_value() {
    # run the test
    printf "$1" | $bin $4 2>/dev/null
    tap_is_int "$?" "$2" "$3"

}

tap_plan 32

in="1\n2\n3\n4\n"
tap_runtest $in $in "already ordered"

in="4\n2\n3\n1\n"
ex="1\n2\n3\n4\n"
tap_runtest $in $ex "easy reordering"

in="1\n2\n3\n4"
ex="1\n2\n3\n4\n"
tap_runtest $in $ex "add trailing newline"

in="1\n2\n4\n3"
ex="1\n2\n3\n4\n"
tap_runtest $in $ex "add trailing newline"

in="1.0-1\n1.0\n1.0-2\n1.0\n"
tap_runtest $in $in "stable sort"

in="firefox-18.0-2-x86_64.pkg.tar.xz\nfirefox-18.0.1-1-x86_64.pkg.tar.xz\n"
tap_runtest $in $in "filename sort" "--files"

in="firefox-18.0-2\nfirefox-18.0.1-1-x86_64.pkg.tar.xz\n"
tap_runtest $in $in "filename sort with invalid filename" "--files"

in="firefox-18.0-2-x86_64.pkg.tar.xz\n/path2/firefox-18.0.1-1-x86_64.pkg.tar.xz\n"
tap_runtest $in $in "filename sort maybe with leading paths" "--files"

in="/path1/firefox-18.0-2-x86_64.pkg.tar.xz\n/path2/firefox-18.0.1-1-x86_64.pkg.tar.xz\n"
tap_runtest $in $in "filename sort with different leading paths" "--files"

in="/path2/firefox-18.0-2-x86_64.pkg.tar.xz\n/path1/path2/firefox-18.0.1-1-x86_64.pkg.tar.xz\n"
tap_runtest $in $in "filename sort with uneven leading path components" "--files"

in="firefox-18.0-2-i686.pkg.tar.xz\nfirefox-18.0.1-1-x86_64.pkg.tar.gz\n"
tap_runtest $in $in "filename sort with different extensions" "--files"

in="/packages/dialog-1.2_20131001-1-x86_64.pkg.tar.xz\n/packages/dialog-1:1.2_20130928-1-x86_64.pkg.tar.xz\n"
tap_runtest $in $in "filename sort with epoch" "--files"

in="/packages/dia-log-1:1.2_20130928-1-x86_64.pkg.tar.xz\n/packages/dialog-1.2_20131001-1-x86_64.pkg.tar.xz\n"
tap_runtest $in $in "filename sort with differing package names and epoch" "--files"

in="/packages/systemd-217-1-x86_64.pkg.tar.xz\n/packages/systemd-sysvcompat-217-1-x86_64.pkg.tar.xz\n"
tap_runtest $in $in "filename sort with package names as shared substring" "--files"

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

tap_runtest $normal $normal "really long input"
tap_runtest $reverse $normal "really long input"
tap_runtest $reverse $reverse "really long input, reversed" "-r"
tap_runtest $normal $reverse "really long input, reversed" "-r"

tap_runtest "$fields" "$fields" "really long input, sort key" "-k3"
tap_runtest "$fields_reverse" "$fields" "really long input, sort key" "-k3"
tap_runtest "$fields_reverse" "$fields_reverse" "really long input, sort key, reversed" "-k 3 -r"
tap_runtest "$fields" "$fields_reverse" "really long input, sort key, reversed" "-k 3 -r"

tap_runtest "$separator" "$separator" "really long input, sort key, separator" "-k3 -t|"
tap_runtest "$separator_reverse" "$separator" "really long input, sort key, separator" "-k3 -t|"
tap_runtest "$separator_reverse" "$separator_reverse" "really long input, sort key, separator, reversed" "-k 3 -t| -r"
tap_runtest "$separator" "$separator_reverse" "really long input, sort key, separator, reversed" "-k 3 -t| -r"

tap_check_return_value "" "2" "invalid sort key (no argument)" "-k"
tap_check_return_value "" "2" "invalid sort key (non-numeric)" "-k asd"
tap_check_return_value "" "2" "invalid field separator (no argument)" "-t"
tap_check_return_value "" "2" "invalid field separator (multiple characters)" "-t sda"
tap_check_return_value "" "2" "invalid field separator (two characters must start with a slash)" "-t ag"
tap_check_return_value "" "2" "invalid field separator (\g is invalid)" '-t \g'

tap_finish

# vim: set noet:
