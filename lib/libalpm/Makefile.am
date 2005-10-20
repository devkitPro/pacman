AUTOMAKE_OPTIONS = gnu
DEFINES = -pedantic -D_GNU_SOURCE
AM_CFLAGS = $(DEFINES)

TARGETS = md5driver.c \
	md5.c \
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
	rpmvercmp.c \
	backup.c \
	trans.c \
	add.c \
	remove.c \
	sync.c \
	handle.c \
	alpm.c

lib_LTLIBRARIES = libalpm.la

include_HEADERS = alpm.h

libalpm_la_SOURCES = $(TARGETS)

libalpm_la_LDFLAGS = -no-undefined -version-info $(PM_VERSION_INFO)
