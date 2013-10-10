#!/bin/sh -xu

autoreconf -i
(cd build-aux && (patch -Np0 -i ltmain-asneeded.patch || true))
