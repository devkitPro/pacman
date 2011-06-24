#!/bin/sh -xu

[ -f Makefile ] && make distclean
rm -rf autom4te.cache
rm -f {Makefile.in,Makefile}
rm -f {config.h.in,config.h}
rm -f config.status
rm -f configure
rm -f stamp*
rm -f aclocal.m4
rm -f compile
rm -f libtool

rm -f lib/libalpm/{Makefile.in,Makefile}
rm -f src/util/{Makefile.in,Makefile}
rm -f src/pacman/{Makefile.in,Makefile}
rm -f scripts/{Makefile.in,Makefile}
rm -f etc/{Makefile.in,Makefile}
rm -f etc/pacman.d/{Makefile.in,Makefile}
rm -f etc/abs/{Makefile.in,Makefile}
rm -f test/{pacman,util}{,/tests}/{Makefile.in,Makefile}
rm -f contrib/{Makefile.in,Makefile}
rm -f doc/{Makefile.in,Makefile}

rm -f test/pacman/*.pyc
rm -f doc/html/*.html
rm -f doc/man3/*.3

rm -f {lib/libalpm,scripts,src/pacman}/po/{Makefile.in,Makefile}
rm -f {lib/libalpm,scripts,src/pacman}/po/POTFILES
rm -f {lib/libalpm,scripts,src/pacman}/po/stamp-po
rm -f {lib/libalpm,scripts,src/pacman}/po/*.gmo
