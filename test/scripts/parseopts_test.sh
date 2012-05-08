#!/bin/bash

declare -i testcount=0 pass=0 fail=0

# source the library function
if [[ -z $1 || ! -f $1 ]]; then
  printf "error: path to parseopts library not provided or does not exist\n"
  exit 1
fi
. "$1"

if ! type -t parseopts >/dev/null; then
  printf 'parseopts function not found\n'
  exit 1
fi

# borrow opts from makepkg
OPT_SHORT="AcdefFghiLmop:rRsV"
OPT_LONG=('allsource' 'asroot' 'ignorearch' 'check' 'clean:' 'cleanall' 'nodeps'
          'noextract' 'force' 'forcever:' 'geninteg' 'help' 'holdver'
          'install' 'key:' 'log' 'nocolor' 'nobuild' 'nocheck' 'nosign' 'pkg:' 'rmdeps'
          'repackage' 'skipinteg' 'sign' 'source' 'syncdeps' 'version' 'config:'
          'noconfirm' 'noprogressbar')

parse() {
  local result=$1 tokencount=$2; shift 2

  (( ++testcount ))
  parseopts "$OPT_SHORT" "${OPT_LONG[@]}" -- "$@" 2>/dev/null
  test_result "$result" "$tokencount" "$*" "${OPTRET[@]}"
  unset OPTRET
}

test_result() {
  local result=$1 tokencount=$2 input=$3; shift 3

  if [[ $result = "$*" ]] && (( tokencount == $# )); then
    (( ++pass ))
  else
    printf '[TEST %3s]: FAIL\n' "$testcount"
    printf '      input: %s\n' "$input"
    printf '     output: %s (%s tokens)\n' "$*" "$#"
    printf '   expected: %s (%s tokens)\n' "$result" "$tokencount"
    echo
    (( ++fail ))
  fi
}

summarize() {
  if (( !fail )); then
    printf 'All %s tests successful\n\n' "$testcount"
    exit 0
  else
    printf '%s of %s tests failed\n\n' "$fail" "$testcount"
    exit 1
  fi
}
trap 'summarize' EXIT

printf 'Beginning parseopts tests\n'

# usage: parse <expected result> <token count> test-params...
# a failed parse will match only the end of options marker '--'

# no options
parse '--' 1

# short options
parse '-s -r --' 3 -s -r

# short options, no spaces
parse '-s -r --' 3 -sr

# short opt missing an opt arg
parse '--' 1 -s -p

# short opt with an opt arg
parse '-p PKGBUILD -L --' 4 -p PKGBUILD -L

# short opt with an opt arg, no space
parse '-p PKGBUILD --' 3 -pPKGBUILD

# valid shortopts as a long opt
parse '--' 1 --sir

# long opt wiht no optarg
parse '--log --' 2 --log

# long opt with missing optarg
parse '--' 1 -sr --pkg

# long opt with optarg
parse '--pkg foo --' 3 --pkg foo

# long opt with optarg with whitespace
parse '--pkg foo bar -- baz' 4 --pkg "foo bar" baz

# long opt with optarg with =
parse '--pkg foo=bar -- baz' 4 --pkg foo=bar baz

# long opt with explicit optarg
parse '--pkg bar -- foo baz' 5 foo --pkg=bar baz

# long opt with explicit optarg, with whitespace
parse '--pkg foo bar -- baz' 4 baz --pkg="foo bar"

# long opt with explicit optarg that doesn't take optarg
parse '--' 1 --force=always -s

# long opt with explicit optarg with =
parse '--pkg foo=bar --' 3 --pkg=foo=bar

# explicit end of options with options after
parse '-s -r -- foo bar baz' 6 -s -r -- foo bar baz

# non-option parameters mixed in with options
parse '-s -r -- foo baz' 5 -s foo baz -r

# optarg with whitespace
parse '-p foo bar -s --' 4 -p'foo bar' -s

# non-option parameter with whitespace
parse '-i -- foo bar' 3 -i 'foo bar'

# successful stem match (opt has no arg)
parse '--nocolor --' 2 --nocol

# successful stem match (opt has arg)
parse '--config foo --' 3 --conf foo

# ambiguous long opt
parse '--' 1 '--for'

# exact match on a possible stem (--force & --forcever)
parse '--force --' 2 --force

# exact match on possible stem (opt has optarg)
parse '--clean foo --' 3 --clean=foo
