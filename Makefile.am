SUBDIRS = lib/libalpm src/util src/pacman scripts doc etc po pactest contrib

# Some files automatically included, so they aren't specified below:
#   AUTHORS, COPYING, NEWS, README
EXTRA_DIST = HACKING

check-local:  src/pacman
	python $(top_srcdir)/pactest/pactest.py --debug=1 \
		--test $(top_srcdir)/pactest/tests/*.py \
		-p $(top_builddir)/src/pacman/pacman
	rm -rf $(top_builddir)/root

# vim:set ts=2 sw=2 noet:
