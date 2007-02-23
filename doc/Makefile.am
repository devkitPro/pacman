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

EXTRA_DIST = $(man_MANS)

MAN2HTML=man2html -r
SUFFIXES=.3 .5 .8 .3.html .5.html .8.html

# targets for each man section, sed removes a weird artifact left by man2html
.3.3.html:
	$(RM) $@
	-${MAN2HTML} $< | sed '1,2d' > $@

.5.5.html:
	$(RM) $@
	-${MAN2HTML} $< | sed '1,2d' > $@

.8.8.html:
	$(RM) $@
	-${MAN2HTML} $< | sed '1,2d' > $@

man2html: pacman.8.html makepkg.8.html PKGBUILD.5.html makepkg.conf.5.html pacman.conf.5.html libalpm.3.html

clean-local:
	$(RM) *.html
	$(RM) man3/*.3

.PHONY: man2html
