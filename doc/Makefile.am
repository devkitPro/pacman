all: makepkg.8 pacman.8

if HAS_MAN2HTML
makepkg.8:
	sed -e 's,#PACMAN_VERSION#,$(PACMAN_VERSION),g' < makepkg.8.in > makepkg.8
	man2html makepkg.8 > html/makepkg.8.html
else
makepkg.8:
	sed -e 's,#PACMAN_VERSION#,$(PACMAN_VERSION),g' < makepkg.8.in > makepkg.8
endif

if HAS_MAN2HTML
pacman.8:
	sed -e 's,#PACMAN_VERSION#,$(PACMAN_VERSION),g' < pacman.8.in > pacman.8
	man2html pacman.8 > html/pacman.8.html
else
pacman.8:
	sed -e 's,#PACMAN_VERSION#,$(PACMAN_VERSION),g' < pacman.8.in > pacman.8
endif

clean:
	rm -rf *.8
	rm -rf html/*

man_MANS: pacman.8 makepkg.8
EXTRA_DIST = $(man_MANS)
