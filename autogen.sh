#!/bin/sh -x

autoreconf -i
patch -d build-aux -Np0 -i ltmain-asneeded.patch

exit 0
