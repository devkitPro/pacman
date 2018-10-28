#!/bin/sh

built_script=$1
dest_path=$2

install -Dm755 "$built_script" "$DESTDIR/$dest_path"
