#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# Minimum version of autoconf required
AC_PREREQ(2.61)

# Autoconf initialization
# AC_INIT(FULL-PACKAGE-NAME, VERSION, BUG-REPORT-ADDRESS)
AC_INIT([Pacman Package Manager], 3.1.0-dev, [pacman-dev@archlinux.org], [pacman])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_HOST
AM_INIT_AUTOMAKE

# Define the libalpm version number here
LIB_MAJOR_VERSION=1
LIB_MINOR_VERSION=1
LIB_MICRO_VERSION=1
LIB_VERSION=$LIB_MAJOR_VERSION.$LIB_MINOR_VERSION.$LIB_MICRO_VERSION
# Needed for libtool to create proper shared lib version.
# This is not completely correct- see
# http://sourceware.org/autobook/autobook/autobook_91.html for details.
LIB_VERSION_INFO=`expr $LIB_MAJOR_VERSION + $LIB_MINOR_VERSION`:$LIB_MICRO_VERSION:$LIB_MINOR_VERSION

# Set subsitution values for version stuff in Makefiles and anywhere else,
# and put LIB_VERSION in config.h
AC_SUBST(LIB_VERSION)
AC_SUBST(LIB_VERSION_INFO)
AC_DEFINE_UNQUOTED([LIB_VERSION], ["$LIB_VERSION"], [libalpm version number])

# Help line for root directory
AC_ARG_WITH(root-dir,
    AC_HELP_STRING([--with-root-dir=path], [Set the location of pacman's root operating directory]),
    [ROOTDIR=$withval], [ROOTDIR=/])

# Help line for database path
AC_ARG_WITH(db-path,
    AC_HELP_STRING([--with-db-path=path], [Set the location of pacman's database]),
    [DBPATH=$withval], [DBPATH=var/lib/pacman/])

# Help line for cache directory
AC_ARG_WITH(cache-dir,
    AC_HELP_STRING([--with-cache-dir=path], [Set the location of pacman's cache directory]),
    [CACHEDIR=$withval], [CACHEDIR=var/cache/pacman/pkg/])

# Help line for lock file
AC_ARG_WITH(lock-file,
    AC_HELP_STRING([--with-lock-file=path], [Set the location of pacman's lock file]),
    [LOCKFILE=$withval], [LOCKFILE=var/run/pacman.lck])

# Help line for config file
AC_ARG_WITH(config-file,
    AC_HELP_STRING([--with-config-file=path], [Set the location of pacman's config file]),
    [CONFIGFILE=$withval], [CONFIGFILE=etc/pacman.conf])

# Help line for package extension
AC_ARG_WITH(pkg-ext,
			AC_HELP_STRING([--with-pkg-ext=ext], [Set the file extension used by packages]),
    [PKGEXT=$withval], [PKGEXT=.pkg.tar.gz])

# Help line for database extension
AC_ARG_WITH(db-ext,
			AC_HELP_STRING([--with-db-ext=ext], [Set the file extension used by the database]),
    [DBEXT=$withval], [DBEXT=.db.tar.gz])

# Help line for doxygen
AC_ARG_ENABLE(doxygen,
    AC_HELP_STRING([--disable-doxygen], [Build API docs via Doxygen]),
    [wantdoxygen=$enableval], [wantdoxygen=yes])

# Help line for debug
AC_ARG_ENABLE(debug,
    AC_HELP_STRING([--enable-debug], [Enable debugging support]),
    [debug=$enableval], [debug=no])

# Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AM_PROG_LIBTOOL

# find installed gettext
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION(0.13.1)

# Check for math
AC_CHECK_LIB([m], [sqrt], , AC_MSG_ERROR([math library is needed to compile pacman!]))

# Check for libarchive
AC_CHECK_LIB([archive], [archive_read_data], , AC_MSG_ERROR([libarchive is needed to compile pacman!]))

# Check for libdownload
AC_CHECK_LIB([download], [downloadParseURL], , AC_MSG_ERROR([libdownload is needed to compile pacman!]))

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([fcntl.h libintl.h limits.h locale.h mntent.h stdlib.h string.h strings.h sys/ioctl.h sys/statvfs.h sys/time.h syslog.h unistd.h wchar.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM
AC_TYPE_UID_T

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_FORK
AC_FUNC_GETMNTENT
AC_PROG_GCC_TRADITIONAL
AC_FUNC_LSTAT
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_REALLOC
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([getcwd getmntent gettimeofday memmove memset mkdir realpath regcomp rmdir setenv setlocale sqrt strcasecmp strchr strdup strerror strndup strrchr strstr strverscmp uname])

# Host-dependant flags
case "${host}" in
	*-*-cygwin*)
		CFLAGS="$CFLAGS -DCYGWIN"
		;;
esac

# Check for architecture
case "${host}" in
	i686-*)
		CARCH="i686"
		CARCHFLAGS="i686"
		ARCHSWITCH="march"
		;;
	x86_64-*)
		CARCH="x86_64"
		CARCHFLAGS="x86-64"
		ARCHSWITCH="march"
		;;
	ia64-*)
		CARCH="ia64"
		CARCHFLAGS="ia64"
		ARCHSWITCH="march"
		;;
	sparc-*)
		CARCH="sparc"
		CARCHFLAGS="v9"
		ARCHSWITCH="mcpu"
		;;
	ppc-* | powerpc-*)
		CARCH="ppc"
		CARCHFLAGS="750"
		ARCHSWITCH="mcpu"
		;;
	i386-*)
		CARCH="i386"
		CARCHFLAGS="i386"
		ARCHSWITCH="march"
		;;
	*)
		AC_MSG_ERROR([[Your architecture is not supported; consider adding it to configure.ac]])
		;;
esac

# Now do some things common to all architectures
CHOST="${host}"
AC_SUBST(CARCH)
AC_SUBST(CARCHFLAGS)
AC_SUBST(ARCHSWITCH)
AC_SUBST(CHOST)

# Check for doxygen support
AC_MSG_CHECKING([for doxygen])
if test "x$wantdoxygen" = "xyes" ; then
    AC_CHECK_PROGS([DOXYGEN], [doxygen])
    if test $DOXYGEN ; then
		AC_MSG_RESULT([yes])
		usedoxygen=yes
    else
		AC_MSG_RESULT([no, doxygen missing])
		usedoxygen=no
    fi
else
    AC_MSG_RESULT([no, disabled by configure])
	usedoxygen=no
fi
AM_CONDITIONAL(HAS_DOXYGEN, test "x$usedoxygen" = "xyes")

# Enable or disable debug code
AC_MSG_CHECKING(for debug mode request)
if test "x$debug" = "xyes" ; then
	AC_DEFINE([PACMAN_DEBUG], , [Enable debug code])
	CFLAGS="$CFLAGS -g -Wall -Werror -fstack-protector -std=c99"
	LDFLAGS="$LDFLAGS -lmcheck"
    AC_MSG_RESULT(yes)
else
	CFLAGS="$CFLAGS -Wall -std=c99"
    AC_MSG_RESULT(no)
fi

# Set root directory
AC_DEFINE_UNQUOTED([PM_ROOT], "$ROOTDIR", [Location of pacman's default root directory])
AC_SUBST(ROOTDIR)

# Set database path
AC_DEFINE_UNQUOTED([PM_DBPATH], "$DBPATH", [Location of pacman database])
AC_SUBST(DBPATH)

# Set cache directory
AC_DEFINE_UNQUOTED([PM_CACHEDIR], "$CACHEDIR", [Location of pacman's package cache])
AC_SUBST(CACHEDIR)

# Set lock file location
AC_DEFINE_UNQUOTED([PM_LOCK], "$LOCKFILE", [Location of pacman lock file])
AC_SUBST(LOCKFILE)

# Set configuration file location
AC_DEFINE_UNQUOTED([PM_CONF], "$CONFIGFILE", [Location of pacman configuration file])
AC_SUBST(CONFIGFILE)

# Set package file extension
AC_DEFINE_UNQUOTED([PM_EXT_PKG], "$PKGEXT", [The file extension used by pacman packages])
AC_SUBST(PKGEXT)

# Set database file extension
AC_DEFINE_UNQUOTED([PM_EXT_DB], "$DBEXT", [The file extension used by pacman databases])
AC_SUBST(DBEXT)

# Configuration files
AC_CONFIG_FILES([
lib/libalpm/Makefile
lib/libalpm/po/Makefile.in
src/pacman/Makefile
src/pacman/po/Makefile.in
src/util/Makefile
scripts/Makefile
doc/Makefile
etc/Makefile
etc/makepkg.conf
etc/pacman.conf
etc/pacman.d/Makefile
etc/pacman.d/mirrorlist
etc/abs/Makefile
pactest/Makefile
contrib/Makefile
Makefile
])
AC_OUTPUT

echo "
$PACKAGE_STRING:

    prefix                 : ${prefix}
    sysconfdir             : $(eval echo ${sysconfdir})
    source code location   : ${srcdir}
    compiler               : ${CC}
    compiler flags         : ${CFLAGS}
    defines                : ${DEFS}

    Architecture           : ${CARCH}
    Architecture flags     : -${ARCHSWITCH}=${CARCHFLAGS}
    Host Type              : ${CHOST}

    libalpm version        : ${LIB_VERSION}
    pacman version         : ${PACKAGE_VERSION}

  Directory and file information:
    root directory         : ${ROOTDIR}
    database path          : ${ROOTDIR}${DBPATH}
    cache directory        : ${ROOTDIR}${CACHEDIR}
    lock file location     : ${ROOTDIR}${LOCKFILE}
    conf file location     : ${ROOTDIR}${CONFIGFILE}
    package extension      : ${PKGEXT}
    database extension     : ${DBEXT}

  Compilation options:
    Doxygen support        : ${usedoxygen}
    debug support          : ${debug}
"
