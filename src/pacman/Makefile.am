# paths set at make time
conffile  = ${sysconfdir}/pacman.conf
dbpath    = ${localstatedir}/lib/pacman/
cachedir  = ${localstatedir}/cache/pacman/pkg/
logfile   = ${localstatedir}/log/pacman.log

bin_PROGRAMS = pacman

if INCLUDE_PACMAN_STATIC
bin_PROGRAMS += pacman.static
endif

DEFS = -DLOCALEDIR=\"@localedir@\" \
       -DCONFFILE=\"$(conffile)\" \
       -DROOTDIR=\"$(ROOTDIR)\" \
       -DDBPATH=\"$(dbpath)\" \
       -DCACHEDIR=\"$(cachedir)\" \
       -DLOGFILE=\"$(logfile)\" \
       @DEFS@
INCLUDES = -I$(top_srcdir)/lib/libalpm

AM_CFLAGS = -pedantic -D_GNU_SOURCE

pacman_SOURCES = \
	conf.h conf.c \
	deptest.c \
	package.h package.c \
	pacman.h pacman.c \
	query.c \
	remove.c \
	sync.c \
	callback.h callback.c \
	upgrade.c \
	util.h util.c

pacman_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.la

pacman_static_SOURCES = $(pacman_SOURCES)
pacman_static_LDFLAGS = $(LDFLAGS) -all-static
pacman_static_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.la

# vim:set ts=2 sw=2 noet:
