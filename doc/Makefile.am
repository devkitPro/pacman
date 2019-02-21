# We have to do some funny stuff here with the manpages. In order to ensure
# a dist tarball doesn't get put out there without manpages, we keep those
# files listed in EXTRA_DIST no matter what. However, we only add them to
# man_MANS if --enable-asciidoc and/or --enable-doxygen are used.

MANPAGES = \
	alpm-hooks.5 \
	pacman.8 \
	makepkg.8 \
	makepkg-template.1 \
	repo-add.8 \
	vercmp.8 \
	pkgdelta.8 \
	pacman-key.8 \
	PKGBUILD.5 \
	makepkg.conf.5 \
	pacman.conf.5 \
	libalpm.3 \
	BUILDINFO.5

DOXYGEN_MANS = $(wildcard man3/*.3)

HTML_MANPAGES = $(addsuffix .html,$(MANPAGES))

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
	asciidoc-override.css \
	alpm-hooks.5.asciidoc \
	pacman.8.asciidoc \
	makepkg.8.asciidoc \
	makepkg-template.1.asciidoc \
	repo-add.8.asciidoc \
	vercmp.8.asciidoc \
	pkgdelta.8.asciidoc \
	pacman-key.8.asciidoc \
	PKGBUILD.5.asciidoc \
	PKGBUILD-example.txt \
	makepkg.conf.5.asciidoc \
	pacman.conf.5.asciidoc \
	BUILDINFO.5.asciidoc \
	libalpm.3.asciidoc \
	footer.asciidoc \
	index.asciidoc \
	submitting-patches.asciidoc \
	translation-help.asciidoc \
	$(MANPAGES) \
	$(DOXYGEN_MANS)

# Files that should be removed, but which Automake does not know.
MOSTLYCLEANFILES = *.xml $(MANPAGES) $(HTML_DOCS) repo-remove.8 website.tar.gz

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
dist_man_MANS = $(MANPAGES)

if USE_DOXYGEN
man_MANS += $(DOXYGEN_MANS)

all-local: doxygen.in

Doxyfile: Doxyfile.in
	sed 's,@OUTPUT_DIRECTORY@,./,' Doxyfile.in >Doxyfile

doxygen.in: Doxyfile
	$(DOXYGEN) $(srcdir)/Doxyfile
endif

man: $(MANPAGES)
html: $(HTML_DOCS)

website: website.tar.gz

.PHONY: html website

website.tar.gz: html
	$(AM_V_GEN)bsdtar czf $@ $(HTML_DOCS) \
		asciidoc-override.css \
		-C /etc/asciidoc/stylesheets/ \
		asciidoc.css \
		-C /etc/asciidoc/javascripts/ \
		asciidoc.js \
		-C /etc/asciidoc/ \
		images

pkgdatadir = ${datadir}/${PACKAGE}

ASCIIDOC_OPTS = \
	-f $(srcdir)/asciidoc.conf \
	-a pacman_version="$(REAL_PACKAGE_VERSION)" \
	-a pacman_date="`date +%Y-%m-%d`" \
	-a pkgdatadir=$(pkgdatadir) \
	-a localstatedir=$(localstatedir) \
	-a sysconfdir=$(sysconfdir) \
	-a datarootdir=$(datarootdir) \
	-a rootdir=$(ROOTDIR)

A2X_OPTS = \
	--no-xmllint \
	-d manpage \
	-f manpage \
	--xsltproc-opts='-param man.endnotes.list.enabled 0 -param man.endnotes.are.numbered 0'

# Generate manpages
%: %.asciidoc asciidoc.conf footer.asciidoc Makefile.am
	$(AM_V_GEN)a2x $(A2X_OPTS) --asciidoc-opts="$(ASCIIDOC_OPTS) --out-file=./$@.xml" $(srcdir)/$@.asciidoc

# Generate HTML pages
%.html: %.asciidoc asciidoc.conf footer.asciidoc Makefile.am
	$(AM_V_GEN)asciidoc $(ASCIIDOC_OPTS) -o - $*.asciidoc | \
		sed -e 's/\r$$//' > $@

HACKING.html: ../HACKING
	$(AM_V_GEN)asciidoc $(ASCIIDOC_OPTS) -o - ../HACKING | \
		sed -e 's/\r$$//' > $@

# Customizations for certain HTML docs
%.html: ASCIIDOC_OPTS += -a linkcss -a toc -a icons -a max-width=960px -a stylesheet=asciidoc-override.css
%.8.html: ASCIIDOC_OPTS += -d manpage
%.5.html: ASCIIDOC_OPTS += -d manpage
%.3.html: ASCIIDOC_OPTS += -d manpage

# Custom dependency rules
PKGBUILD.5 PKGBUILD.5.html: PKGBUILD.5.asciidoc PKGBUILD-example.txt

# Manpages as symlinks
repo-remove.8: repo-add.8
	$(RM) repo-remove.8
	$(LN_S) repo-add.8 repo-remove.8

install-data-hook:
	cd $(DESTDIR)$(mandir)/man8 && \
		$(RM) repo-remove.8 && \
		( $(LN_S) repo-add.8 repo-remove.8 || \
		ln repo-add.8 repo-remove.8 || \
		cp repo-add.8 repo-remove.8 )

uninstall-hook:
	$(RM) $(DESTDIR)$(mandir)/man8/repo-remove.8
