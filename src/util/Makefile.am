# paths set at make time
conffile  = ${sysconfdir}/pacman.conf
dbpath    = ${localstatedir}/lib/pacman/
cachedir  = ${localstatedir}/cache/pacman/pkg/

bin_PROGRAMS = vercmp testpkg testdb

DEFS = -DLOCALEDIR=\"@localedir@\" \
       -DCONFFILE=\"$(conffile)\" \
       -DROOTDIR=\"$(ROOTDIR)\" \
       -DDBPATH=\"$(dbpath)\" \
       -DCACHEDIR=\"$(cachedir)\" \
       @DEFS@
INCLUDES = -I$(top_srcdir)/lib/libalpm

AM_CFLAGS = -pedantic -D_GNU_SOURCE

vercmp_SOURCES = vercmp.c
vercmp_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.la

testpkg_SOURCES = testpkg.c
testpkg_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.la

testdb_SOURCES = testdb.c
testdb_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.la

# vim:set ts=2 sw=2 noet:
