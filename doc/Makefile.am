man_MANS = \
	pacman.8 \
	makepkg.8 \
	PKGBUILD.5 \
	makepkg.conf.5 \
	pacman.conf.5 \
	libalpm.3

if HAS_DOXYGEN
man_MANS += $(wildcard man3/*.3)
endif

EXTRA_DIST = \
	pacman.8.txt \
	makepkg.8.txt \
	PKGBUILD.5.txt \
	makepkg.conf.5.txt \
	pacman.conf.5.txt \
	libalpm.3.txt \
	footer.txt \
	Doxyfile

# Files that should be removed, but which Automake does not know.
MOSTLYCLEANFILES = $(man_MANS) man3/*.3 *.xml

ASCIIDOC_OPTS = \
	-f asciidoc.conf \
	-apacman_version="$(PACKAGE_VERSION)" \
	-apacman_date="`date +%Y-%m-%d`"

if HAS_DOXYGEN
all: doxygen.in

doxygen.in:
	doxygen $(srcdir)/Doxyfile
endif

$(man_MANS): footer.txt
	a2x -d manpage -f manpage --asciidoc-opts="$(ASCIIDOC_OPTS)" $@.txt

# vim:set ts=2 sw=2 noet:
