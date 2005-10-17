bin_PROGRAMS = vercmp convertdb

AM_CFLAGS = \
	-I$(top_srcdir)/lib/libalpm \
	-I$(top_srcdir)/src/pacman

vercmp_SOURCES = vercmp.c
convertdb_SOURCES = convertdb.c

vercmp_LDADD = $(top_srcdir)/lib/libalpm/libalpm.la
convertdb_LDADD = $(top_srcdir)/lib/libalpm/libalpm.la
