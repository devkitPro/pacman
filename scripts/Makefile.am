# enforce that all scripts have a --help and --version option
AUTOMAKE_OPTIONS = std-options

SUBDIRS = po

bin_SCRIPTS = \
	$(OURSCRIPTS) \
	makepkg-template \
	makepkg-wrapper \
	repo-remove \
	repo-elephant

OURSCRIPTS = \
	makepkg \
	pacman-db-upgrade \
	pacman-key \
	pacman-optimize \
	pkgdelta \
	repo-add

EXTRA_DIST = \
	makepkg.sh.in \
	makepkg-template.pl.in \
	makepkg-wrapper.sh.in \
	pacman-db-upgrade.sh.in \
	pacman-key.sh.in \
	pacman-optimize.sh.in \
	pkgdelta.sh.in \
	repo-add.sh.in \
	$(LIBRARY)

LIBRARY = \
	library/output_format.sh \
	library/parseopts.sh \
	library/human_to_size.sh \
	library/size_to_human.sh \
	library/term_colors.sh

# Files that should be removed, but which Automake does not know.
MOSTLYCLEANFILES = $(bin_SCRIPTS)

libmakepkgdir = $(libdir)/makepkg

clean-local:
	$(AM_V_at)$(RM) -r .lib

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
	-e 's|@libmakepkgdir[@]|$(libmakepkgdir)|g' \
	-e 's|@pkgdatadir[@]|$(pkgdatadir)|g' \
	-e 's|@prefix[@]|$(prefix)|g' \
	-e '1s|!/bin/bash|!$(BASH_SHELL)|g' \
	-e 's|@PACKAGE_VERSION[@]|$(REAL_PACKAGE_VERSION)|g' \
	-e 's|@PACKAGE_BUGREPORT[@]|$(PACKAGE_BUGREPORT)|g' \
	-e 's|@PACKAGE_NAME[@]|$(PACKAGE_NAME)|g' \
	-e 's|@BUILDSCRIPT[@]|$(BUILDSCRIPT)|g' \
	-e 's|@TEMPLATE_DIR[@]|$(TEMPLATE_DIR)|g' \
	-e 's|@DEBUGSUFFIX[@]|$(DEBUGSUFFIX)|g' \
	-e "s|@INODECMD[@]|$(INODECMD)|g" \
	-e 's|@SIZECMD[@]|$(SIZECMD)|g' \
	-e 's|@SEDINPLACE[@]|$(SEDINPLACE)|g' \
	-e 's|@DUFLAGS[@]|$(DUFLAGS)|g' \
	-e 's|@DUPATH[@]|$(DUPATH)|g' \
	-e 's|@SCRIPTNAME[@]|$@|g' \
	-e 's|@configure_input[@]|Generated from $@.sh.in; do not edit by hand.|g'

## All the scripts depend on Makefile so that they are rebuilt when the
## prefix etc. changes. Use chmod -w to prevent people from editing the
## wrong file by accident.
$(OURSCRIPTS): Makefile
	$(AM_V_at)$(RM) $@
	$(AM_V_GEN)test -f $(srcdir)/$@.sh.in && m4 -P -I $(srcdir) $(srcdir)/$@.sh.in | $(edit) >$@
	$(AM_V_at)chmod +x,a-w $@
	@$(BASH_SHELL) -O extglob -n $@

makepkg: \
	$(srcdir)/makepkg.sh.in \
	$(srcdir)/makepkg-wrapper.sh.in \
	$(srcdir)/library/parseopts.sh

makepkg-template: \
	$(srcdir)/makepkg-template.pl.in \
	Makefile

	$(AM_V_at)$(RM) -f makepkg-template
	$(AM_V_GEN)$(edit) $< > $@
	$(AM_V_at)chmod +x,a-w $@

pacman-db-upgrade: \
	$(srcdir)/pacman-db-upgrade.sh.in \
	$(srcdir)/library/output_format.sh

pacman-key: \
	$(srcdir)/pacman-key.sh.in \
	$(srcdir)/library/output_format.sh \
	$(srcdir)/library/parseopts.sh

pacman-optimize: \
	$(srcdir)/pacman-optimize.sh.in \
	$(srcdir)/library/output_format.sh

pkgdelta: \
	$(srcdir)/pkgdelta.sh.in \
	$(srcdir)/library/output_format.sh \
	$(srcdir)/library/parseopts.sh

repo-add: \
	$(srcdir)/repo-add.sh.in \
	$(srcdir)/library/output_format.sh

repo-remove: $(srcdir)/repo-add.sh.in
	$(AM_V_at)$(RM) repo-remove
	$(AM_V_at)$(LN_S) repo-add repo-remove

repo-elephant: $(srcdir)/repo-add.sh.in
	$(AM_V_at)$(RM) repo-elephant
	$(AM_V_at)$(LN_S) repo-add repo-elephant

makepkg-wrapper: \
	Makefile \
	$(srcdir)/makepkg-wrapper.sh.in \
	$(srcdir)/makepkg.sh.in \
	$(srcdir)/library/parseopts.sh \
	| makepkg
	$(AM_V_at)$(MKDIR_P) .lib
	$(AM_V_at)mv -f makepkg .lib
	$(AM_V_at)$(RM) $@
	$(AM_V_GEN)sed \
		-e "s|@PWD[@]|$$(pwd)|" \
		-e '1s|!/bin/bash|!$(BASH_SHELL)|g' \
		$(srcdir)/$@.sh.in > $@
	$(AM_V_at)chmod +x,a-w $@
	$(AM_V_at)$(LN_S) makepkg-wrapper makepkg

install-data-hook:
	cd $(DESTDIR)$(bindir) && \
		$(RM) makepkg makepkg-wrapper
	$(INSTALL) .lib/makepkg $(DESTDIR)$(bindir)/makepkg
	cd $(DESTDIR)$(bindir) && \
		$(RM) repo-elephant && \
		( $(LN_S) repo-add repo-elephant || \
		ln repo-add repo-elephant || \
		cp repo-add repo-elephant )
	cd $(DESTDIR)$(bindir) && \
		$(RM) repo-remove && \
		( $(LN_S) repo-add repo-remove || \
		ln repo-add repo-remove || \
		cp repo-add repo-remove )

uninstall-hook:
	cd $(DESTDIR)$(bindir) && \
		$(RM) repo-remove repo-elephant

# vim:set noet:
