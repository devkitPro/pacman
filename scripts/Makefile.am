# enforce that all scripts have a --help and --version option
AUTOMAKE_OPTIONS = std-options

SUBDIRS = po

bin_SCRIPTS = \
	$(OURSCRIPTS) \
	repo-remove

OURSCRIPTS = \
	makepkg \
	pacman-db-upgrade \
	pacman-key \
	pacman-optimize \
	pkgdelta \
	rankmirrors \
	repo-add

EXTRA_DIST = \
	makepkg.sh.in \
	pacman-db-upgrade.sh.in \
	pacman-key.sh.in \
	pacman-optimize.sh.in \
	pkgdelta.sh.in \
	rankmirrors.sh.in \
	repo-add.sh.in \
	$(LIBRARY)

LIBRARY = \
	library/output_format.sh \
	library/parse_options.sh

# Files that should be removed, but which Automake does not know.
MOSTLYCLEANFILES = $(bin_SCRIPTS)

if USE_GIT_VERSION
GIT_VERSION := $(shell sh -c 'git describe --abbrev=4 --dirty | sed s/^v//')
REAL_PACKAGE_VERSION = $(GIT_VERSION)
else
REAL_PACKAGE_VERSION = $(PACKAGE_VERSION)
endif

#### Taken from the autoconf scripts Makefile.am ####
edit = sed \
	-e 's|@localedir[@]|$(localedir)|g' \
	-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
	-e 's|@localstatedir[@]|$(localstatedir)|g' \
	-e 's|@prefix[@]|$(prefix)|g' \
	-e 's|@BASH_SHELL[@]|$(BASH_SHELL)|g' \
	-e 's|@PACKAGE_VERSION[@]|$(REAL_PACKAGE_VERSION)|g' \
	-e 's|@PACKAGE_BUGREPORT[@]|$(PACKAGE_BUGREPORT)|g' \
	-e 's|@PACKAGE_NAME[@]|$(PACKAGE_NAME)|g' \
	-e 's|@BUILDSCRIPT[@]|$(BUILDSCRIPT)|g' \
	-e 's|@SIZECMD[@]|$(SIZECMD)|g' \
	-e 's|@SEDINPLACE[@]|$(SEDINPLACE)|g' \
	-e 's|@DUPATH[@]|$(DUPATH)|g' \
	-e 's|@SCRIPTNAME[@]|$@|g' \
	-e 's|@configure_input[@]|Generated from $@.sh.in; do not edit by hand.|g'

## All the scripts depend on Makefile so that they are rebuilt when the
## prefix etc. changes. Use chmod -w to prevent people from editing the
## wrong file by accident.
# two 'test' lines- make sure we can handle both sh and py type scripts
# third 'test' line- make sure one of the two checks succeeded
$(OURSCRIPTS): Makefile
	@echo '    ' GEN $@;
	@rm -f $@
	@test -f $(srcdir)/$@.sh.in && m4 -P -I $(srcdir) $(srcdir)/$@.sh.in | $(edit) >$@
	@chmod +x $@
	@chmod a-w $@

makepkg: \
	$(srcdir)/makepkg.sh.in \
	$(srcdir)/library/parse_options.sh

pacman-db-upgrade: \
	$(srcdir)/pacman-db-upgrade.sh.in \
	$(srcdir)/library/output_format.sh

pacman-key: \
	$(srcdir)/pacman-key.sh.in \
	$(srcdir)/library/output_format.sh

pacman-optimize: $(srcdir)/pacman-optimize.sh.in

pkgdelta: \
	$(srcdir)/pkgdelta.sh.in \
	$(srcdir)/library/output_format.sh

rankmirrors: $(srcdir)/rankmirrors.sh.in

repo-add: \
	$(srcdir)/repo-add.sh.in \
	$(srcdir)/library/output_format.sh

repo-remove: $(srcdir)/repo-add.sh.in
	rm -f repo-remove
	$(LN_S) repo-add repo-remove

# vim:set ts=2 sw=2 noet:
