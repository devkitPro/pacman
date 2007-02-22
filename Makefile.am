SUBDIRS = lib/libalpm src/util src/pacman scripts doc etc 

# Some files automatically included, so they aren't specified below:
#   AUTHORS, COPYING, NEWS, README
# TODO: move translation-help to ABOUT-NLS?
EXTRA_DIST = HACKING

check: src/pacman
	cd pactest; python pactest.py --test=tests/*.py -p ../src/pacman/pacman --debug=1
