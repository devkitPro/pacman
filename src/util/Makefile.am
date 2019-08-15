EXTRA_DIST = meson.build

# paths set at make time
conffile  = ${sysconfdir}/pacman.conf
dbpath    = ${localstatedir}/lib/pacman/
gpgdir    = ${sysconfdir}/pacman.d/gnupg/
cachedir  = ${localstatedir}/cache/pacman/pkg/

bin_PROGRAMS = vercmp testpkg

AM_CPPFLAGS = \
	-imacros $(top_builddir)/config.h \
	-I$(top_srcdir)/lib/libalpm \
	-DLOCALEDIR=\"@localedir@\" \
	-DCONFFILE=\"$(conffile)\" \
	-DDBPATH=\"$(dbpath)\" \
	-DGPGDIR=\"$(gpgdir)\" \
	-DCACHEDIR=\"$(cachedir)\"

AM_CFLAGS = -pedantic -D_GNU_SOURCE $(WARNING_CFLAGS) \
	$(LIBARCHIVE_CFLAGS)

testpkg_SOURCES = testpkg.c
testpkg_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.la

vercmp_SOURCES = vercmp.c
vercmp_LDADD = $(top_builddir)/lib/libalpm/libalpm_la-version.lo
