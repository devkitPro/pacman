# enforce that all scripts have a --help and --version option
AUTOMAKE_OPTIONS = std-options
AM_INSTALLCHECK_STD_OPTIONS_EXEMPT = \
	makepkg-wrapper \
	pacman-db-upgrade-wrapper \
	pacman-key-wrapper \
	pkgdelta-wrapper

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
	$(COMPLETION_DIST) \
	$(LIBRARY) \
	$(LIBMAKEPKG_DIST)

LIBRARY = \
	library/output_format.sh \
	library/parseopts.sh \
	library/human_to_size.sh \
	library/size_to_human.sh \
	library/term_colors.sh

libmakepkgdir = $(datarootdir)/makepkg

LIBMAKEPKGDIRS = \
	lint_package \
	lint_pkgbuild \
	source \
	tidy \
	util

LIBMAKEPKG_IN = \
	libmakepkg/lint_package.sh \
	libmakepkg/lint_package/build_references.sh \
	libmakepkg/lint_package/missing_backup.sh \
	libmakepkg/lint_pkgbuild.sh \
	libmakepkg/lint_pkgbuild/arch.sh \
	libmakepkg/lint_pkgbuild/backup.sh \
	libmakepkg/lint_pkgbuild/changelog.sh \
	libmakepkg/lint_pkgbuild/epoch.sh \
	libmakepkg/lint_pkgbuild/install.sh \
	libmakepkg/lint_pkgbuild/optdepends.sh \
	libmakepkg/lint_pkgbuild/options.sh \
	libmakepkg/lint_pkgbuild/package_function.sh \
	libmakepkg/lint_pkgbuild/pkgbase.sh \
	libmakepkg/lint_pkgbuild/pkglist.sh \
	libmakepkg/lint_pkgbuild/pkgname.sh \
	libmakepkg/lint_pkgbuild/pkgrel.sh \
	libmakepkg/lint_pkgbuild/pkgver.sh \
	libmakepkg/lint_pkgbuild/provides.sh \
	libmakepkg/lint_pkgbuild/source.sh \
	libmakepkg/lint_pkgbuild/util.sh \
	libmakepkg/lint_pkgbuild/variable.sh \
	libmakepkg/source.sh \
	libmakepkg/source/bzr.sh \
	libmakepkg/source/file.sh \
	libmakepkg/source/git.sh \
	libmakepkg/source/hg.sh \
	libmakepkg/source/local.sh \
	libmakepkg/source/svn.sh \
	libmakepkg/tidy.sh \
	libmakepkg/tidy/docs.sh \
	libmakepkg/tidy/emptydirs.sh \
	libmakepkg/tidy/libtool.sh \
	libmakepkg/tidy/optipng.sh \
	libmakepkg/tidy/purge.sh \
	libmakepkg/tidy/staticlibs.sh \
	libmakepkg/tidy/strip.sh \
	libmakepkg/tidy/upx.sh \
	libmakepkg/tidy/zipman.sh \
	libmakepkg/util.sh \
	libmakepkg/util/message.sh \
	libmakepkg/util/option.sh \
	libmakepkg/util/pkgbuild.sh \
	libmakepkg/util/source.sh \
	libmakepkg/util/util.sh

LIBMAKEPKG_DIST = \
	$(addsuffix .in, $(LIBMAKEPKG_IN))

COMPLETION_IN = \
	completion/bash_completion \
	completion/zsh_completion

COMPLETION_DIST = \
	$(addsuffix .in, $(COMPLETION_IN))

# Files that should be removed, but which Automake does not know.
MOSTLYCLEANFILES = $(bin_SCRIPTS) $(LIBMAKEPKG_IN) $(COMPLETION_IN)

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
	-e 's|@rootdir[@]|$(ROOTDIR)|g' \
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
	-e "s|@OWNERCMD[@]|$(OWNERCMD)|g" \
	-e "s|@MODECMD[@]|$(MODECMD)|g" \
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

$(LIBMAKEPKG_IN): %: %.in Makefile
	$(AM_V_at)$(RM) $@
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_GEN)test -f $(srcdir)/$@.in && $(edit) $(srcdir)/$@.in >$@
	$(AM_V_at)chmod a-w $@
	@$(BASH_SHELL) -O extglob -n $@

$(COMPLETION_IN): %: %.in Makefile
	$(AM_V_at)$(RM) $@
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_GEN)$(edit) $(srcdir)/$@.in >$@
	$(AM_V_at)chmod a-w $@

all-am: $(COMPLETION_IN)

makepkg: \
	$(srcdir)/makepkg.sh.in \
	$(srcdir)/makepkg-wrapper.sh.in \
	$(srcdir)/library/parseopts.sh \
	$(LIBMAKEPKG_IN)

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
	makepkg

	$(AM_V_at)$(MKDIR_P) .lib
	$(AM_V_at)mv -f makepkg .lib
	$(AM_V_at)$(RM) $@
	$(AM_V_GEN)sed \
		-e "s|@PWD[@]|$$(pwd)|" \
		-e '1s|!/bin/bash|!$(BASH_SHELL)|g' \
		$(srcdir)/$@.sh.in > $@
	$(AM_V_at)chmod +x,a-w $@
	$(AM_V_at)$(LN_S) makepkg-wrapper makepkg

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(sysconfdir)/bash_completion.d/
	$(INSTALL_DATA) completion/bash_completion $(DESTDIR)$(sysconfdir)/bash_completion.d/pacman
	$(MKDIR_P) $(DESTDIR)$(datarootdir)/zsh/site-functions/
	$(INSTALL_DATA) completion/zsh_completion $(DESTDIR)$(datarootdir)/zsh/site-functions/_pacman

uninstall-local:
	$(RM) $(DESTDIR)$(sysconfdir)/bash_completion.d/pacman
	$(RM) $(DESTDIR)$(datarootdir)/zsh/site-functions/_pacman

install-exec-hook:
	cd $(DESTDIR)$(bindir) && \
		$(RM) makepkg makepkg-wrapper
	$(INSTALL) .lib/makepkg $(DESTDIR)$(bindir)/makepkg
	for dir in $(LIBMAKEPKGDIRS); do \
		$(MKDIR_P) $(DESTDIR)$(libmakepkgdir)/$$dir; \
	done
	for lib in $(LIBMAKEPKG); do \
		$(INSTALL) $(srcdir)/$$lib $(DESTDIR)$(libmakepkgdir)/$${lib#libmakepkg}; \
	done
	for lib in $(LIBMAKEPKG_IN); do \
		$(INSTALL) $$lib $(DESTDIR)$(libmakepkgdir)/$${lib#libmakepkg}; \
	done
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
	for lib in $(LIBMAKEPKG) $(LIBMAKEPKG_IN); do \
		$(RM) $(DESTDIR)$(libmakepkgdir)/$${lib#libmakepkg}; \
	done
	for dir in $(LIBMAKEPKGDIRS); do \
		$(RM) -r $(DESTDIR)$(libmakepkgdir)/$$dir; \
	done


# vim:set noet:
