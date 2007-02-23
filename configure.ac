ENV_CFLAGS=$CFLAGS
CFLAGS=""

AC_PREREQ(2.59)
dnl Update it right before the release since $pkgver_foo are all _post_ release snapshots
AC_INIT([Pacman package manager], 3.0.0, [pacman-dev@archlinux.org], [pacman])
AC_LANG([C])
AC_CONFIG_HEADERS(config.h)
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION(0.13.1)

dnl Define the libalpm version number here
LIB_MAJOR_VERSION=1
LIB_MINOR_VERSION=0
LIB_MICRO_VERSION=0
LIB_VERSION=$LIB_MAJOR_VERSION.$LIB_MINOR_VERSION.$LIB_MICRO_VERSION
dnl Needed for libtool to create proper shared lib version.
dnl This is not completely correct- see 
dnl http://sourceware.org/autobook/autobook/autobook_91.html for details.
LIB_VERSION_INFO=`expr $LIB_MAJOR_VERSION + $LIB_MINOR_VERSION`:$LIB_MICRO_VERSION:$LIB_MINOR_VERSION

dnl Set subsitution values for version stuff in Makefiles and anywhere else
AC_SUBST(LIB_VERSION)
AC_SUBST(LIB_VERSION_INFO)

dnl Put version number in config.h
AC_DEFINE_UNQUOTED([LIB_VERSION], ["$LIB_VERSION"], [libalpm version number])

dnl Configuration files
dnl AC_CONFIG_FILES([etc/makepkg.conf] [etc/pacman.conf])

AC_PROG_CC
AC_HEADER_STDC
AC_PROG_INSTALL
AC_CHECK_FUNCS([strverscmp])
AM_PROG_LIBTOOL
AM_CONDITIONAL(LINKSTATIC, test "$enable_static" = "yes")

dnl Help line for man2html
AC_ARG_ENABLE(man2html,
    AC_HELP_STRING([--enable-man2html], [Build html docs via man2html]),
    [wantman2html=$enableval], [wantman2html=no])

dnl Help line for doxygen
AC_ARG_ENABLE(doxygen,
    AC_HELP_STRING([--disable-doxygen], [Build API docs via Doxygen]),
    [wantdoxygen=$enableval], [wantdoxygen=yes])

dnl Help line for config file
AC_ARG_WITH(config-file,
    AC_HELP_STRING([--with-config-file=path], [Set the location of pacman's config file]),
    [configfile=$withval], [configfile=/etc/pacman.conf])

dnl Help line for debug
AC_ARG_ENABLE(debug,
    AC_HELP_STRING([--enable-debug], [Enable debugging support]),
    [debug=$enableval], [debug=no])

dnl Help line for fakeroot
AC_ARG_ENABLE(fakeroot,
    AC_HELP_STRING([--disable-fakeroot], [Disable fakeroot proof support]),
    [fakeroot=$enableval], [fakeroot=yes])

dnl Host-dependant flags
case "${host}" in
	*-*-cygwin*)
		ENV_CFLAGS="$ENV_CFLAGS -DCYGWIN"
		;;
esac

dnl Check for architecture
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
		AC_MSG_ERROR([Your architecture is not supported])
		;;
esac

dnl Now do some things common to all architectures
CHOST="${host}"
AC_SUBST(CARCH)
AC_SUBST(CARCHFLAGS)
AC_SUBST(ARCHSWITCH)
AC_SUBST(CHOST)

dnl Humor lowers blood pressure
AC_MSG_CHECKING(your blood pressure)
AC_MSG_RESULT([a bit high, but we can proceed])

dnl Check for man2html binary
AC_MSG_CHECKING(for man2html)
if test "x$wantman2html" = "xyes" ; then
    AC_CHECK_PROGS([MAN2HTML], [man2html])
    if test $MAN2HTML ; then
		AC_MSG_RESULT(yes)
		useman2html=yes
    else
		AC_MSG_RESULT(no, man2html missing)
		useman2html=no
    fi
else
    AC_MSG_RESULT(no, disabled by configure)
	useman2html=no
fi
AM_CONDITIONAL(HAS_MAN2HTML, test "x$useman2html" = "xyes")

dnl Check for doxygen support
AC_MSG_CHECKING(for doxygen)
if test "x$wantdoxygen" = "xyes" ; then
    AC_CHECK_PROGS([DOXYGEN], [doxygen])
    if test $DOXYGEN ; then
		AC_MSG_RESULT(yes)
		usedoxygen=yes
    else
		AC_MSG_RESULT(no, doxygen missing)
		usedoxygen=no
    fi
else
    AC_MSG_RESULT(no, disabled by configure)
	usedoxygen=no
fi
AM_CONDITIONAL(HAS_DOXYGEN, test "x$usedoxygen" = "xyes")

dnl Check for math
AC_CHECK_LIB([m], [sqrt], [AC_CHECK_HEADER([math.h], [LIBM='-lm'])])
if test -z "$LIBM"; then
    AC_MSG_ERROR("math library needed to compile pacman!");
fi

dnl Check for libarchive
AC_CHECK_LIB([archive], [archive_read_data], [AC_CHECK_HEADER([archive.h], [LIBARCHIVE='-larchive'])])
if test -z "$LIBARCHIVE"; then
    AC_MSG_ERROR("libarchive is needed to compile pacman!");
fi

dnl Check for libdownload
AC_CHECK_LIB([download], [downloadParseURL], [AC_CHECK_HEADER([download.h], [LIBDOWNLOAD='-ldownload'])])
if test -z "$LIBDOWNLOAD"; then
    AC_MSG_ERROR("libdownload is needed to compile pacman!");
fi

dnl Set config location
AC_MSG_CHECKING(for configuration file name)
if test -n "$configfile"; then
	AC_DEFINE_UNQUOTED([PACCONF], "$configfile", [Location of pacman conf file])
    AC_MSG_RESULT(["$configfile"])
else
    AC_MSG_ERROR(["pacman config file (--with-config-file is not set"])
fi

dnl Enable or disable debug code
AC_MSG_CHECKING(for debug mode request)
if test "x$debug" = "xyes" ; then
	AC_DEFINE([PACMAN_DEBUG], , [Enable debug code])
	CFLAGS="$CFLAGS -g -Wall -Werror -std=c99"
	LDFLAGS="$LDFLAGS -lmcheck"
    AC_MSG_RESULT(yes)
else
	CFLAGS="$CFLAGS -Wall -std=c99"
    AC_MSG_RESULT(no)
fi

dnl Enable or disable fakeroot code
AC_MSG_CHECKING(for fakeroot proof support)
if test "x$fakeroot" = "xyes" ; then
	AC_MSG_RESULT(yes)
else
	AC_DEFINE([FAKEROOT], , [Disable fakeroot-proof code])
	AC_MSG_RESULT(no)
fi

dnl Set CFLAGS to both $CFLAGS and ones from env ($ENV_CFLAGS)
CFLAGS="$CFLAGS $ENV_CFLAGS"

dnl ==========================================================================

AC_OUTPUT([
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

echo "
pacman-$VERSION:

    prefix                 : ${prefix}
    source code location   : ${srcdir}
    compiler               : ${CC}
    compiler flags         : ${CFLAGS}
    defines                : ${DEFS}

    Architecture           : ${CARCH}
    Architecture flags     : -${ARCHSWITCH}=${CARCHFLAGS}
    Host Type              : ${CHOST}

    libalpm version        : ${LIB_VERSION}
    pacman version         : ${PACKAGE_VERSION}
    pacman.conf location   : ${configfile}

    Doxygen support        : ${usedoxygen}
    man2html support       : ${useman2html}
    debug support          : ${debug}
    fakeroot-proof support : ${fakeroot}
"
