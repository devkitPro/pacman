#!/bin/sh -xu

[ -f Makefile ] && make distclean
rm -rf autom4te.cache
rm -rf {Makefile.in,Makefile}
rm -rf {config.h.in,config.h}
rm -rf config.status
rm -rf configure
rm -rf stamp*
rm -rf aclocal.m4
rm -rf compile
rm -rf libtool

rm -rf lib/libalpm/{Makefile.in,Makefile}
rm -rf src/util/{Makefile.in,Makefile}
rm -rf src/pacman/{Makefile.in,Makefile}
rm -rf scripts/{Makefile.in,Makefile}
rm -rf etc/{Makefile.in,Makefile}
rm -rf etc/pacman.d/{Makefile.in,Makefile}
rm -rf etc/abs/{Makefile.in,Makefile}
rm -rf pactest/{Makefile.in,Makefile}
rm -rf doc/{Makefile.in,Makefile}
rm -rf doc/html/*.html
rm -rf doc/man3/*.3

rm -rf po/{Makefile.in,Makefile}
rm -rf po/POTFILES
rm -rf po/stamp-po
rm -rf po/*.gmo

rm -rf lib/libalpm/po/{Makefile.in,Makefile}
rm -rf lib/libalpm/po/POTFILES
rm -rf lib/libalpm/po/stamp-po
rm -rf lib/libalpm/po/*.gmo
