#!/bin/sh -xu

[ -f Makefile ] && make distclean
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
rm -rf doc/hu/Makefile
rm -rf doc/hu/Makefile.in
rm -rf doc/html/*.html
rm -rf doc/man3/*.3
rm -rf compile
rm -rf libtool
rm -rf mkinstalldirs
rm -rf config.rpath
rm -rf scripts/.deps/
rm -rf scripts/Makefile.in
rm -rf etc/Makefile.in
rm -rf etc/Makefile
rm -rf etc/pacman.d/Makefile.in
rm -rf etc/pacman.d/Makefile
rm -rf etc/abs/Makefile.in
rm -rf etc/abs/Makefile
rm -rf pactest/Makefile.in
rm -rf pactest/Makefile

rm -rf src/pacman/po/Makefile
rm -rf src/pacman/po/Makefile.in
rm -rf src/pacman/po/POTFILES
rm -rf src/pacman/po/stamp-po
rm -rf src/pacman/po/*.gmo

rm -rf lib/libalpm/po/Makefile
rm -rf lib/libalpm/po/Makefile.in
rm -rf lib/libalpm/po/POTFILES
rm -rf lib/libalpm/po/stamp-po
rm -rf lib/libalpm/po/*.gmo
