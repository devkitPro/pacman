all: po4a makepkg.8 PKGBUILD.5 pacman.8

SUBDIRS = hu

po4a:
if HAS_PO4A
	@$(NORMAL_INSTALL)
	po4a -k 0 --variable srcdir=$(srcdir) $(srcdir)/po4a.cfg 
endif

# DIST_SUBDIRS 
if HAS_MAN2HTML
makepkg.8:
	man2html makepkg.8 > html/makepkg.8.html

PKGBUILD.5:
	man2html PKGBUILD.5 > html/PKGBUILD.5.html

pacman.8:
	man2html pacman.8 > html/pacman.8.html
endif

clean:
	rm -f html/*.html
	rm -f hu/*.8
	rm -f man3/*.3

man_MANS = pacman.8 makepkg.8 PKGBUILD.5 libalpm.3

if HAS_DOXYGEN
man_MANS += *.3
endif

EXTRA_DIST = $(man_MANS) po4a.cfg
