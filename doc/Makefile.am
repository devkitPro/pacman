ASCIIDOC_MANS = \
	pacman.8 \
	makepkg.8 \
	PKGBUILD.5 \
	makepkg.conf.5 \
	pacman.conf.5 \
	libalpm.3

man_MANS = $(ASCIIDOC_MANS)

if HAS_DOXYGEN
man_MANS += $(wildcard man3/*.3)
endif

EXTRA_DIST = \
	pacman.8.txt \
	makepkg.8.txt \
	PKGBUILD.5.txt \
	PKGBUILD-example.txt \
	makepkg.conf.5.txt \
	pacman.conf.5.txt \
	libalpm.3.txt \
	footer.txt \
	Doxyfile

# Files that should be removed, but which Automake does not know.
MOSTLYCLEANFILES = $(man_MANS) man3/*.3 *.xml

ASCIIDOC_OPTS = \
	-f asciidoc.conf \
	-a pacman_version="$(PACKAGE_VERSION)" \
	-a pacman_date="`date +%Y-%m-%d`" \
	-a sysconfdir=$(sysconfdir)

if HAS_DOXYGEN
all: doxygen.in

doxygen.in:
	doxygen $(srcdir)/Doxyfile
endif

$(ASCIIDOC_MANS):
	a2x -d manpage -f manpage --asciidoc-opts="$(ASCIIDOC_OPTS)" $@.txt

# These rules are due to the includes and files of the asciidoc text
$(ASCIIDOC_MANS): footer.txt
pacman.8: pacman.8.txt
makepkg.8: makepkg.8.txt
PKGBUILD.5: PKGBUILD.5.txt PKGBUILD-example.txt
makepkg.conf.5: makepkg.conf.5.txt
pacman.conf.5: pacman.conf.5.txt
libalpm.3: libalpm.3.txt

# vim:set ts=2 sw=2 noet:
