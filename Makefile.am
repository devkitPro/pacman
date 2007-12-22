SUBDIRS = lib/libalpm src/util src/pacman scripts etc po pactest contrib
if WANT_DOC
SUBDIRS += doc
endif

# Some files automatically included, so they aren't specified below:
#   AUTHORS, COPYING, NEWS, README
EXTRA_DIST = HACKING

# run the pactest test suite
check-local:  src/pacman
	$(PYTHON) $(top_srcdir)/pactest/pactest.py --debug=1 \
		--test $(top_srcdir)/pactest/tests/*.py \
		-p $(top_builddir)/src/pacman/pacman
	rm -rf $(top_builddir)/root

# create the pacman DB and cache directories upon install
install-data-local:
	for dir in "$(DESTDIR)$(localstatedir)/lib/pacman" "$(DESTDIR)$(localstatedir)/cache/pacman/pkg"; do \
	  test -z "$$dir" || $(MKDIR_P) "$$dir"; \
	done

# vim:set ts=2 sw=2 noet:
