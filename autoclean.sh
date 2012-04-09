#!/bin/sh -xu

[ -f Makefile ] && make distclean

rm -rf autom4te.cache
rm -f config.h.in config.h
rm -f config.status
rm -f configure
rm -f stamp*
rm -f aclocal.m4
rm -f compile
rm -f libtool

rm -f test/pacman/*.pyc
rm -f doc/html/*.html
rm -f doc/man3/*.3

find . \( -name 'Makefile' -o \
          -name 'Makefile.in' -o \
          -path '*/po/POTFILES' -o \
          -path '*/po/stamp-po' -o \
          -path '*/po/*.gmo' \) -exec rm -f {} +
