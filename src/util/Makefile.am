# paths set at make time
conffile  = ${sysconfdir}/pacman.conf
dbpath    = ${localstatedir}/lib/pacman/
cachedir  = ${localstatedir}/cache/pacman/pkg/

bin_PROGRAMS = vercmp testpkg testdb cleanupdelta pacsort pactree

DEFS = -DLOCALEDIR=\"@localedir@\" \
       -DCONFFILE=\"$(conffile)\" \
       -DDBPATH=\"$(dbpath)\" \
       -DCACHEDIR=\"$(cachedir)\" \
       @DEFS@

AM_CPPFLAGS = \
	-imacros $(top_builddir)/config.h \
	-I$(top_srcdir)/lib/libalpm

AM_CFLAGS = -pedantic -D_GNU_SOURCE

cleanupdelta_SOURCES = cleanupdelta.c
cleanupdelta_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.la

pacsort_SOURCES = pacsort.c
pacsort_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.la

pactree_SOURCES = pactree.c
pactree_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.la

testdb_SOURCES = testdb.c
testdb_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.la

testpkg_SOURCES = testpkg.c
testpkg_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.la

vercmp_SOURCES = vercmp.c
vercmp_LDADD = $(top_builddir)/lib/libalpm/version.lo

# vim:set ts=2 sw=2 noet:
