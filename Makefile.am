SUBDIRS = lib/libalpm src/util src/pacman scripts doc etc pactest

# Some files automatically included, so they aren't specified below:
#   AUTHORS, COPYING, NEWS, README
# TODO: move translation-help to ABOUT-NLS?
EXTRA_DIST = HACKING

# TODO : figure out a way to get 'make distcheck' to build with
#        --disable-fakeroot so it actually passes tests
check-local:  src/pacman
	python $(top_srcdir)/pactest/pactest.py --test=$(top_srcdir)/pactest/tests/*.py -p $(top_builddir)/src/pacman/pacman --debug=1
	rm -rf $(top_builddir)/root

