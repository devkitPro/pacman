dist_sysconf_DATA = makepkg.conf pacman.conf
EXTRA_DIST = makepkg.conf.in pacman.conf.in

# Files that should be removed, but which Automake does not know.
MOSTLYCLEANFILES = $(dist_sysconf_DATA)

#### Taken from the autoconf scripts Makefile.am ####
edit = sed \
	-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
	-e 's|@localstatedir[@]|$(localstatedir)|g' \
	-e 's|@prefix[@]|$(prefix)|g' \
	-e 's|@PACKAGE_VERSION[@]|$(PACKAGE_VERSION)|g' \
	-e 's|@PACKAGE_NAME[@]|$(PACKAGE_NAME)|g' \
	-e 's|@PKGEXT[@]|$(PKGEXT)|g' \
	-e 's|@SRCEXT[@]|$(SRCEXT)|g' \
	-e 's|@CARCH[@]|$(CARCH)|g' \
	-e 's|@CHOST[@]|$(CHOST)|g' \
	-e 's|@ARCHSWITCH[@]|$(ARCHSWITCH)|g' \
	-e 's|@CARCHFLAGS[@]|$(CARCHFLAGS)|g' \
	-e 's|@ROOTDIR[@]|$(ROOTDIR)|g'

$(dist_sysconf_DATA): Makefile
	rm -f $@ $@.tmp
	$(edit) `test -f ./$@.in || echo $(srcdir)/`$@.in >$@.tmp
	mv $@.tmp $@

makepkg.conf: $(srcdir)/makepkg.conf.in
pacman.conf: $(srcdir)/pacman.conf.in

# vim:set ts=2 sw=2 noet:
