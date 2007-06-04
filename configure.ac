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
	AC_HELP_STRING([--with-root-dir=path], [set the location of pacman's root operating directory]),
	[ROOTDIR=$withval], [ROOTDIR=/])

# Help line for package extension
AC_ARG_WITH(pkg-ext,
	AC_HELP_STRING([--with-pkg-ext=ext], [set the file extension used by packages]),
	[PKGEXT=$withval], [PKGEXT=.pkg.tar.gz])

# Help line for source package directory
AC_ARG_WITH(src-ext,
	AC_HELP_STRING([--with-src-ext=ext], [set the file extension used by source packages]),
	[SRCEXT=$withval], [SRCEXT=.src.tar.gz])

# Help line for database extension
AC_ARG_WITH(db-ext,
	AC_HELP_STRING([--with-db-ext=ext], [set the file extension used by the database]),
	[DBEXT=$withval], [DBEXT=.db.tar.gz])

# Help line for doxygen
AC_ARG_ENABLE(doxygen,
	AC_HELP_STRING([--disable-doxygen], [do not build API docs via Doxygen]),
	[wantdoxygen=$enableval], [wantdoxygen=yes])

# Help line for debug
AC_ARG_ENABLE(debug,
	AC_HELP_STRING([--enable-debug], [enable debugging support]),
	[debug=$enableval], [debug=no])

# Help line for abs
AC_ARG_ENABLE(abs,
	AC_HELP_STRING([--disable-abs], [do not include Arch Linux Build System script]),
	[includeabs=$enableval], [includeabs=yes])

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

# Enable or disable inclusion of abs script
AC_MSG_CHECKING(for inclusion of abs script)
if test "x$includeabs" = "xyes" ; then
	AC_DEFINE([INCLUDE_ABS], , [Include abs script])
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi
AM_CONDITIONAL(INCLUDE_ABS, test "x$includeabs" = "xyes")

# Set root directory
AC_SUBST(ROOTDIR)
# Set package file extension
AC_SUBST(PKGEXT)
AC_DEFINE_UNQUOTED([PKGEXT], "$PKGEXT", [The file extension used by pacman packages])
# Set source package file extension
AC_SUBST(SRCEXT)
AC_DEFINE_UNQUOTED([SRCEXT], "$SRCEXT", [The file extension used by pacman source packages])
# Set database file extension
AC_SUBST(DBEXT)
AC_DEFINE_UNQUOTED([DBEXT], "$DBEXT", [The file extension used by pacman databases])

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
etc/pacman.d/Makefile
etc/abs/Makefile
pactest/Makefile
contrib/Makefile
Makefile
])
AC_OUTPUT

echo "
$PACKAGE_STRING:

  Build information:
    source code location   : ${srcdir}
    prefix                 : ${prefix}
    sysconfdir             : $(eval echo ${sysconfdir})
       conf file           : $(eval echo ${sysconfdir})/pacman.conf
    localstatedir          : $(eval echo ${localstatedir})
       lock file           : $(eval echo ${localstatedir})/run/pacman.lck
       database dir        : $(eval echo ${localstatedir})/lib/pacman/
       cache dir           : $(eval echo ${localstatedir})/cache/pacman/pkg/
    compiler               : ${CC}
    compiler flags         : ${CFLAGS}
    defines                : ${DEFS}

    Architecture           : ${CARCH}
    Architecture flags     : -${ARCHSWITCH}=${CARCHFLAGS}
    Host Type              : ${CHOST}

    libalpm version        : ${LIB_VERSION}
    pacman version         : ${PACKAGE_VERSION}

  Directory and file information:
    root working directory : ${ROOTDIR}
    package extension      : ${PKGEXT}
    source pkg extension   : ${SRCEXT}
    database extension     : ${DBEXT}

  Compilation options:
    Doxygen support        : ${usedoxygen}
    debug support          : ${debug}
    include abs            : ${includeabs}
"

# vim:set ts=2 sw=2 noet:
