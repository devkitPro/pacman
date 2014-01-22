dist_sysconf_DATA = makepkg.conf pacman.conf
EXTRA_DIST = makepkg.conf.in pacman.conf.in

# Files that should be removed, but which Automake does not know.
MOSTLYCLEANFILES = $(dist_sysconf_DATA)

SED_PROCESS = \
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && \
	$(SED) \
	-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
	-e 's|@localstatedir[@]|$(localstatedir)|g' \
	-e 's|@prefix[@]|$(prefix)|g' \
	-e 's|@PACKAGE_VERSION[@]|$(PACKAGE_VERSION)|g' \
	-e 's|@PACKAGE_NAME[@]|$(PACKAGE_NAME)|g' \
	-e 's|@PKGEXT[@]|$(PKGEXT)|g' \
	-e 's|@SRCEXT[@]|$(SRCEXT)|g' \
	-e 's|@STRIP_BINARIES[@]|$(STRIP_BINARIES)|g' \
	-e 's|@STRIP_SHARED[@]|$(STRIP_SHARED)|g' \
	-e 's|@STRIP_STATIC[@]|$(STRIP_STATIC)|g' \
	-e 's|@CARCH[@]|$(CARCH)|g' \
	-e 's|@CHOST[@]|$(CHOST)|g' \
	-e 's|@ARCHSWITCH[@]|$(ARCHSWITCH)|g' \
	-e 's|@ROOTDIR[@]|$(ROOTDIR)|g' \
	< $< > $@

%.conf: %.conf.in Makefile
	$(SED_PROCESS)

# vim:set noet:
