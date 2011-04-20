#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# Minimum version of autoconf required
AC_PREREQ(2.62)

# UPDATING VERSION NUMBERS FOR RELEASES
#
# libalpm:
# current
#   The most recent interface number that this library implements.
# revision
#   The implementation number of the current interface.
# age
#   The difference between the newest and oldest interfaces that this library
#   implements. In other words, the library implements all the interface
#   numbers in the range from number current - age to current.
#
# 1. Start with version information of `0:0:0' for each libtool library.
# 2. Update the version information only immediately before a public release of
#    your software. More frequent updates are unnecessary, and only guarantee
#    that the current interface number gets larger faster.
# 3. If the library source code has changed at all since the last update, then
#    increment revision (`c:r:a' becomes `c:r+1:a').
# 4. If any interfaces have been added, removed, or changed since the last
#    update, increment current, and set revision to 0.
# 5. If any interfaces have been added since the last public release, then
#    increment age.
# 6. If any interfaces have been removed since the last public release, then
#    set age to 0.
#
# pacman:
#   Extreme huge major changes:
#     pacman_version_major += 1
#     pacman_version_minor = 0
#     pacman_version_micro = 0
#
#   Real releases:
#     pacman_version_minor += 1
#     pacman_version_micro = 0
#
#   Bugfix releases:
#     pacman_version_micro += 1

m4_define([lib_current], [6])
m4_define([lib_revision], [2])
m4_define([lib_age], [0])

m4_define([pacman_version_major], [3])
m4_define([pacman_version_minor], [5])
m4_define([pacman_version_micro], [2])
m4_define([pacman_version],
          [pacman_version_major.pacman_version_minor.pacman_version_micro])

# Autoconf initialization
AC_INIT([pacman], [pacman_version], [pacman-dev@archlinux.org])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_HOST
AM_INIT_AUTOMAKE

LIB_VERSION=`expr lib_current - lib_age`.lib_age.lib_revision
LIB_VERSION_INFO="lib_current:lib_revision:lib_age"

# Set subsitution values for version stuff in Makefiles and anywhere else,
# and put LIB_VERSION in config.h
AC_SUBST(LIB_VERSION)
AC_SUBST(LIB_VERSION_INFO)
AC_DEFINE_UNQUOTED([LIB_VERSION], ["$LIB_VERSION"], [libalpm version number])

# Help line for root directory
AC_ARG_WITH(root-dir,
	AS_HELP_STRING([--with-root-dir=path], [set the location of the root operating directory]),
	[ROOTDIR=$withval], [ROOTDIR=/])

# Help line for package extension
AC_ARG_WITH(pkg-ext,
	AS_HELP_STRING([--with-pkg-ext=ext], [set the file extension used by packages]),
	[PKGEXT=$withval], [PKGEXT=.pkg.tar.gz])

# Help line for source package directory
AC_ARG_WITH(src-ext,
	AS_HELP_STRING([--with-src-ext=ext], [set the file extension used by source packages]),
	[SRCEXT=$withval], [SRCEXT=.src.tar.gz])

# Help line for buildscript filename
AC_ARG_WITH(buildscript,
	AS_HELP_STRING([--with-buildscript=name], [set the build script name used by makepkg]),
	[BUILDSCRIPT=$withval], [BUILDSCRIPT=PKGBUILD])

# Help line for using OpenSSL
AC_ARG_WITH(openssl,
	AS_HELP_STRING([--with-openssl], [use OpenSSL crypto implementations instead of internal routines]),
	[], [with_openssl=check])

# Check for useable libcurl
LIBCURL_CHECK_CONFIG([yes], [7.19.4])

# Help line for documentation
AC_ARG_ENABLE(doc,
	AS_HELP_STRING([--disable-doc], [prevent make from looking at doc/ dir]),
	[wantdoc=$enableval], [wantdoc=yes])

# Help line for doxygen
AC_ARG_ENABLE(doxygen,
	AS_HELP_STRING([--enable-doxygen], [build your own API docs via Doxygen]),
	[wantdoxygen=$enableval], [wantdoxygen=no])

# Help line for debug
AC_ARG_ENABLE(debug,
	AS_HELP_STRING([--enable-debug], [enable debugging support]),
	[debug=$enableval], [debug=no])

# Help line for using git version in pacman version string
AC_ARG_ENABLE(git-version,
	AS_HELP_STRING([--enable-git-version],
		[enable use of git version in version string if available]),
	[wantgitver=$enableval], [wantgitver=no])

# Checks for programs.
AC_PROG_AWK
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_CHECK_PROGS([PYTHON], [python2.7 python2.6 python2.5 python2 python], [false])
AC_PATH_PROGS([BASH_SHELL], [bash bash4 bash3], [false])

# find installed gettext
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION(0.13.1)

AC_CHECK_LIB([m], [fabs], ,
	AC_MSG_ERROR([libm is needed to compile pacman!]))

# Check for libarchive
AC_CHECK_LIB([archive], [archive_read_data], ,
	AC_MSG_ERROR([libarchive is needed to compile pacman!]))

# Check for OpenSSL
AC_MSG_CHECKING(whether to link with libssl)
AS_IF([test "x$with_openssl" != "xno"],
	[AC_MSG_RESULT(yes)
	AC_CHECK_LIB([ssl], [MD5_Final], ,
	[if test "x$with_openssl" != "xcheck"; then
		AC_MSG_FAILURE([--with-openssl was given, but -lssl was not found])
	fi],
	[-lcrypto])],
	AC_MSG_RESULT(no))
AM_CONDITIONAL([HAVE_LIBSSL], [test "x$ac_cv_lib_ssl_MD5_Final" = "xyes"])

# Check for gpgme
AC_CHECK_LIB([gpgme], [gpgme_check_version], ,
	AC_MSG_ERROR([gpgme is needed to compile pacman!]))

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h glob.h libintl.h locale.h mntent.h string.h \
                  sys/ioctl.h sys/mount.h sys/param.h sys/statvfs.h \
                  sys/time.h sys/types.h sys/ucred.h syslog.h termios.h \
                  wchar.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_STRUCT_TM
AC_TYPE_UID_T
AC_STRUCT_DIRENT_D_TYPE
PATH_MAX_DEFINED

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_GETMNTENT
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MKTIME
AC_CHECK_FUNCS([geteuid getmntinfo realpath regcomp strcasecmp \
                strndup strrchr strsep swprintf tcflush \
                wcwidth uname])
# For the diskspace code
FS_STATS_TYPE
AC_CHECK_MEMBERS([struct statvfs.f_flag],,,[[#include <sys/statvfs.h>]])
AC_CHECK_MEMBERS([struct statfs.f_flags],,,[[#include <sys/param.h>
                  #include <sys/mount.h>]])

# Enable large file support if available
AC_SYS_LARGEFILE

# Check if we can use symbol visibility support in GCC
GCC_VISIBILITY_CC
# Check if we have -fgnu89-inline flag
GCC_GNU89_INLINE_CC

# Host-dependant definitions
SIZECMD="stat -L -c %s"
SEDINPLACE="sed -i"
STRIP_BINARIES="--strip-all"
STRIP_SHARED="--strip-unneeded"
STRIP_STATIC="--strip-debug"
case "${host_os}" in
	*bsd*)
		SIZECMD="stat -L -f %z"
		SEDINPLACE="sed -i ''"
		;;
	cygwin*)
		host_os_cygwin=yes
		CFLAGS="$CFLAGS -DCYGWIN"
		;;
	darwin*)
		host_os_darwin=yes
		SIZECMD="/usr/bin/stat -L -f %z"
		SEDINPLACE="/usr/bin/sed -i ''"
		STRIP_BINARIES=""
		STRIP_SHARED="-S"
		STRIP_STATIC="-S"
		;;
esac

AM_CONDITIONAL([CYGWIN], test "x$host_os_cygwin" = "xyes")
AM_CONDITIONAL([DARWIN], test "x$host_os_darwin" = "xyes")
AC_PATH_PROGS([DUPATH], [du], [du], [/usr/bin$PATH_SEPARATOR/bin] )
AC_SUBST(SIZECMD)
AC_SUBST(SEDINPLACE)
AC_SUBST(STRIP_BINARIES)
AC_SUBST(STRIP_SHARED)
AC_SUBST(STRIP_STATIC)

# Check for architecture, used in default makepkg.conf
# (Note single space left after CARCHFLAGS)
case "${host}" in
	i686-*)
		CARCH="i686"
		CARCHFLAGS="-march=i686 "
		;;
	x86_64-*)
		CARCH="x86_64"
		CARCHFLAGS="-march=x86-64 "
		;;
	ia64-*)
		CARCH="ia64"
		CARCHFLAGS="-march=ia64 "
		;;
	sparc-*)
		CARCH="sparc"
		CARCHFLAGS="-mcpu=v9 "
		;;
	ppc-* | powerpc-*)
		CARCH="ppc"
		CARCHFLAGS="-mcpu=750 "
		;;
	i386-*)
		CARCH="i386"
		CARCHFLAGS="-march=i386 "
		;;
	arm-*)
		CARCH="arm"
		CARCHFLAGS="-march=armv4 "
		;;
	*)
		AC_MSG_WARN([[Your architecture is unknown for makepkg.conf, consider adding it to configure.ac]])
		CARCH="unknown"
		CARCHFLAGS=""
		;;
esac

# Now do some things common to all architectures
CHOST="${host}"
AC_SUBST(CARCH)
AC_SUBST(CARCHFLAGS)
AC_SUBST(CHOST)

# Check for documentation support and status
AC_CHECK_PROGS([ASCIIDOC], [asciidoc])
AC_MSG_CHECKING([for building documentation])
if test "x$wantdoc" = "xyes" ; then
	if test $ASCIIDOC ; then
		AC_MSG_RESULT([yes, enabled by configure])
	else
		asciidoc="(warning : asciidoc not installed)"
		AC_MSG_RESULT([yes $asciidoc])
	fi
	wantdoc=yes
else
	AC_MSG_RESULT([no, disabled by configure])
	wantdoc=no
fi
AM_CONDITIONAL(WANT_DOC, test "x$wantdoc" = "xyes")

# Check for doxygen support and status
AC_CHECK_PROGS([DOXYGEN], [doxygen])
AC_MSG_CHECKING([for doxygen])
if test "x$wantdoxygen" = "xyes" ; then
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
AM_CONDITIONAL(USE_DOXYGEN, test "x$usedoxygen" = "xyes")

# Enable or disable debug code
AC_MSG_CHECKING(for debug mode request)
if test "x$debug" = "xyes" ; then
	AC_MSG_RESULT(yes)
	AC_DEFINE([PACMAN_DEBUG], , [Enable debug code])
	# Check for mcheck
	AC_CHECK_HEADERS([mcheck.h])
	# Check for -fstack-protector availability
	GCC_STACK_PROTECT_LIB
	GCC_STACK_PROTECT_CC
	GCC_FORTIFY_SOURCE_CC
	CFLAGS="$CFLAGS -g -Wall -Werror"
else
	AC_MSG_RESULT(no)
	CFLAGS="$CFLAGS -Wall"
fi

# Enable or disable use of git version in pacman version string
AC_MSG_CHECKING(whether to use git version if available)
if test "x$wantgitver" = "xyes" ; then
	AC_CHECK_PROGS([GIT], [git])
	AC_CHECK_FILE([.git/], hasgitdir=yes)
	if test $GIT -a "x$hasgitdir" = "xyes"; then
		AC_MSG_RESULT([yes])
		usegitver=yes
		AC_DEFINE([USE_GIT_VERSION], , [Use GIT version in version string])
	else
		AC_MSG_RESULT([no, git or .git dir missing])
		usegitver=no
	fi
else
	AC_MSG_RESULT([no, disabled by configure])
	usegitver=no
fi
AM_CONDITIONAL(USE_GIT_VERSION, test "x$usegitver" = "xyes")

# Set root directory
AC_SUBST(ROOTDIR)
# Set package file extension
AC_SUBST(PKGEXT)
AC_DEFINE_UNQUOTED([PKGEXT], "$PKGEXT", [The file extension used by pacman packages])
# Set source package file extension
AC_SUBST(SRCEXT)
AC_DEFINE_UNQUOTED([SRCEXT], "$SRCEXT", [The file extension used by pacman source packages])
# Set makepkg build script name
AC_SUBST(BUILDSCRIPT)
AC_DEFINE_UNQUOTED([BUILDSCRIPT], "$BUILDSCRIPT", [The build script name used by makepkg])

# Configuration files
AC_CONFIG_FILES([
lib/libalpm/Makefile
lib/libalpm/po/Makefile.in
src/pacman/Makefile
src/util/Makefile
scripts/Makefile
doc/Makefile
etc/Makefile
po/Makefile.in
test/pacman/Makefile
test/pacman/tests/Makefile
test/util/Makefile
contrib/Makefile
Makefile
])
AC_OUTPUT

echo "
${PACKAGE_NAME}:

  Build information:
    source code location   : ${srcdir}
    prefix                 : ${prefix}
    sysconfdir             : $(eval echo ${sysconfdir})
       conf file           : $(eval echo ${sysconfdir})/pacman.conf
    localstatedir          : $(eval echo ${localstatedir})
       database dir        : $(eval echo ${localstatedir})/lib/pacman/
       cache dir           : $(eval echo ${localstatedir})/cache/pacman/pkg/
    compiler               : ${CC}
    compiler flags         : ${CFLAGS}
    defines                : ${DEFS}

    Architecture           : ${CARCH}
    Architecture flags     : ${CARCHFLAGS}
    Host Type              : ${CHOST}
    Filesize command       : ${SIZECMD}
    In-place sed command   : ${SEDINPLACE}

    libalpm version        : ${LIB_VERSION}
    libalpm version info   : ${LIB_VERSION_INFO}
    pacman version         : ${PACKAGE_VERSION}
    using git version      : ${usegitver}

  Directory and file information:
    root working directory : ${ROOTDIR}
    package extension      : ${PKGEXT}
    source pkg extension   : ${SRCEXT}
    build script name      : ${BUILDSCRIPT}

  Compilation options:
    Run make in doc/ dir   : ${wantdoc} ${asciidoc}
    Doxygen support        : ${usedoxygen}
    debug support          : ${debug}
"

# vim:set ts=2 sw=2 noet:
