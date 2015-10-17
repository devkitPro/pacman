AUTOMAKE_OPTIONS = gnu

SUBDIRS = po

lib_LTLIBRARIES = libalpm.la
include_HEADERS = alpm_list.h alpm.h

AM_CPPFLAGS = \
	-imacros $(top_builddir)/config.h \
	-DSYSHOOKDIR=\"@datarootdir@/libalpm/hooks/\" \
	-DLOCALEDIR=\"@localedir@\"

AM_CFLAGS = -pedantic -D_GNU_SOURCE $(WARNING_CFLAGS)

if ENABLE_VISIBILITY_CC
if DARWIN
AM_CFLAGS += -fvisibility=hidden
else
AM_CFLAGS += -fvisibility=internal
endif
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libalpm.pc

libalpm_la_SOURCES = \
	add.h add.c \
	alpm.h alpm.c \
	alpm_list.h alpm_list.c \
	backup.h backup.c \
	base64.h base64.c \
	be_local.c \
	be_package.c \
	be_sync.c \
	conflict.h conflict.c \
	db.h db.c \
	delta.h delta.c \
	deps.h deps.c \
	diskspace.h diskspace.c \
	dload.h dload.c \
	error.c \
	filelist.h filelist.c \
	graph.h graph.c \
	group.h group.c \
	handle.h handle.c \
	hook.h hook.c \
	ini.h ini.c \
	libarchive-compat.h \
	log.h log.c \
	package.h package.c \
	pkghash.h pkghash.c \
	rawstr.c \
	remove.h remove.c \
	signing.c signing.h \
	sync.h sync.c \
	trans.h trans.c \
	util.h util.c \
	util-common.h util-common.c \
	version.c

if !HAVE_LIBSSL
libalpm_la_SOURCES += \
	md5.h md5.c \
	sha2.h sha2.c
endif

libalpm_la_LDFLAGS = -no-undefined -version-info $(LIB_VERSION_INFO)

libalpm_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GPGME_CFLAGS) \
	$(LIBARCHIVE_CFLAGS) \
	$(LIBCURL_CFLAGS) \
	$(LIBSSL_CFLAGS)

libalpm_la_LIBADD = \
	$(LTLIBINTL) \
	$(GPGME_LIBS) \
	$(LIBARCHIVE_LIBS) \
	$(LIBCURL_LIBS) \
	$(LIBSSL_LIBS)

# vim:set noet:
