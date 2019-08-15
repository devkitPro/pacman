SUBDIRS = po

EXTRA_DIST = meson.build po/meson.build

# paths set at make time
conffile  = ${sysconfdir}/pacman.conf
dbpath    = ${localstatedir}/lib/pacman/
gpgdir    = ${sysconfdir}/pacman.d/gnupg/
hookdir   = ${sysconfdir}/pacman.d/hooks/
cachedir  = ${localstatedir}/cache/pacman/pkg/
logfile   = ${localstatedir}/log/pacman.log

bin_PROGRAMS = pacman pacman-conf

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

pacman_LDADD = \
	$(LTLIBINTL) \
	$(top_builddir)/lib/libalpm/.libs/libalpm.la \
	$(LIBARCHIVE_LIBS)

pacman_conf_SOURCES = pacman-conf.c \
	util.h \
	util.c \
	ini.h \
	ini.c \
	util-common.h \
	util-common.c \
	callback.h \
	callback.c \
	conf.h \
	conf.c

pacman_conf_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.la
