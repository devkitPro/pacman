bin_PROGRAMS = pacman pacman.static

AM_CFLAGS = -D_GNU_SOURCE \
	-I$(top_srcdir)/lib/libalpm \
	-I$(top_srcdir)/lib/libftp

pacman_SOURCES = util.c log.c list.c package.c db.c download.c trans.c add.c \
	remove.c upgrade.c query.c sync.c conf.c deptest.c pacman.c

pacman_static_SOURCES = $(pacman_SOURCES)

pacman_LDADD = \
	$(top_srcdir)/lib/libalpm/libalpm.la \
	$(top_srcdir)/lib/libftp/libftp.la

pacman_static_LDADD = \
	$(top_srcdir)/lib/libalpm/.libs/libalpm.a \
	$(top_srcdir)/lib/libftp/.libs/libftp.a $(LDFLAGS)

pacman_static_LDFLAGS = -all-static
