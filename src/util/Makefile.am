bin_PROGRAMS = vercmp testpkg

INCLUDES = -I$(top_srcdir)/lib/libalpm

vercmp_SOURCES = vercmp.c
vercmp_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.so

testpkg_SOURCES = testpkg.c
testpkg_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.so
