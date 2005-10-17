AUTOMAKE_OPTIONS = gnu
DEFINES = -DFTPLIB_DEFMODE=FTPLIB_PORT -D_REENTRANT
AM_CFLAGS = $(DEFINES)

#SOMAJOR = 3
#SOMINOR = 1
#SOMICRO = 1
#SOVER = `expr $(SOMAJOR) + $(SOMINOR)`:$(SOMICRO):$(SOMINOR)

noinst_LTLIBRARIES = libftp.la
#lib_LTLIBRARIES = libftp.la

libftp_la_SOURCES = \
	ftplib.h \
	ftplib.c

#libftp_la_LDFLAGS = -L. -version-info $(SOVER)
