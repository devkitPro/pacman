SUBDIRS = lib/libalpm src/util src/pacman scripts etc test/pacman test/util test/scripts
if WANT_DOC
SUBDIRS += doc
endif

DIST_SUBDIRS = $(SUBDIRS) contrib src/common

ACLOCAL_AMFLAGS = -I m4 --install
AM_MAKEFLAGS = --no-print-directory

# Make sure we test and build manpages when doing distcheck
DISTCHECK_CONFIGURE_FLAGS = --enable-doc --disable-git-version

# Some files automatically included, so they aren't specified below:
#   AUTHORS, COPYING, NEWS, README
EXTRA_DIST = HACKING test/tap.sh

# Sample makepkg prototype files
pkgdatadir = ${datadir}/${PACKAGE}
dist_pkgdata_DATA = \
	proto/PKGBUILD.proto \
	proto/PKGBUILD-split.proto \
	proto/PKGBUILD-vcs.proto \
	proto/proto.install

$(top_srcdir)/test/pacman/tests/TESTS: $(wildcard test/pacman/tests/*.py)
	@printf "TESTS += %s\n" $^ | LC_ALL=C sort -u > "$@"

TESTS =  test/scripts/parseopts_test.sh \
				 test/scripts/human_to_size_test.sh \
				 test/scripts/makepkg-template_test.sh \
				 test/scripts/pacman-db-upgrade-v9.py \
				 test/util/pacsorttest.sh \
				 test/util/vercmptest.sh
include $(top_srcdir)/test/pacman/tests/TESTS

TEST_SUITE_LOG = test/test-suite.log
TEST_EXTENSIONS = .py
AM_TESTS_ENVIRONMENT = \
	PMTEST_UTIL_DIR=$(top_builddir)/src/util/; export PMTEST_UTIL_DIR; \
	PMTEST_SCRIPT_DIR=$(top_builddir)/scripts/; export PMTEST_SCRIPT_DIR; \
	PMTEST_SCRIPTLIB_DIR=$(top_srcdir)/scripts/library/; export PMTEST_SCRIPTLIB_DIR;
LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
								 $(top_srcdir)/build-aux/tap-driver.sh
PY_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
								 $(top_srcdir)/build-aux/tap-driver.sh
PY_LOG_COMPILER = $(PYTHON) $(top_srcdir)/test/pacman/pactest.py
AM_PY_LOG_FLAGS = \
		--scriptlet-shell $(SCRIPTLET_SHELL) \
		--ldconfig $(LDCONFIG) \
		--bindir $(top_builddir)/src/pacman \
		--bindir $(top_builddir)/scripts

# create the pacman DB, cache, makepkg-template and system hook directories upon install
install-data-local:
	for dir in "$(DESTDIR)$(localstatedir)/lib/pacman" "$(DESTDIR)$(localstatedir)/cache/pacman/pkg" \
	           "$(DESTDIR)$(datarootdir)/makepkg-template" "$(DESTDIR)$(datarootdir)/libalpm/hooks"; do \
	  test -z "$$dir" || $(MKDIR_P) "$$dir"; \
	done

update-po:
	$(MAKE) -C lib/libalpm/po update-po
	$(MAKE) -C scripts/po update-po
	$(MAKE) -C src/pacman/po update-po

update-copyright:
	for file in $(shell sh -c 'git grep -l "Copyright .* Pacman Development" | grep -v "\.po"'); do \
	  sed -i -e "/Copyright (/s/-$(OLD)/-$(NEW)/" -e "/Copyright (/s/ $(OLD)/ $(OLD)-$(NEW)/" "$$file"; \
	done

.PHONY: update-po update-copyright

# vim:set noet:
