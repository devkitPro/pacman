SUBDIRS = po

# paths set at make time
conffile  = ${sysconfdir}/pacman.conf
dbpath    = ${localstatedir}/lib/pacman/
gpgdir    = ${sysconfdir}/pacman.d/gnupg/
hookdir   = ${sysconfdir}/pacman.d/hooks/
cachedir  = ${localstatedir}/cache/pacman/pkg/
logfile   = ${localstatedir}/log/pacman.log

bin_PROGRAMS = pacman

AM_CPPFLAGS = \
	-imacros $(top_builddir)/config.h \
	-I$(top_srcdir)/lib/libalpm \
	-DLOCALEDIR=\"@localedir@\" \
	-DCONFFILE=\"$(conffile)\" \
	-DDBPATH=\"$(dbpath)\" \
	-DGPGDIR=\"$(gpgdir)\" \
	-DHOOKDIR=\"$(hookdir)\" \
	-DCACHEDIR=\"$(cachedir)\" \
	-DLOGFILE=\"$(logfile)\"

AM_CFLAGS = -pedantic -D_GNU_SOURCE $(WARNING_CFLAGS) \
	$(LIBARCHIVE_CFLAGS)

if USE_GIT_VERSION
GIT_VERSION := $(shell sh -c 'git describe --abbrev=4 --dirty | sed s/^v//')
AM_CPPFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

pacman_SOURCES = \
	check.h check.c \
	conf.h conf.c \
	database.c \
	deptest.c \
	files.c \
	ini.h ini.c \
	package.h package.c \
	pacman.h pacman.c \
	query.c \
	remove.c \
	sighandler.h sighandler.c \
	sync.c \
	callback.h callback.c \
	upgrade.c \
	util.h util.c \
	util-common.h util-common.c

LDADD = $(LTLIBINTL) $(top_builddir)/lib/libalpm/.libs/libalpm.la

# vim:set noet:
