bin_PROGRAMS = pacman

if LINKSTATIC
bin_PROGRAMS += pacman.static
endif

SUBDIRS = po

localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

AM_CFLAGS = -D_GNU_SOURCE -I$(top_srcdir)/lib/libalpm $(CFLAGS) 

pacman_SOURCES = util.c log.c list.c package.c downloadprog.c trans.c add.c \
	remove.c upgrade.c query.c sync.c conf.c deptest.c pacman.c

pacman_static_SOURCES = $(pacman_SOURCES)

pacman_LDADD = -L$(top_srcdir)/lib/libalpm/.libs \
			   -ldownload -lm -lalpm

pacman_static_LDADD = -L$(top_srcdir)/lib/libalpm/.libs/ \
					  -ldownload -lm -lalpm

pacman_static_LDFLAGS = $(LDFLAGS) -all-static
