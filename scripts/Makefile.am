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
	repo-remove \
	repo-elephant \
	$(WRAPPER)

OURSCRIPTS = \
	makepkg \
	pacman-db-upgrade \
	pacman-key \
	pkgdelta \
	repo-add

EXTRA_DIST = \
	makepkg.sh.in \
	makepkg-template.pl.in \
	pacman-db-upgrade.sh.in \
	pacman-key.sh.in \
	pkgdelta.sh.in \
	repo-add.sh.in \
	wrapper.sh.in \
	$(COMPLETION_DIST) \
	$(LIBRARY) \
	$(LIBMAKEPKG_DIST)

LIBRARY = \
	library/human_to_size.sh

libmakepkgdir = $(datarootdir)/makepkg

LIBMAKEPKGDIRS = \
	buildenv \
	executable \
	integrity \
	lint_config \
	lint_package \
	lint_pkgbuild \
	source \
	tidy \
	util

LIBMAKEPKG_IN = \
	libmakepkg/executable.sh \
	libmakepkg/executable/ccache.sh \
	libmakepkg/executable/checksum.sh \
	libmakepkg/executable/distcc.sh \
	libmakepkg/executable/fakeroot.sh \
	libmakepkg/executable/gpg.sh \
	libmakepkg/executable/gzip.sh \
	libmakepkg/executable/pacman.sh \
	libmakepkg/executable/strip.sh \
	libmakepkg/executable/sudo.sh \
	libmakepkg/executable/vcs.sh \
	libmakepkg/buildenv.sh \
	libmakepkg/buildenv/buildflags.sh \
	libmakepkg/buildenv/compiler.sh \
	libmakepkg/buildenv/debugflags.sh \
	libmakepkg/buildenv/makeflags.sh \
	libmakepkg/integrity.sh \
	libmakepkg/integrity/generate_checksum.sh \
	libmakepkg/integrity/generate_signature.sh \
	libmakepkg/integrity/verify_checksum.sh \
	libmakepkg/integrity/verify_signature.sh \
	libmakepkg/lint_config.sh \
	libmakepkg/lint_config/paths.sh \
	libmakepkg/lint_config/variable.sh \
	libmakepkg/lint_package.sh \
	libmakepkg/lint_package/build_references.sh \
	libmakepkg/lint_package/dotfiles.sh \
	libmakepkg/lint_package/file_names.sh \
	libmakepkg/lint_package/missing_backup.sh \
	libmakepkg/lint_pkgbuild.sh \
	libmakepkg/lint_pkgbuild/arch.sh \
	libmakepkg/lint_pkgbuild/backup.sh \
	libmakepkg/lint_pkgbuild/changelog.sh \
	libmakepkg/lint_pkgbuild/checkdepends.sh \
	libmakepkg/lint_pkgbuild/conflicts.sh \
	libmakepkg/lint_pkgbuild/depends.sh \
	libmakepkg/lint_pkgbuild/epoch.sh \
	libmakepkg/lint_pkgbuild/install.sh \
	libmakepkg/lint_pkgbuild/makedepends.sh \
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
	libmakepkg/srcinfo.sh \
	libmakepkg/tidy.sh \
	libmakepkg/tidy/docs.sh \
	libmakepkg/tidy/emptydirs.sh \
	libmakepkg/tidy/libtool.sh \
	libmakepkg/tidy/purge.sh \
	libmakepkg/tidy/staticlibs.sh \
	libmakepkg/tidy/strip.sh \
	libmakepkg/tidy/zipman.sh \
	libmakepkg/util.sh \
	libmakepkg/util/compress.sh \
	libmakepkg/util/error.sh \
	libmakepkg/util/message.sh \
	libmakepkg/util/option.sh \
	libmakepkg/util/parseopts.sh \
	libmakepkg/util/pkgbuild.sh \
	libmakepkg/util/source.sh \
	libmakepkg/util/util.sh

LIBMAKEPKG_DIST = \
	$(addsuffix .in, $(LIBMAKEPKG_IN))

WRAPPER = \
	makepkg-wrapper \
	pacman-db-upgrade-wrapper \
	pacman-key-wrapper \
	pkgdelta-wrapper

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
	-e 's|@SEDINPLACEFLAGS[@]|$(SEDINPLACEFLAGS)|g' \
	-e 's|@SEDPATH[@]|$(SEDPATH)|g' \
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
	$(srcdir)/wrapper.sh.in \
	$(LIBMAKEPKG_IN)

makepkg-template: \
	$(srcdir)/makepkg-template.pl.in \
	Makefile

	$(AM_V_at)$(RM) -f makepkg-template
	$(AM_V_GEN)$(edit) $< > $@
	$(AM_V_at)chmod +x,a-w $@

pacman-db-upgrade: $(srcdir)/pacman-db-upgrade.sh.in

pacman-key: $(srcdir)/pacman-key.sh.in

pkgdelta: $(srcdir)/pkgdelta.sh.in

repo-add: $(srcdir)/repo-add.sh.in

repo-remove: $(srcdir)/repo-add.sh.in
	$(AM_V_at)$(RM) repo-remove
	$(AM_V_at)$(LN_S) repo-add repo-remove

repo-elephant: $(srcdir)/repo-add.sh.in
	$(AM_V_at)$(RM) repo-elephant
	$(AM_V_at)$(LN_S) repo-add repo-elephant

.SECONDEXPANSION:
$(WRAPPER): \
	$$(subst -wrapper,,$$@)

	$(AM_V_at)$(MKDIR_P) .lib
	$(AM_V_at)mv -f $(subst -wrapper,,$@) .lib
	$(AM_V_at)$(RM) $@
	$(AM_V_GEN)sed \
		-e "s|@PWD[@]|$$(pwd)|" \
		-e "s|@PROGNAME[@]|$(subst -wrapper,,$@)|g" \
		-e '1s|!/bin/bash|!$(BASH_SHELL)|g' \
		$(srcdir)/wrapper.sh.in > $@
	$(AM_V_at)chmod +x,a-w $@
	$(AM_V_at)$(LN_S) $@ $(subst -wrapper,,$@)

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(bashcompdir)
	$(INSTALL_DATA) completion/bash_completion $(DESTDIR)/$(bashcompdir)/pacman
	for completion in makepkg pacman-key; do \
	    $(LN_S) pacman $(DESTDIR)/$(bashcompdir)/$$completion; \
	done
	$(MKDIR_P) $(DESTDIR)$(datarootdir)/zsh/site-functions/
	$(INSTALL_DATA) completion/zsh_completion $(DESTDIR)$(datarootdir)/zsh/site-functions/_pacman

uninstall-local:
	$(RM) $(DESTDIR)$(bashcompdir)/makepkg
	$(RM) $(DESTDIR)$(bashcompdir)/pacman
	$(RM) $(DESTDIR)$(bashcompdir)/pacman-key
	$(RM) $(DESTDIR)$(datarootdir)/zsh/site-functions/_pacman

install-exec-hook:
	for wrapper in $(WRAPPER); do \
		$(RM) $(DESTDIR)$(bindir)/$${wrapper}; \
		$(INSTALL) .lib/$${wrapper%-wrapper} $(DESTDIR)$(bindir)/$${wrapper%-wrapper}; \
	done
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
