# We have to do some funny stuff here with the manpages. In order to ensure
# a dist tarball doesn't get put out there without manpages, we keep those
# files listed in EXTRA_DIST no matter what. However, we only add them to
# man_MANS if --enable-asciidoc and/or --enable-doxygen are used.

ASCIIDOC_MANS = \
	pacman.8 \
	makepkg.8 \
	repo-add.8 \
	PKGBUILD.5 \
	makepkg.conf.5 \
	pacman.conf.5 \
	libalpm.3

DOXYGEN_MANS = $(wildcard man3/*.3)

EXTRA_DIST = \
	asciidoc.conf \
	pacman.8.txt \
	makepkg.8.txt \
	repo-add.8.txt \
	PKGBUILD.5.txt \
	PKGBUILD-example.txt \
	makepkg.conf.5.txt \
	pacman.conf.5.txt \
	libalpm.3.txt \
	footer.txt \
	Doxyfile \
	$(ASCIIDOC_MANS) \
	$(DOXYGEN_MANS)

# Files that should be removed, but which Automake does not know.
MOSTLYCLEANFILES = *.xml $(ASCIIDOC_MANS)

# Ensure manpages are fresh when building a dist tarball
dist-hook:
	$(MAKE) $(AM_MAKEFLAGS) clean
	$(MAKE) $(AM_MAKEFLAGS) all

if USE_GIT_VERSION
GIT_VERSION := $(shell sh -c 'git describe --abbrev=4 | sed s/^v//')-dirty
REAL_PACKAGE_VERSION = $(GIT_VERSION)
else
REAL_PACKAGE_VERSION = $(PACKAGE_VERSION)
endif

man_MANS =
dist_man_MANS = $(ASCIIDOC_MANS) repo-remove.8

if USE_DOXYGEN
man_MANS += $(DOXYGEN_MANS)

all-local: doxygen.in

doxygen.in:
	$(DOXYGEN) $(srcdir)/Doxyfile
endif

ASCIIDOC_OPTS = \
	-f asciidoc.conf \
	-a pacman_version="$(REAL_PACKAGE_VERSION)" \
	-a pacman_date="`date +%Y-%m-%d`" \
	-a sysconfdir=$(sysconfdir)
A2X_OPTS = \
	--no-xmllint \
	-d manpage \
	-f manpage \
	--xsltproc-opts='-param man.endnotes.list.enabled 0' \
	--xsltproc-opts='-param man.endnotes.are.numbered 0'

# These rules are due to the includes and files of the asciidoc text
$(ASCIIDOC_MANS): asciidoc.conf footer.txt
	a2x $(A2X_OPTS) --asciidoc-opts="$(ASCIIDOC_OPTS)" $@.txt

pacman.8: pacman.8.txt
makepkg.8: makepkg.8.txt
repo-add.8: repo-add.8.txt
PKGBUILD.5: PKGBUILD.5.txt PKGBUILD-example.txt
makepkg.conf.5: makepkg.conf.5.txt
pacman.conf.5: pacman.conf.5.txt
libalpm.3: libalpm.3.txt
# this one is just a symlink
repo-remove.8: repo-add.8
	rm -f repo-remove.8
	$(LN_S) repo-add.8 repo-remove.8

# vim:set ts=2 sw=2 noet:
