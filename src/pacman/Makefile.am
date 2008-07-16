# paths set at make time
conffile  = ${sysconfdir}/pacman.conf
dbpath    = ${localstatedir}/lib/pacman/
cachedir  = ${localstatedir}/cache/pacman/pkg/
logfile   = ${localstatedir}/log/pacman.log

bin_PROGRAMS = pacman

DEFS = -DLOCALEDIR=\"@localedir@\" \
       -DCONFFILE=\"$(conffile)\" \
       -DROOTDIR=\"$(ROOTDIR)\" \
       -DDBPATH=\"$(dbpath)\" \
       -DCACHEDIR=\"$(cachedir)\" \
       -DLOGFILE=\"$(logfile)\" \
       @DEFS@
INCLUDES = -I$(top_srcdir)/lib/libalpm

AM_CFLAGS = -pedantic -D_GNU_SOURCE

if USE_GIT_VERSION
GIT_VERSION := $(shell sh -c 'git describe --abbrev=4 | sed s/^v//')-dirty
DEFS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

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

LDADD = $(LTLIBINTL) $(top_builddir)/lib/libalpm/.libs/libalpm.la

# vim:set ts=2 sw=2 noet:
