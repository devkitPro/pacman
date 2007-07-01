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

EXTRA_DIST = $(man_MANS) Doxyfile

if HAS_DOXYGEN
all: doxygen.in

doxygen.in:
	doxygen $(srcdir)/Doxyfile
endif

clean-local:
	$(RM) man3/*.3

# vim:set ts=2 sw=2 noet:
