#!/bin/sh -xu

aclocal
autoheader
automake --foreign
autoconf
