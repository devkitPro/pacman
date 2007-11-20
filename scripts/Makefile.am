# enforce that all scripts have a --help and --version option
AUTOMAKE_OPTIONS = std-options

bin_SCRIPTS = \
	gensync \
	makepkg \
	makeworld \
	pacman-optimize \
	rankmirrors \
	repo-add \
	repo-remove \
	updatesync

EXTRA_DIST = \
	gensync.sh.in \
	makepkg.sh.in \
	makeworld.sh.in \
	pacman-optimize.sh.in \
	rankmirrors.py.in \
	repo-add.sh.in \
	repo-remove.sh.in \
	updatesync.sh.in

# Files that should be removed, but which Automake does not know.
MOSTLYCLEANFILES = $(bin_SCRIPTS) *.tmp

#### Taken from the autoconf scripts Makefile.am ####
edit = sed \
	-e 's|@localedir[@]|$(localedir)|g' \
	-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
	-e 's|@localstatedir[@]|$(localstatedir)|g' \
	-e 's|@prefix[@]|$(prefix)|g' \
	-e 's|@PACKAGE_VERSION[@]|$(PACKAGE_VERSION)|g' \
	-e 's|@PACKAGE_BUGREPORT[@]|$(PACKAGE_BUGREPORT)|g' \
	-e 's|@PACKAGE_NAME[@]|$(PACKAGE_NAME)|g' \
	-e 's|@configure_input[@]|Generated from $@.in; do not edit by hand.|g'

## All the scripts depend on Makefile so that they are rebuilt when the
## prefix etc. changes. Use chmod -w to prevent people from editing the
## wrong file by accident.
# two 'test' lines- make sure we can handle both sh and py type scripts
# third 'test' line- make sure one of the two checks succeeded
$(bin_SCRIPTS): Makefile
	rm -f $@ $@.tmp
	test -f $(srcdir)/$@.sh.in && $(edit) $(srcdir)/$@.sh.in >$@.tmp || true
	test -f $(srcdir)/$@.py.in && $(edit) $(srcdir)/$@.py.in >$@.tmp || true
	test -f $@.tmp || false
	chmod +x $@.tmp
	chmod a-w $@.tmp
	mv $@.tmp $@

gensync: $(srcdir)/gensync.sh.in
makepkg: $(srcdir)/makepkg.sh.in
makeworld: $(srcdir)/makeworld.sh.in
pacman-optimize: $(srcdir)/pacman-optimize.sh.in
rankmirrors: $(srcdir)/rankmirrors.py.in
repo-add: $(srcdir)/repo-add.sh.in
repo-remove: $(srcdir)/repo-remove.sh.in
re-pacman: $(srcdir)/re-pacman.sh.in
updatesync: $(srcdir)/updatesync.sh.in

# vim:set ts=2 sw=2 noet:
