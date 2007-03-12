SUBDIRS = po

bin_PROGRAMS = pacman

if LINKSTATIC
bin_PROGRAMS += pacman.static
endif

localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@
INCLUDES = -I$(top_srcdir)/lib/libalpm

AM_CFLAGS = -D_GNU_SOURCE

pacman_SOURCES = \
	add.h add.c \
	conf.h conf.c \
	deptest.h deptest.c \
	downloadprog.h downloadprog.c \
	log.h log.c \
	package.h package.c \
	pacman.c \
	query.h query.c \
	remove.h remove.c \
	sync.h sync.c \
	trans.h trans.c \
	upgrade.h upgrade.c \
	util.h util.c

pacman_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.la \
			   -ldownload

pacman_static_SOURCES = $(pacman_SOURCES)
pacman_static_LDFLAGS = $(LDFLAGS) -all-static
pacman_static_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.la \
					  -ldownload
