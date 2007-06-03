#!/bin/bash

if [ -r "/etc/makepkg.conf" ]; then
	source /etc/makepkg.conf
else
	echo "wget-xdelta: Unable to find makepkg.conf"
	exit 1
fi

if [ -r ~/.makepkg.conf ]; then
	source ~/.makepkg.conf
fi

out_file=$(basename $1)
file_url=$2

if ! [[ "$out_file" =~ "pkg.tar.gz" ]]; then
	# If it's not a package file download as normal and exit.
	#wget --passive-ftp -c -O "$out_file" "$file_url"
	exit $?
fi


# Get the package name and version
[[ "$out_file" =~ "$CARCH" ]] && arch="-$CARCH" || arch=""
pkg_data=$(echo $out_file | \
	sed "s|^\(.*\)-\([[:alnum:]_\.]*-[[:alnum:]_\.]*\)${arch}${PKGEXT}.part|\1 \2|")
pkgname=$(echo $pkg_data | cut -d ' ' -f 1)
new_version=$(echo $pkg_data | cut -d ' ' -f 2)
base_url=${file_url%/*}

# Look for the last version
for file in $(ls -r /var/cache/pacman/pkg/${pkgname}-*-*{,-$CARCH}$PKGEXT 2>/dev/null); do
	[[ "$file" =~ "$CARCH" ]] && arch="-$CARCH" || arch=""
	check_version=$(echo $file | \
		sed "s|^.*/${pkgname}-\([[:alnum:]_\.]*-[[:alnum:]_\.]*\)${arch}$PKGEXT$|\1|" | \
		grep -v "^/var/cache/pacman/pkg")

	[ "$check_version" = "" ] && continue

	vercmp=$(vercmp "$check_version" "$old_version")
	if [ "$check_version" != "$new_version" -a $vercmp -gt 0 ]; then
		old_version=$check_version
		old_file=$file
	fi
done

if [ "$old_version" != "" -a "$old_version" != "$new_version" ]; then
	# Great, we have a cached file, now calculate a patch name from it
	delta_name="$pkgname-${old_version}_to_${new_version}-${CARCH}.delta"

	echo "wget-xdelta: Attempting to download delta $delta_name..." >&2
	if wget --passive-ftp -c "$base_url/$delta_name"; then
		echo "wget-xdelta: Applying delta..."
		if xdelta patch "$delta_name" "$old_file" "$out_file"; then
			echo "wget-xdelta: Delta applied successfully!"
			rm "$delta_name"
			exit 0
		else
			echo "wget-xdelta: Failed to apply delta!"
			rm $delta_name
		fi
	fi
 fi

echo "wget-xdelta: Downloading new package..."
wget --passive-ftp -c -O "$out_file" "$file_url"
exit $?

# vim:set ts=4 sw=4 noet:
