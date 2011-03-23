# We have to do some funny stuff here with the manpages. In order to ensure
# a dist tarball doesn't get put out there without manpages, we keep those
# files listed in EXTRA_DIST no matter what. However, we only add them to
# man_MANS if --enable-asciidoc and/or --enable-doxygen are used.

ASCIIDOC_MANS = \
	pacman.8 \
	makepkg.8 \
	repo-add.8 \
	vercmp.8 \
	pacman-key.8 \
	PKGBUILD.5 \
	makepkg.conf.5 \
	pacman.conf.5 \
	libalpm.3

DOXYGEN_MANS = $(wildcard man3/*.3)

HTML_MANPAGES = \
	pacman.8.html \
	makepkg.8.html \
	repo-add.8.html \
	vercmp.8.html \
	pacman-key.8.html \
	PKGBUILD.5.html \
	makepkg.conf.5.html \
	pacman.conf.5.html \
	libalpm.3.html

HTML_OTHER = \
	index.html \
	submitting-patches.html \
	translation-help.html \
	HACKING.html

HTML_DOCS = \
	$(HTML_MANPAGES) \
	$(HTML_OTHER)

EXTRA_DIST = \
	asciidoc.conf \
	pacman.8.txt \
	makepkg.8.txt \
	repo-add.8.txt \
	vercmp.8.txt \
	pacman-key.8.txt \
	PKGBUILD.5.txt \
	PKGBUILD-example.txt \
	makepkg.conf.5.txt \
	pacman.conf.5.txt \
	libalpm.3.txt \
	footer.txt \
	index.txt \
	submitting-patches.txt \
	translation-help.txt \
	Doxyfile \
	$(ASCIIDOC_MANS) \
	$(DOXYGEN_MANS)

# Files that should be removed, but which Automake does not know.
MOSTLYCLEANFILES = *.xml $(ASCIIDOC_MANS) $(HTML_DOCS) repo-remove.8 website.tar.gz

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
dist_man_MANS = $(ASCIIDOC_MANS) repo-remove.8

if USE_DOXYGEN
man_MANS += $(DOXYGEN_MANS)

all-local: doxygen.in

doxygen.in:
	$(DOXYGEN) $(srcdir)/Doxyfile
endif

html: $(HTML_DOCS)

website: html
	bsdtar czf website.tar.gz $(HTML_DOCS) \
		-C /etc/asciidoc/stylesheets/ \
		xhtml11.css xhtml11-manpage.css xhtml11-quirks.css \
		-C /etc/asciidoc/javascripts/ \
		asciidoc-xhtml11.js \
		-C /etc/asciidoc/ \
		images

pkgdatadir = ${datadir}/${PACKAGE}

ASCIIDOC_OPTS = \
	-f asciidoc.conf \
	-a pacman_version="$(REAL_PACKAGE_VERSION)" \
	-a pacman_date="`date +%Y-%m-%d`" \
	-a pkgdatadir=$(pkgdatadir) \
	-a localstatedir=$(localstatedir) \
	-a sysconfdir=$(sysconfdir)

A2X_OPTS = \
	--no-xmllint \
	-d manpage \
	-f manpage \
	--xsltproc-opts='-param man.endnotes.list.enabled 0 -param man.endnotes.are.numbered 0'

# These rules are due to the includes and files of the asciidoc text
$(ASCIIDOC_MANS): asciidoc.conf footer.txt
	a2x $(A2X_OPTS) --asciidoc-opts="$(ASCIIDOC_OPTS)" $@.txt

%.html: %.txt
	asciidoc $(ASCIIDOC_OPTS) $*.txt
	dos2unix $@

HACKING.html: ../HACKING
	asciidoc $(ASCIIDOC_OPTS) -o $@ ../HACKING
	dos2unix $@

# Customizations for certain HTML docs
$(HTML_MANPAGES): asciidoc.conf footer.txt
$(HTML_OTHER): asciidoc.conf
%.html: ASCIIDOC_OPTS += -a linkcss -a toc -a icons
%.8.html: ASCIIDOC_OPTS += -d manpage
%.5.html: ASCIIDOC_OPTS += -d manpage
%.3.html: ASCIIDOC_OPTS += -d manpage

# Dependency rules
pacman.8 pacman.8.html: pacman.8.txt
makepkg.8 makepkg.8.html: makepkg.8.txt
repo-add.8 repo-add.8.html: repo-add.8.txt
vercmp.8 vercmp.8.html: vercmp.8.txt
pacman-key.8 pacman-key.8.html: pacman-key.8.txt
PKGBUILD.5 PKGBUILD.5.html: PKGBUILD.5.txt PKGBUILD-example.txt
makepkg.conf.5 makepkg.conf.5.html: makepkg.conf.5.txt
pacman.conf.5 pacman.conf.5.html: pacman.conf.5.txt
libalpm.3 libalpm.3.html: libalpm.3.txt
# this one is just a symlink
repo-remove.8: repo-add.8
	rm -f repo-remove.8
	$(LN_S) repo-add.8 repo-remove.8

# vim:set ts=2 sw=2 noet:
