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

if INCLUDE_ABS
bin_SCRIPTS += abs
endif

EXTRA_DIST = \
	abs.in \
	gensync.in \
	makepkg.in \
	makeworld.in \
	pacman-optimize.in \
	rankmirrors.in \
	repo-add.in \
	repo-remove.in \
	updatesync.in

# Files that should be removed, but which Automake does not know.
MOSTLYCLEANFILES = $(bin_SCRIPTS) *.tmp

#### Taken from the autoconf scripts Makefile.am ####
edit = sed \
	-e 's|@localedir[@]|$(localedir)|g' \
	-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
	-e 's|@prefix[@]|$(prefix)|g' \
	-e 's|@PACKAGE_VERSION[@]|$(PACKAGE_VERSION)|g' \
    -e 's|@PACKAGE_NAME[@]|$(PACKAGE_NAME)|g' \
    -e 's|@ROOTDIR[@]|$(ROOTDIR)|g' \
    -e 's|@DBPATH[@]|$(DBPATH)|g' \
    -e 's|@LOCKFILE[@]|$(LOCKFILE)|g' \
	-e 's|@configure_input[@]|Generated from $@.in; do not edit by hand.|g'

## All the scripts depend on Makefile so that they are rebuilt when the
## prefix etc. changes. Use chmod -w to prevent people from editing the
## wrong file by accident.
$(bin_SCRIPTS): Makefile
	rm -f $@ $@.tmp
	$(edit) `test -f ./$@.in || echo $(srcdir)/`$@.in >$@.tmp
	chmod +x $@.tmp
	chmod a-w $@.tmp
	mv $@.tmp $@

abs: $(srcdir)/abs.in
gensync: $(srcdir)/gensync.in
makepkg: $(srcdir)/makepkg.in
makeworld: $(srcdir)/makeworld.in
pacman-optimize: $(srcdir)/pacman-optimize.in
rankmirrors: $(srcdir)/rankmirrors.in
repo-add: $(srcdir)/repo-add.in
repo-remove: $(srcdir)/repo-remove.in
re-pacman: $(srcdir)/re-pacman.in
updatesync: $(srcdir)/updatesync.in
