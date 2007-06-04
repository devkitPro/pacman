pkgsysconfdir = ${sysconfdir}/pacman.d
dist_pkgsysconf_DATA = community current extra release testing unstable
EXTRA_DIST = mirrorlist.in
#
# Files that should be removed, but which Automake does not know.
MOSTLYCLEANFILES = $(dist_pkgsysconf_DATA)

$(dist_pkgsysconf_DATA): Makefile
	rm -f $@ $@.tmp
	sed \
		-e "s|@REPO[@]|$@|g" \
		-e 's|@CARCH[@]|$(CARCH)|g' \
		$(srcdir)/mirrorlist.in >$@.tmp
	mv $@.tmp $@

community: $(srcdir)/mirrorlist.in
current: $(srcdir)/mirrorlist.in
extra: $(srcdir)/mirrorlist.in
release: $(srcdir)/mirrorlist.in
testing: $(srcdir)/mirrorlist.in
unstable: $(srcdir)/mirrorlist.in

# vim:set ts=2 sw=2 noet:
