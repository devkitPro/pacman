#!/bin/sh -xu

make distclean
rm -rf autom4te.cache
rm -rf Makefile
rm -rf Makefile.in
rm -rf configure
rm -rf config.*
rm -rf stamp*
rm -rf depcomp
rm -rf install-sh
rm -rf missing
rm -rf src/pacman/Makefile
rm -rf src/pacman/Makefile.in
rm -rf src/util/Makefile
rm -rf src/util/Makefile.in
rm -rf lib/libftp/Makefile
rm -rf lib/libftp/Makefile.in
rm -rf lib/libalpm/Makefile.in
rm -rf lib/libalpm/Makefile
rm -rf aclocal.m4
rm -rf ltmain.sh
rm -rf doc/Makefile
rm -rf doc/Makefile.in
rm -rf doc/html/*
rm -rf doc/*.8
rm -rf compile
rm -rf libtool
rm -rf scripts/.deps/
rm -rf scripts/Makefile.in
