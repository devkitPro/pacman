#!/bin/bash
o=$(basename $1)
u=$2
CARCH="i686" # Hmmm where to get this from? /etc/makepkg.conf?
cached_file=""
# Only check for pkg.tar.gz files in the cache, we download db.tar.gz as well
if [[ "$o" =~ "pkg.tar.gz" ]] # if $o contains pkg.tar.gz
then
  pkgname=${o%-*-[0-9]-${CARCH}.pkg.tar.gz.part}   # Parse out the package name
  newend=${o##$pkgname-}                  # Parse out everything following pkgname
  new_version=${newend%-${CARCH}.pkg.tar.gz.part}  # Strip off .pkg.tar.gz.part leaving version
  url=${u%/*}
  for cached_file in $(ls -r /var/cache/pacman/pkg/${pkgname}-*-${CARCH}.pkg.tar.gz 2>/dev/null); do
    # just take the first one, by name. I suppose we could take the latest by date...
    oldend=${cached_file##*/$pkgname-}
    old_version=${oldend%-${CARCH}.pkg.tar.gz}
    if [ "$old_version" = "$new_version" ]; then
      # We already have the new version in the cache! Just continue the download.
      cached_file=""
    fi
    break
  done
fi
if [ "$cached_file" != "" ]; then
  # Great, we have a cached file, now calculate a patch name from it
  delta_name=$pkgname-${old_version}_to_${new_version}-${CARCH}.delta
  # try to download the delta
  if wget --passive-ftp -c $url/$delta_name; then
    # Now apply the delta to the cached file to produce the new file
    echo Applying delta...
    if xdelta patch $delta_name $cached_file $o; then
      # Remove the delta now that we are finished with it
      rm $delta_name
    else
      # Hmmm. xdelta failed for some reason
      rm $delta_name
      # just download the file
      wget --passive-ftp -c -O $o $u
    fi
  else
    # just download the file
    wget --passive-ftp -c -O $o $u
  fi 
else
  # just download the file
  wget --passive-ftp -c -O $o $u
fi
