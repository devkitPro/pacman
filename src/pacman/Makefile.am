bin_PROGRAMS = pacman pacman.static

SUBDIRS = po

localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

AM_CFLAGS = $(CFLAGS) -D_GNU_SOURCE \
	-I$(top_srcdir)/lib/libalpm \
	-I$(top_srcdir)/lib/libfetch

pacman_SOURCES = util.c log.c list.c package.c download.c trans.c add.c \
	remove.c upgrade.c query.c sync.c conf.c deptest.c pacman.c

pacman_static_SOURCES = $(pacman_SOURCES)

pacman_LDADD = -L$(top_srcdir)/lib/libalpm/.libs \
			   -L$(top_srcdir)/lib/libfetch -lfetch \
			   -larchive -lm -lalpm -lssl -lcrypto

pacman_static_LDADD = -L$(top_srcdir)/lib/libalpm/.libs/ \
					  -L$(top_srcdir)/lib/libfetch -lfetch \
					  -larchive -lm -lalpm -lssl -lcrypto

pacman_static_LDFLAGS = $(LDFLAGS) -all-static
