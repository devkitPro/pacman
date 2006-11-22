bin_PROGRAMS = vercmp

AM_CFLAGS = \
	-I$(top_srcdir)/lib/libalpm \
	-I$(top_srcdir)/src/pacman \
	$(CFLAGS)

vercmp_SOURCES = vercmp.c

vercmp_LDADD = $(top_srcdir)/lib/libalpm/libalpm.la
