all: po4a makepkg.8 PKGBUILD.8 pacman.8

SUBDIRS = hu

po4a:
if HAS_PO4A
	@$(NORMAL_INSTALL)
	po4a -k 0 po4a.cfg
endif

if HAS_MAN2HTML
makepkg.8:
	man2html makepkg.8 > html/makepkg.8.html

PKGBUILD.8:
	man2html PKGBUILD.8 > html/PKGBUILD.8.html

pacman.8:
	man2html pacman.8 > html/pacman.8.html
endif

clean:
	rm -f html/*
	rm -f hu/*.8
	rm -f man3/*.3

man_MANS = pacman.8 makepkg.8 PKGBUILD.8 libalpm.3

if HAS_DOXYGEN
man_MANS += man3/*.3
endif

EXTRA_DIST = $(man_MANS)
