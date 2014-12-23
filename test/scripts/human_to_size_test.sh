#!/bin/bash

source "$(dirname "$0")"/../tap.sh || exit 1

# source the library function
lib=${1:-${PMTEST_SCRIPTLIB_DIR}human_to_size.sh}
if [[ -z $lib || ! -f $lib ]]; then
	tap_bail "human_to_size library (%s) could not be located" "${lib}"
	exit 1
fi
. "$lib"

if ! type -t human_to_size &>/dev/null; then
	tap_bail "human_to_size function not found"
	exit 1
fi

parse_hts() {
	local input=$1 expected=$2
	tap_is_str "$(human_to_size "$input")" "$expected" "$input"
}

tap_plan 15

# parse_hts <input> <expected output>

parse_hts '1MiB' 1048576

parse_hts '10XiB' ''

parse_hts '10 MiB' 10485760

parse_hts '10 XiB' ''

parse_hts '.1 TiB' 109951162778

parse_hts '  -3    KiB   ' -3072

parse_hts 'foo3KiB' ''

parse_hts '3KiBfoo' ''

parse_hts '3kib' ''

parse_hts '+1KiB' 1024

parse_hts '+1.0 KiB' 1024

parse_hts '1MB' 1000000

parse_hts '1M' 1048576

parse_hts ' 1 G ' 1073741824

parse_hts '1Q' ''

# vim: set noet:
