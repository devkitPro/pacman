#!/bin/sh -exu

#intltoolize -f -c
libtoolize -f -c
aclocal --force
autoheader -f
autoconf -f
automake -a -c --gnu --foreign
cp -f /usr/share/automake-1.9/mkinstalldirs ./
cp -f /usr/share/gettext/config.rpath ./
