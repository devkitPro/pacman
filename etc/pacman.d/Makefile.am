pkgsysconfdir = ${sysconfdir}/pacman.d
dist_pkgsysconf_DATA = mirrorlist
EXTRA_DIST = mirrorlist.in
#
# Files that should be removed, but which Automake does not know.
MOSTLYCLEANFILES = $(dist_pkgsysconf_DATA)

$(dist_pkgsysconf_DATA): Makefile
	rm -f $@ $@.tmp
	sed \
		-e 's|@CARCH[@]|$(CARCH)|g' \
		$(srcdir)/mirrorlist.in >$@.tmp
	mv $@.tmp $@

mirrorlist: $(srcdir)/mirrorlist.in

# vim:set ts=2 sw=2 noet:
