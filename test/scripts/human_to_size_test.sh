#!/bin/bash

declare -i testcount=0 fail=0 pass=0 total=15

# source the library function
lib=${1:-${PMTEST_SCRIPTLIB_DIR}human_to_size.sh}
if [[ -z $lib || ! -f $lib ]]; then
	printf "Bail out! human_to_size library (%s) could not be located\n" "${lib}"
	exit 1
fi
. "$lib"

if ! type -t human_to_size &>/dev/null; then
	printf "Bail out! human_to_size function not found\n"
	exit 1
fi

parse_hts() {
	local input=$1 expected=$2 result

	(( ++testcount ))

	result=$(human_to_size "$1")
	if [[ $result = "$expected" ]]; then
		(( ++pass ))
		printf "ok %d - %s\n" "$testcount" "$input"
	else
		(( ++fail ))
		printf "not ok %d - %s\n" "$testcount" "$input"
		printf '# [TEST %3s]: FAIL\n' "$testcount"
		printf '#      input: %s\n' "$input"
		printf '#     output: %s\n' "$result"
		printf '#   expected: %s\n' "$expected"
	fi
}

summarize() {
	if (( !fail )); then
		printf '# All %s tests successful\n\n' "$testcount"
		exit 0
	else
		printf '# %s of %s tests failed\n\n' "$fail" "$testcount"
		exit 1
	fi
}
trap 'summarize' EXIT

printf '# Beginning human_to_size tests\n'

echo "1..$total"

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
