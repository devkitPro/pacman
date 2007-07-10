SUBDIRS = lib/libalpm src/util src/pacman scripts doc etc po pactest contrib

# Some files automatically included, so they aren't specified below:
#   AUTHORS, COPYING, NEWS, README
EXTRA_DIST = HACKING

# run the pactest test suite
check-local:  src/pacman
	python $(top_srcdir)/pactest/pactest.py --debug=1 \
		--test $(top_srcdir)/pactest/tests/*.py \
		-p $(top_builddir)/src/pacman/pacman
	rm -rf $(top_builddir)/root

# create the pacman DB and cache directories upon install
install-data-local:
	for dir in "$(DESTDIR)$(localstatedir)/lib/pacman" "$(DESTDIR)$(localstatedir)/cache/pacman/pkg"; do \
	  test -z "$$dir" || $(MKDIR_P) "$$dir"; \
	done

# vim:set ts=2 sw=2 noet:
