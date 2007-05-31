SUBDIRS = po

# paths set at make time
conffile  = ${sysconfdir}/pacman.conf
lockfile  = ${localstatedir}/run/pacman.lck

bin_PROGRAMS = pacman pacman.static

DEFS = -DLOCALEDIR=\"@localedir@\" \
       -DCONFFILE=\"$(conffile)\" \
       -DLOCKFILE=\"$(lockfile)\" \
       @DEFS@
INCLUDES = -I$(top_srcdir)/lib/libalpm

AM_CFLAGS = -pedantic -D_GNU_SOURCE

pacman_SOURCES = \
	add.c \
	conf.h conf.c \
	deptest.c \
	package.h package.c \
	pacman.h pacman.c \
	query.c \
	remove.c \
	sync.c \
	callback.h callback.c \
	util.h util.c

pacman_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.la \
			   -ldownload

pacman_static_SOURCES = $(pacman_SOURCES)
pacman_static_LDFLAGS = $(LDFLAGS) -all-static
pacman_static_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.la \
					  -ldownload
