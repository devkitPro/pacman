#!/bin/bash
# gen-mirrorlist.sh
# There's absolutely no way to make autoconf do this, so instead, autoconf will
# call this script - simple enough.


REPOS="current extra unstable release community"

for i in $REPOS; do
    rm -f $i
    cat mirrorlist | sed "s|@@REPO@@|$i|g" > $i
done
