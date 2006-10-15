AUTOMAKE_OPTIONS = gnu
DEFINES = -pedantic -D_GNU_SOURCE
AM_CFLAGS = $(DEFINES) \
	    -I$(top_srcdir)/lib/libftp
SUBDIRS = po

localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

TARGETS = md5driver.c \
	md5.c \
	sha1.c \
	util.c \
	list.c \
	log.c \
	error.c \
	package.c \
	group.c \
	db.c \
	cache.c \
	conflict.c \
	deps.c \
	provide.c \
	versioncmp.c \
	backup.c \
	trans.c \
	add.c \
	remove.c \
	sync.c \
	handle.c \
	server.c \
	alpm.c \
	be_files.c

lib_LTLIBRARIES = libalpm.la

include_HEADERS = alpm.h

libalpm_la_SOURCES = $(TARGETS)

libalpm_la_LDFLAGS = -no-undefined -version-info $(PM_VERSION_INFO)
libalpm_la_LIBADD = $(top_srcdir)/lib/libftp/libftp.la

if HAS_DOXYGEN
all: doxygen.in

doxygen.in:
	doxygen Doxyfile

endif
