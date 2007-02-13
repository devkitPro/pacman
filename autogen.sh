#!/bin/sh -e

if [ "$1" == "--gettext-only" ]; then
	sh autoclean.sh
	for i in lib/libalpm/po src/pacman/po
	do
		cd $i
		mv Makevars Makevars.tmp
		package=`pwd|sed 's|.*/\(.*\)/.*|\1|'`
		intltool-update --pot --gettext-package=$package
		for j in *.po
		do
			if msgmerge $j $package.pot -o $j.new; then
				mv -f $j.new $j
				echo -n "$i/$j: "
				msgfmt -c --statistics -o $j.gmo $j
				rm -f $j.gmo
			else
				echo "msgmerge for $j failed!"
				rm -f $j.new
			fi
		done
		mv Makevars.tmp Makevars
		cd - >/dev/null
	done
	cd doc
	po4a -k 0 po4a.cfg
	cd po
	for i in *po
	do
		if msgmerge $i $package.pot -o $i.new; then
			mv -f $i.new $i
			echo -n "man/$i: "
			msgfmt -c --statistics -o $i.gmo $i
			rm -f $i.gmo
		else
			echo "msgmerge for $i failed!"
			rm -f $i.new
		fi
	done
	exit 0
fi

cp -f $(dirname $(which automake))/../share/automake-*/mkinstalldirs ./
cp -f $(dirname $(which automake))/../share/gettext/config.rpath ./

libtoolize -f -c
aclocal --force
autoheader -f
autoconf -f
automake -a -c --gnu --foreign
