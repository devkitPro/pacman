SUBDIRS = lib/libalpm src/util src/pacman scripts doc etc 

EXTRA_DIST =		\
	NEWS	  	\
	COPYING		\
	README

check: src/pacman
	cd pactest; python pactest.py --test=tests/*.py -p ../src/pacman/pacman --debug=-1

tags:
	ctags -a lib/libalpm/*.[ch]
	ctags -a src/pacman/*.[ch]
