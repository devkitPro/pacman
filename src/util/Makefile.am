bin_PROGRAMS = vercmp testpkg

INCLUDES = -I$(top_srcdir)/lib/libalpm

AM_CFLAGS = -pedantic

vercmp_SOURCES = vercmp.c
vercmp_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.la

testpkg_SOURCES = testpkg.c
testpkg_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.la
