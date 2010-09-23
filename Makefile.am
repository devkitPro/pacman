SUBDIRS = lib/libalpm src/util src/pacman scripts etc po test/pacman test/util contrib
if WANT_DOC
SUBDIRS += doc
endif

ACLOCAL_AMFLAGS = -I m4 --install

# Make sure we test and build manpages when doing distcheck
DISTCHECK_CONFIGURE_FLAGS = --enable-doc --disable-git-version

# Some files automatically included, so they aren't specified below:
#   AUTHORS, COPYING, NEWS, README
EXTRA_DIST = HACKING

# Sample makepkg prototype files
pkgdatadir = ${datadir}/${PACKAGE}
dist_pkgdata_DATA = PKGBUILD.proto PKGBUILD-split.proto proto.install ChangeLog.proto

# run the pactest test suite and vercmp tests
check-local: test/pacman test/util src/pacman src/util
	$(PYTHON) $(top_srcdir)/test/pacman/pactest.py --debug=1 \
		--test $(top_srcdir)/test/pacman/tests/*.py \
		-p $(top_builddir)/src/pacman/pacman
	rm -rf $(top_builddir)/root
	$(SH) $(top_srcdir)/test/util/vercmptest.sh \
		$(top_builddir)/src/util/vercmp

# create the pacman DB and cache directories upon install
install-data-local:
	for dir in "$(DESTDIR)$(localstatedir)/lib/pacman" "$(DESTDIR)$(localstatedir)/cache/pacman/pkg"; do \
	  test -z "$$dir" || $(MKDIR_P) "$$dir"; \
	done

# vim:set ts=2 sw=2 noet:
