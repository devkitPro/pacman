ENV_CFLAGS=$CFLAGS

AC_PREREQ(2.59)
dnl Update it right before the release since $pkgver_foo are all _post_ release snapshots
AC_INIT([Pacman package manager], 3.4.0, [pacman-dev@archlinux.org], pacman)
AC_LANG(C)
AM_CONFIG_HEADER(config.h)
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION(0.13.1)

dnl Host dependant flags
case "${host}" in
	*-*-cygwin*)
		ENV_CFLAGS="$ENV_CFLAGS -DCYGWIN"
		;;
esac

dnl Define here the libalpm version number
PM_MAJOR_VERSION=0
PM_MINOR_VERSION=1
PM_MICRO_VERSION=0
PM_VERSION=$PM_MAJOR_VERSION.$PM_MINOR_VERSION.$PM_MICRO_VERSION
dnl Needed for libtool to create proper shared lib version
PM_VERSION_INFO=`expr $PM_MAJOR_VERSION + $PM_MINOR_VERSION`:$PM_MICRO_VERSION:$PM_MINOR_VERSION

AC_SUBST(PM_MAJOR_VERSION)
AC_SUBST(PM_MINOR_VERSION)
AC_SUBST(PM_MICRO_VERSION)
AC_SUBST(PM_VERSION)
AC_SUBST(PM_VERSION_INFO)

dnl Define the default config file path for pacman.conf
PACCONF=/etc/pacman.conf
AC_DEFINE_UNQUOTED([PACCONF], ["$PACCONF"], [pacman.conf location])

dnl Put out version numbers to config.h
AC_DEFINE_UNQUOTED([PM_VERSION], ["$PM_VERSION"], [libalpm version number])

dnl Configuration files
AC_CONFIG_FILES([etc/makepkg.conf] [etc/pacman.conf])

AC_PROG_CC
AC_HEADER_STDC
AC_PROG_INSTALL
AC_CHECK_FUNCS([strverscmp])
AM_PROG_LIBTOOL

dnl Humor lowers blood pressure
AC_MSG_CHECKING(your blood pressure)
AC_MSG_RESULT([a bit high, but we can proceed])

dnl Help line for man2html
AC_ARG_ENABLE(man2html,
    AC_HELP_STRING([--enable-man2html], [Build html docs via man2html]),
    [wantman2html=$enableval], [wantman2html=no])

dnl Help line for po4a
AC_ARG_ENABLE(po4a,
    AC_HELP_STRING([--disable-po4a], [Build localized manpages via po4a]),
    [wantpo4a=$enableval], [wantpo4a=yes])

dnl Help line for doxygen
AC_ARG_ENABLE(doxygen,
    AC_HELP_STRING([--disable-doxygen], [Build API docs via Doxygen]),
    [wantdoxygen=$enableval], [wantdoxygen=yes])

dnl Help line for python
AC_ARG_ENABLE(python,
    AC_HELP_STRING([--disable-python], [Build Python bindings]),
    [wantpython=$enableval], [wantpython=yes])

dnl Help line for perl
AC_ARG_ENABLE(perl,
    AC_HELP_STRING([--disable-perl], [Build Perl bindings]),
    [wantperl=$enableval], [wantperl=yes])

dnl Help line for java
AC_ARG_ENABLE(java,
    AC_HELP_STRING([--disable-java], [Build Java bindings]),
    [wantjava=$enableval], [wantjava=yes])

dnl Check for architecture
case "$host" in
	i686-*) AC_DEFINE_UNQUOTED([CARCH], ["i686"], [Architecture])
		AC_DEFINE_UNQUOTED([CARCHFLAGS], ["i686"], [Architecture])
		AC_DEFINE_UNQUOTED([MARCHFLAG], ["march"], [Architecture])
		CARCH="i686"
		CARCHFLAGS="i686"
		MARCHFLAG="march"
		CHOST="${host}"
		;;
	x86_64-*) AC_DEFINE_UNQUOTED([CARCH], ["x86_64"], [Architecture])
		AC_DEFINE_UNQUOTED([CARCHFLAGS], ["k8"], [Architecture])
		AC_DEFINE_UNQUOTED([MARCHFLAG], ["march"], [Architecture])
		CARCH="x86_64"
		CARCHFLAGS="k8"
		MARCHFLAG="march"
		CHOST="${host}"
		;;
	ia64-*) AC_DEFINE_UNQUOTED([CARCH], ["ia64"], [Architecture])
		AC_DEFINE_UNQUOTED([CARCHFLAGS], ["ia64"], [Architecture])
		AC_DEFINE_UNQUOTED([MARCHFLAG], ["march"], [Architecture])
		CARCH="ia64"
		CARCHFLAGS="ia64"
		MARCHFLAG="march"
		CHOST="${host}"
		;;
	sparc-*) AC_DEFINE_UNQUOTED([CARCH], ["sparc"], [Architecture])
		AC_DEFINE_UNQUOTED([CARCHFLAGS], ["v9"], [Architecture])
		AC_DEFINE_UNQUOTED([MARCHFLAG], ["mcpu"], [Architecture])
		CARCH="sparc"
		CARCHFLAGS="v9"
		MARCHFLAG="mcpu"
		CHOST="${host}"
		;;
	ppc-* | powerpc-*)
		AC_DEFINE_UNQUOTED([CARCH], ["ppc"], [Architecture])
		AC_DEFINE_UNQUOTED([CARCHFLAGS], ["750"], [Architecture])
		AC_DEFINE_UNQUOTED([MARCHFLAG], ["mcpu"], [Architecture])
		CARCH="ppc"
		CARCHFLAGS="750"
		MARCHFLAG="mcpu"
		CHOST="${host}"
		;;
	i386-*) AC_DEFINE_UNQUOTED([CARCH], ["i386"], [Architecture])
		AC_DEFINE_UNQUOTED([CARCHFLAGS], ["i386"], [Architecture])
		AC_DEFINE_UNQUOTED([MARCHFLAG], ["march"], [Architecture])
		CARCH="i386"
		CARCHFLAGS="i386"
		MARCHFLAG="march"
		CHOST="${host}"
		;;
	*)
		AC_MSG_ERROR([Your architecture is not supported])
		;;
esac

AC_SUBST(CARCH)
AC_SUBST(CARCHFLAGS)
AC_SUBST(MARCHFLAG)
AC_SUBST(CHOST)

dnl Help line for debug
AC_ARG_ENABLE(debug,
    AC_HELP_STRING([  --enable-debug], [Enable debugging support]),
    [debug=$enableval], [debug=yes])

dnl Help line for fakeroot
AC_ARG_ENABLE(fakeroot,
    AC_HELP_STRING([--disable-fakeroot], [Disable fakeroot proof support]),
    [fakeroot=$enableval], [fakeroot=yes])

dnl Check for man2html binary
AC_MSG_CHECKING(for support man2html)
if test x$wantman2html = xyes ; then
    AC_CHECK_PROGS([MAN2HTML], [man2html])
    AM_CONDITIONAL(HAS_MAN2HTML, test $MAN2HTML)
    if test $MAN2HTML ; then
        AC_DEFINE([HAS_MAN2HTML], [TRUE], [Enabled HTML generation from man via man2html])
	AC_MSG_RESULT(yes)
    else
        AC_DEFINE([HAS_MAN2HTML], [FALSE], [Disabled HTML generation from man via man2html])
	AC_MSG_RESULT(no, man2html missing)
    fi
else
    AM_CONDITIONAL(HAS_MAN2HTML, test $MAN2HTML)
    AC_DEFINE([HAS_MAN2HTML], [FALSE], [Not specified at configure line])
    AC_MSG_RESULT(not requested by configure)
fi

dnl Check for po4a binary
AC_MSG_CHECKING(for support po4a)
if test x$wantpo4a = xyes ; then
    AC_CHECK_PROGS([PO4A], [po4a])
    AM_CONDITIONAL(HAS_PO4A, test $PO4A)
    if test $PO4A ; then
        AC_DEFINE([HAS_PO4A], [TRUE], [Enabled PO4A (localization support for manpages)])
	AC_MSG_RESULT(yes)
	POSTATUS="yes, path: `which $PO4A`"
    else
        AC_DEFINE([HAS_PO4A], [FALSE], [Disabled PO4A (localization support for manpages)])
	AC_MSG_RESULT(no, po4a binary missing)
	POSTATUS="no, missing po4a binary"
    fi
else
    AM_CONDITIONAL(HAS_PO4A, test $PO4A)
    AC_DEFINE([HAS_PO4A], [FALSE], [Not specified at configure line])
    AC_MSG_RESULT(not requested by configure)
    POSTATUS="no, disabled by configure"
fi

dnl Check for doxygen support
AC_MSG_CHECKING(for support Doxygen)
if test x$wantdoxygen = xyes ; then
    AC_CHECK_PROGS([DOXYGEN], [doxygen])
    AM_CONDITIONAL(HAS_DOXYGEN, test $DOXYGEN)
    if test $DOXYGEN ; then
        AC_DEFINE([HAS_DOXYGEN], [TRUE], [Enabled Doxygen Support])
	AC_MSG_RESULT(yes)
	DOXYSTATUS="yes, path: `which $DOXYGEN`"
    else
        AC_DEFINE([HAS_DOXYGEN], [FALSE], [Disabled Doxygen support])
	AC_MSG_RESULT(no, doxygen missing)
	DOXYSTATUS="doxygen binary not found"
    fi
else
    AM_CONDITIONAL(HAS_DOXYGEN, test $DOXYGEN)
    AC_DEFINE([HAS_DOXYGEN], [FALSE], [Not specified at configure line])
    AC_MSG_RESULT(not requested by configure)
    DOXYSTATUS="no, disabled by configure"
fi

dnl Check for python support
AC_MSG_CHECKING(if Python bindings should be generated)
if test x$wantpython = xyes ; then
    AC_CHECK_PROGS([SWIG], [swig])
    AM_CONDITIONAL(HAS_PYTHON, test $SWIG)
    if test $SWIG ; then
        AC_DEFINE([HAS_PYTHON], [TRUE], [Enabled Python Support])
	AC_MSG_RESULT(yes)
	PYTHONSTATUS="yes, path: `which $SWIG`"
    else
        AC_DEFINE([HAS_PYTHON], [FALSE], [Disabled Python support])
	AC_MSG_RESULT(no, swig missing)
	PYTHONSTATUS="swig binary not found"
    fi
else
    AM_CONDITIONAL(HAS_PYTHON, test $PYTHON)
    AC_DEFINE([HAS_PYTHON], [FALSE], [Not specified at configure line])
    AC_MSG_RESULT(not requested by configure)
    PYTHONSTATUS="no, disabled by configure"
fi

dnl Check for perl support
AC_MSG_CHECKING(if Perl bindings should be generated)
if test x$wantperl = xyes ; then
    AC_CHECK_PROGS([SWIG], [swig])
    AM_CONDITIONAL(HAS_PERL, test $SWIG)
    if test $SWIG ; then
        AC_DEFINE([HAS_PERL], [TRUE], [Enabled Perl Support])
	AC_MSG_RESULT(yes)
	PERLSTATUS="yes, path: `which $SWIG`"
    else
        AC_DEFINE([HAS_PERL], [FALSE], [Disabled Perl support])
	AC_MSG_RESULT(no, swig missing)
	PERLSTATUS="swig binary not found"
    fi
else
    AM_CONDITIONAL(HAS_PERL, test $PERL)
    AC_DEFINE([HAS_PERL], [FALSE], [Not specified at configure line])
    AC_MSG_RESULT(not requested by configure)
    PERLSTATUS="no, disabled by configure"
fi

dnl Check for java support
AC_MSG_CHECKING(if Java bindings should be generated)
if test x$wantjava = xyes ; then
 	AC_CHECK_PROGS([SWIG], [swig])
 	AC_CHECK_PROGS([JAVAC], [javac])
 	AM_CONDITIONAL(HAS_JAVA, test $SWIG)
 	AM_CONDITIONAL(HAS_JAVA, test $JAVAC)
 	if test $JAVAC ; then
 		if test $SWIG ; then
 			AC_DEFINE([HAS_JAVA], [TRUE], [Enabled Java Support (javac=ok swig=ok)])
 			AC_MSG_RESULT(yes)
 			JAVASTATUS="yes, path: `which $JAVAC`"
 		else
 			AC_DEFINE([HAS_JAVA], [FALSE], [Java support disabled because swig not found (javac=ok swig=missing)])
 			AC_MSG_RESULT(no, swig not found and that is need for java)
 			JAVASTATUS="java compiler found, swig not found"
 		fi
 	else
 		if test $SWIG ; then
 			AC_DEFINE([HAS_JAVA], [FALSE], [Disabled Java support (javac=missing swig=ok])
 			AC_MSG_RESULT(no, javac missing, swig ok)
 			JAVASTATUS="java compiler not found, swig found"
 		else
 			AC_DEFINE([HAS_JAVA], [FALSE], [Disabled Java support (javac=missing swig=missing])
 			AC_MSG_RESULT(no, javac missing, swig missing)
 			JAVASTATUS="java compiler not found, swig not found"
 		fi
 	fi
else
    AM_CONDITIONAL(HAS_JAVA, test $JAVAC)
    AC_DEFINE([HAS_JAVA], [FALSE], [Not specified at configure line])
    AC_MSG_RESULT(not requested by configure)
    JAVASTATUS="no, disabled by configure"
fi

dnl Check for bzip2
AC_CHECK_LIB([bz2], [BZ2_bzCompress], [AC_CHECK_HEADER([bzlib.h], [LIBBZ2='-lbz2'])])
if test -n "$LIBBZ2"; then
    LDFLAGS="$LDFLAGS $LIBBZ2"
else
    AC_MSG_ERROR("bzip2 library and its include files are needed to build pacman!");
fi

dnl Check for math
AC_CHECK_LIB([m], [log10], [AC_CHECK_HEADER([math.h], [LIBM='-lm'])])
if test -n "$LIBM"; then
    LDFLAGS="$LDFLAGS $LIBM"
else
    AC_MSG_ERROR("math library not found!");
fi

dnl Check for libarchive
AC_CHECK_LIB([archive], [archive_read_data], [AC_CHECK_HEADER([archive.h], [LIBARCHIVE='-larchive'])])
if test -n "$LIBARCHIVE"; then
    CFLAGS="$CFLAGS"
    LDFLAGS="$LDFLAGS $LIBARCHIVE"
else
    AC_MSG_ERROR("libarchive is needed to compile pacman!");
fi

dnl Enable or disable debug code
AC_MSG_CHECKING(for debug mode request)
if test x$debug = xyes ; then
    AM_CONDITIONAL(PACMAN_DEBUG, test x$debug = xyes)
	CFLAGS="-g -Wall -Werror -std=c99 -DPACMAN_DEBUG"
	LDFLAGS="$LDFLAGS -lmcheck"
    AC_MSG_RESULT(yes)
else
    AM_CONDITIONAL(PACMAN_DEBUG, test x$debug = xno)
	CFLAGS="$CFLAGS -Wall -std=c99"
    AC_MSG_RESULT(no)
fi

dnl Enable or disable fakeroot code
AC_MSG_CHECKING(for fakeroot proof support)
if test x$fakeroot = xyes ; then
	AC_MSG_RESULT(yes)
else
	CFLAGS="$CFLAGS -DFAKEROOT"
	AC_MSG_RESULT(no)
fi

CFLAGS="$CFLAGS"

dnl ==========================================================================

AC_OUTPUT([
lib/libalpm/Makefile
lib/libalpm/po/Makefile.in
src/pacman/Makefile
src/pacman/po/Makefile.in
src/util/Makefile
scripts/Makefile
doc/Makefile
doc/makepkg.8
doc/libalpm.3
doc/pacman.8
doc/hu/Makefile
etc/Makefile
etc/pacman.d/Makefile
bindings/perl/Makefile
bindings/python/Makefile
bindings/java/Makefile
Makefile
])

echo "
pacman-$VERSION:

        prefix                   : ${prefix}
        source code location     : ${srcdir}
        compiler                 : ${CC}
        compiler flags           : ${CFLAGS}

	libalpm version          : ${PM_VERSION}
	pacman.conf location     : ${PACCONF}

	Architecture CARCH       : ${CARCH}
	Architecture used cflag  : ${CARCHFLAGS}
	Architecture CHOST       : ${CHOST}

	Doxygen support          : ${DOXYSTATUS}
	Manpage localization     : ${POSTATUS}
	Perl bindings            : ${PERLSTATUS}
	Python bindings          : ${PYTHONSTATUS}
	Java bindings            : ${JAVASTATUS}

	debug support            : ${debug}
"
