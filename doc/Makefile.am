all: makepkg.8 pacman.8 PKGBUILD.5 makepkg.conf.5 pacman.conf.5

if HAS_MAN2HTML
makepkg.8:
	man2html makepkg.8 > html/makepkg.8.html

pacman.8:
	man2html pacman.8 > html/pacman.8.html

PKGBUILD.5:
	man2html PKGBUILD.5 > html/PKGBUILD.5.html

makepkg.conf.5:
	man2html makepkg.8 > html/makepkg.8.html

pacman.conf.5:
	man2html pacman.conf.5 > html/pacman.conf.5.html

endif

clean:
	rm -f html/*.html
	rm -f man3/*.3

man_MANS = \
	pacman.8 \
	makepkg.8 \
	PKGBUILD.5 \
	makepkg.conf.5 \
	pacman.conf.5 \
	libalpm.3

if HAS_DOXYGEN
man_MANS += *.3
endif

EXTRA_DIST = $(man_MANS)
