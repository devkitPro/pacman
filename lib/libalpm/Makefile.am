AUTOMAKE_OPTIONS = gnu

SUBDIRS = po

lib_LTLIBRARIES = libalpm.la
include_HEADERS = alpm_list.h alpm.h

DEFS = -DLOCALEDIR=\"@localedir@\" @DEFS@

AM_CFLAGS = -fvisibility=internal -pedantic -D_GNU_SOURCE -fgnu89-inline

libalpm_la_SOURCES = \
	add.h add.c \
	alpm.h alpm.c \
	alpm_list.h alpm_list.c \
	backup.h backup.c \
	be_files.c \
	cache.h cache.c \
	conflict.h conflict.c \
	db.h db.c \
	delta.h delta.c \
	deps.h deps.c \
	error.h error.c \
	group.h group.c \
	handle.h handle.c \
	log.h log.c \
	md5.h md5.c \
	package.h package.c \
	provide.h provide.c \
	remove.h remove.c \
	server.h server.c \
	sync.h sync.c \
	trans.h trans.c \
	util.h util.c

libalpm_la_LDFLAGS = -no-undefined -version-info $(LIB_VERSION_INFO)
libalpm_la_LIBADD = -larchive -ldownload

# vim:set ts=2 sw=2 noet:
