# We have to do some funny stuff here with the manpages. In order to ensure
# a dist tarball doesn't get put out there without manpages, we keep those
# files listed in EXTRA_DIST no matter what. However, we only add them to
# man_MANS if --enable-asciidoc and/or --enable-doxygen are used.

ASCIIDOC_MANS = \
	verify-pacman-repo-db.1 \
	bacman.8

EXTRA_DIST = \
	asciidoc.conf \
	footer.txt \
	verify-pacman-repo-db.1.txt \
	bacman.8.txt \
	$(ASCIIDOC_MANS)

# Files that should be removed, but which Automake does not know.
MOSTLYCLEANFILES = *.xml $(ASCIIDOC_MANS)

# Ensure manpages are fresh when building a dist tarball
dist-hook:
	$(MAKE) $(AM_MAKEFLAGS) clean
	$(MAKE) $(AM_MAKEFLAGS) all

if USE_GIT_VERSION
GIT_VERSION := $(shell sh -c 'git describe --abbrev=4 --dirty | sed s/^v//')
REAL_PACKAGE_VERSION = $(GIT_VERSION)
else
REAL_PACKAGE_VERSION = $(PACKAGE_VERSION)
endif

man_MANS =
dist_man_MANS = $(ASCIIDOC_MANS)

pkgdatadir = ${datadir}/${PACKAGE}

ASCIIDOC_OPTS = \
	-f $(srcdir)/asciidoc.conf \
	-a pacman_version="$(REAL_PACKAGE_VERSION)" \
	-a pacman_date="`date +%Y-%m-%d`" \
	-a pkgdatadir=$(pkgdatadir) \
	-a localstatedir=$(localstatedir) \
	-a sysconfdir=$(sysconfdir) \
	-a datarootdir=$(datarootdir)

A2X_OPTS = \
	--no-xmllint \
	-d manpage \
	-f manpage \
	--xsltproc-opts='-param man.endnotes.list.enabled 0 -param man.endnotes.are.numbered 0'

# These rules are due to the includes and files of the asciidoc text
$(ASCIIDOC_MANS): asciidoc.conf footer.txt Makefile.am
	$(AM_V_GEN)a2x $(A2X_OPTS) --asciidoc-opts="$(ASCIIDOC_OPTS) --out-file=./$@" $(srcdir)/$@.txt

# Dependency rules
verify-pacman-repo-db.1: verify-pacman-repo-db.1.txt
bacman.8: bacman.8.txt

# vim:set noet:
