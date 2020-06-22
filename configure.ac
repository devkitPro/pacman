#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# Minimum version of autoconf required
AC_PREREQ(2.64)

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

m4_define([lib_current], [12])
m4_define([lib_revision], [2])
m4_define([lib_age], [0])

m4_define([pacman_version_major], [5])
m4_define([pacman_version_minor], [2])
m4_define([pacman_version_micro], [2])
m4_define([pacman_version],
          [pacman_version_major.pacman_version_minor.pacman_version_micro])

# Autoconf initialization
AC_INIT([pacman], [pacman_version], [pacman-dev@archlinux.org])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_REQUIRE_AUX_FILE([tap-driver.sh])

AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([1.11 foreign])
AM_SILENT_RULES([yes])

LT_INIT
LIB_VERSION=`expr lib_current - lib_age`.lib_age.lib_revision
LIB_VERSION_INFO="lib_current:lib_revision:lib_age"

# Respect empty CFLAGS during compiler tests
if test "x$CFLAGS" = "x"; then
  CFLAGS=""
fi

# Set substitution values for version stuff in Makefiles and anywhere else,
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

# Help line for buildscript filename
AC_ARG_WITH(makepkg-template-dir,
	AS_HELP_STRING([--with-makepkg-template-dir=name], [set the template dir used by makepkg-template]),
	[TEMPLATE_DIR=$withval], [TEMPLATE_DIR=${datarootdir}/makepkg-template])

# Help line for debug package suffix
AC_ARG_WITH(debug-suffix,
	AS_HELP_STRING([--with-debug-suffix=name], [set the suffix for split debugging symbol packages used by makepkg]),
	[DEBUGSUFFIX=$withval], [DEBUGSUFFIX=debug])

# Help line for changing shell used to run install scriptlets
AC_ARG_WITH(scriptlet-shell,
	AS_HELP_STRING([--with-scriptlet-shell=shell],
		[set the full path to the shell used to run install scriptlets]),
	[SCRIPTLET_SHELL=$withval], [SCRIPTLET_SHELL=/bin/sh])

# Help line for ldconfig path
AC_ARG_WITH(ldconfig,
	AS_HELP_STRING([--with-ldconfig=path],
		[set the full path to ldconfig]),
	[LDCONFIG=$withval], [LDCONFIG=/sbin/ldconfig])

# Help line for determining whether file is seccomp-enabled
AC_ARG_WITH(file-seccomp,
	AS_HELP_STRING([--with-file-seccomp={yes|no|auto}],
		[determine whether file is seccomp-enabled @<:@default=auto@:>@]),
	[with_file_seccomp=$withval], [with_file_seccomp=auto])

# Help line for selecting a crypto library
AC_ARG_WITH(crypto,
	AS_HELP_STRING([--with-crypto={openssl|nettle}],
		[select crypto implementation @<:@default=openssl@:>@]),
	[with_crypto=$withval], [with_crypto=openssl])

# Help line for using gpgme
AC_ARG_WITH(gpgme,
	AS_HELP_STRING([--with-gpgme], [use GPGME for PGP signature verification]),
	[], [with_gpgme=check])

# Help line for using libcurl
AC_ARG_WITH(libcurl,
	AS_HELP_STRING([--with-libcurl], [use libcurl for the internal downloader]),
	[], [with_libcurl=check])

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

# Help line for compiler warning flags
AC_ARG_ENABLE(warningflags,
	AS_HELP_STRING([--enable-warningflags], [enable extra compiler warning flags]),
	[warningflags=$enableval], [warningflags=no])

# Help line for using git version in pacman version string
AC_ARG_ENABLE(git-version,
	AS_HELP_STRING([--enable-git-version],
		[enable use of git version in version string if available]),
	[wantgitver=$enableval], [wantgitver=no])

# Enable large file support if available (must be enabled before
# testing compilation against gpgme).
AC_SYS_LARGEFILE

# Record large file flags in pkgconfig file
if test "$enable_largefile" != no; then
	if test "$ac_cv_sys_file_offset_bits" != 'no'; then
		LFS_CFLAGS="-D_FILE_OFFSET_BITS=$ac_cv_sys_file_offset_bits"
	fi
fi
AC_SUBST(LFS_CFLAGS)


# Checks for programs.
AC_PROG_AWK
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_CHECK_PROGS([PYTHON], [python3 python], [false])
AC_PATH_PROGS([BASH_SHELL], [bash bash4], [false])

# check for perl 5.10.1 (needed by makepkg-template)
AC_PATH_PROG([PERL],[perl])
AC_DEFUN([AX_PROG_PERL_VERSION],
	[AC_CACHE_CHECK([for Perl version $1 or later], [ax_cv_prog_perl_version],
		[AS_IF(["$PERL" -e 'require v$1;' >/dev/null 2>&1],
			[ax_cv_prog_perl_version=yes],
			[ax_cv_prog_perl_version=no])])
	AS_IF([test x"$ax_cv_prog_perl_version" = xyes], [$2], [$3])])
AX_PROG_PERL_VERSION([5.10.1], [], [AC_MSG_ERROR([perl is too old])])

AS_IF([test "x$BASH_SHELL" = "xfalse"],
	AC_MSG_WARN([*** bash >= 4.4.0 is required for pacman scripts]),
	[bash_version_major=`$BASH_SHELL -c 'echo "${BASH_VERSINFO[[0]]}"'`
	bash_version_minor=`$BASH_SHELL -c 'echo "${BASH_VERSINFO[[1]]}"'`
	ok=yes
	if test "$bash_version_major" -lt 4; then
		ok=no
	fi
	if test "$bash_version_major" -eq 4 && test "$bash_version_minor" -lt 4; then
		ok=no
	fi
	if test "$ok" = "no"; then
		AC_MSG_ERROR([*** bash >= 4.4.0 is required for pacman scripts])
	fi
	unset bash_version_major bash_version_minor ok])

# find installed gettext
AM_GNU_GETTEXT([external], [need-ngettext])
AM_GNU_GETTEXT_VERSION(0.13.1)

AC_CHECK_LIB([m], [fabs], ,
	AC_MSG_ERROR([libm is needed to compile pacman!]))

PKG_CHECK_VAR(bashcompdir, [bash-completion], [completionsdir], ,
	bashcompdir="${datarootdir}/bash-completion/completions")

# Check for libarchive
PKG_CHECK_MODULES(LIBARCHIVE, [libarchive >= 3.0.0], ,
	AC_MSG_ERROR([*** libarchive >= 3.0.0 is needed to compile pacman!]))

m4_pattern_forbid([^AX_COMPARE_VERSION$])
# Check file for seccomp
if test "x$with_file_seccomp" = "xauto"; then
	file_version="$(file --version| sed -n 's/^file-\(.*\)/\1/p')"
	AX_COMPARE_VERSION([$file_version], [ge], [5.38], [with_file_seccomp=yes])
fi
if test "x$with_file_seccomp" = "xyes"; then
	FILECMD="file -S"
else
	FILECMD="file"
fi
AC_SUBST(FILECMD)

# Check for OpenSSL
have_openssl=no
have_nettle=no
if test "x$with_crypto" = "xnettle"; then
	AC_SUBST(pc_crypto, [nettle])
	PKG_CHECK_MODULES(NETTLE, [nettle],
		[AC_DEFINE(HAVE_LIBNETTLE, 1, [Define whether to use nettle]) have_nettle=yes], have_nettle=no)
	if test "x$have_nettle" = xno -a "x$with_crypto" = xnettle; then
		AC_MSG_ERROR([*** nettle support requested but libraries not found])
	fi
else if test "x$with_crypto" = "xopenssl"; then
	AC_SUBST(pc_crypto, [libcrypto])
	PKG_CHECK_MODULES(LIBSSL, [libcrypto],
		[AC_DEFINE(HAVE_LIBSSL, 1, [Define if libcrypto is available]) have_openssl=yes], have_openssl=no)
	if test "x$have_openssl" = xno; then
		AC_MSG_ERROR([*** openssl support requested but libraries not found])
	fi
else
	AC_MSG_ERROR([*** unknown crypto support library requested - $with_crypto])
fi
fi
AM_CONDITIONAL(HAVE_LIBSSL, [test "$have_openssl" = "yes"])
AM_CONDITIONAL(HAVE_LIBNETTLE, [test "$have_nettle" = "yes"])

# Check for libcurl
have_libcurl=no
if test "x$with_libcurl" != "xno"; then
	PKG_CHECK_MODULES(LIBCURL, [libcurl >= 7.32.0],
		[AC_DEFINE(HAVE_LIBCURL, 1, [Define if libcurl is available]) have_libcurl=yes], have_libcurl=no)
	if test "x$have_libcurl" = xno -a "x$with_libcurl" = xyes; then
		AC_MSG_ERROR([*** libcurl >= 7.32.0 is required for internal downloader support])
	fi
fi
AM_CONDITIONAL(HAVE_LIBCURL, [test "$have_libcurl" = "yes"])
# used to generate libalpm.pc
if test "x$have_libcurl" = xyes; then
	AC_SUBST(pc_libcurl, [libcurl])
fi

# Check for gpgme
AC_MSG_CHECKING(whether to link with libgpgme)
AS_IF([test "x$with_gpgme" != "xno"],
	[AC_MSG_RESULT([yes])],
	[AC_MSG_RESULT([no])])

have_gpgme=no
if test "x$with_gpgme" != "xno"; then
	PKG_CHECK_MODULES(GPGME, [gpgme],
		[AC_DEFINE([HAVE_LIBGPGME], [1], [Define if gpgme should be used to provide GPG signature support.])
		AC_SUBST(pc_gpgme, [gpgme])
		have_gpgme=yes],
		[AM_PATH_GPGME([1.3.0],
			[LIBS_save="$LIBS"
			CPPFLAGS_save="$CPPFLAGS"
			CFLAGS_save="$CFLAGS"

			LIBS="$LIBS $GPGME_LIBS"
			CPPFLAGS="$CPPFLAGS $GPGME_CPPFLAGS"
			CFLAGS="$CFLAGS $GPGME_CFLAGS"

			AC_MSG_CHECKING([for sane gpgme])
			AC_LINK_IFELSE(
				[AC_LANG_PROGRAM(
					[[#include <gpgme.h>]],
					[[return gpgme_check_version("1.3.0");]])],
				[AC_MSG_RESULT([yes])
				have_gpgme=yes
				AC_DEFINE([HAVE_LIBGPGME], [1], [Define if gpgme should be used to provide GPG signature support.])
				AC_SUBST(pc_gpgme_libs, ["$GPGME_LIBS"])],
				[AC_MSG_RESULT([no])
				have_gpgme=no
				unset GPGME_LIBS
				unset GPGME_CFLAGS]
				AS_IF([test "x$with_gpgme" = "xyes"],
					[AC_MSG_FAILURE([*** gpgme >= 1.3.0 is needed for GPG signature support])])
				)

			LIBS="$LIBS_save"
			CPPFLAGS="$CPPFLAGS_save"
			CFLAGS="$CFLAGS_save"
			unset CPPFLAGS_save
			unset CFLAGS_save],)])
fi

AS_IF([test "x$have_gpgme" = xno -a "x$with_gpgme" = xyes],
	[AC_MSG_FAILURE([--with-gpgme was given, but gpgme was not found])])
AM_CONDITIONAL([HAVE_LIBGPGME], [test "x$have_gpgme" = "xyes"])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h float.h glob.h langinfo.h libintl.h limits.h \
                  locale.h mntent.h netinet/in.h netinet/tcp.h \
                  stddef.h string.h sys/ioctl.h \
                  sys/mnttab.h sys/mount.h \
                  sys/param.h sys/statvfs.h sys/time.h sys/types.h \
                  sys/ucred.h syslog.h termios.h wchar.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT64_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_STRUCT_TM
AC_TYPE_UID_T
AC_STRUCT_DIRENT_D_TYPE
PATH_MAX_DEFINED

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_GETMNTENT
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_CHECK_FUNCS([dup2 getcwd getmntinfo gettimeofday memmove memset \
                mkdir realpath regcomp rmdir setenv setlocale strcasecmp \
                strchr strcspn strdup strerror strndup strnlen strrchr \
                strsep strstr strtol swprintf tcflush wcwidth uname])
AC_CHECK_MEMBERS([struct stat.st_blksize],,,[[#include <sys/stat.h>]])

# For the diskspace code
FS_STATS_TYPE
AC_CHECK_MEMBERS([struct statvfs.f_flag],,,[[#include <sys/statvfs.h>]])
AC_CHECK_MEMBERS([struct statfs.f_flags],,,[[#include <sys/param.h>
                  #include <sys/mount.h>]])

# Check if we can use symbol visibility support in GCC
GCC_VISIBILITY_CC

# Host-dependant definitions
DEFAULT_SEDINPLACEFLAGS=" --follow-symlinks -i"
INODECMD="stat -c '%i %n'"
STRIP_BINARIES="--strip-all"
STRIP_SHARED="--strip-unneeded"
STRIP_STATIC="--strip-debug"
case "${host_os}" in
	*bsd*)
		INODECMD="stat -f '%i %N'"
		DEFAULT_SEDINPLACEFLAGS=" -i \"\""
		;;
	darwin*)
		host_os_darwin=yes
		INODECMD="/usr/bin/stat -f '%i %N'"
		DEFAULT_SEDINPLACEFLAGS=" -i ''"
		STRIP_BINARIES=""
		STRIP_SHARED="-S"
		STRIP_STATIC="-S"
		;;
esac
AM_CONDITIONAL([DARWIN], test "x$host_os_darwin" = "xyes")
AC_PATH_PROGS([SEDPATH], [sed], [sed], [/usr/bin$PATH_SEPARATOR/bin] )
AC_SUBST(INODECMD)
AC_SUBST(STRIP_BINARIES)
AC_SUBST(STRIP_SHARED)
AC_SUBST(STRIP_STATIC)

# Flags for sed in place
if test "${SEDINPLACEFLAGS+set}" != "set"; then
    SEDINPLACEFLAGS="${DEFAULT_SEDINPLACEFLAGS}"
fi
AC_ARG_VAR(SEDINPLACEFLAGS, [flags for sed, overriding the default])

# Variables plugged into makepkg.conf
CARCH="${host%%-*}"
CHOST="${host}"
AC_SUBST(CARCH)
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
	# Check for -fstack-protector availability
	GCC_STACK_PROTECT_LIB
	GCC_STACK_PROTECT_CC
	GCC_STACK_CLASH_PROTECTION
	GCC_FORTIFY_SOURCE_CC
	WARNING_CFLAGS="-g -Wall -Werror"
else
	AC_MSG_RESULT(no)
	WARNING_CFLAGS="-Wall"
fi

# Enable or disable compiler warning flags
AC_MSG_CHECKING(for excessive compiler warning flags)
if test "x$warningflags" = "xyes" ; then
	AC_MSG_RESULT(yes)
	CFLAGS_ADD([-Wcast-align], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wclobbered], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wempty-body], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wfloat-equal], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wformat-nonliteral], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wformat-security], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wignored-qualifiers], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wimplicit-fallthrough], [WARNING_CFLAGS])
	CFLAGS_ADD([-Winit-self], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wlogical-op], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wmissing-declarations], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wmissing-field-initializers], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wmissing-parameter-type], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wmissing-prototypes], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wold-style-declaration], [WARNING_CFLAGS])
	CFLAGS_ADD([-Woverride-init], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wpointer-arith], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wredundant-decls], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wshadow], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wsign-compare], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wstrict-aliasing], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wstrict-overflow=5], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wstrict-prototypes], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wtype-limits], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wuninitialized], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wunused-but-set-parameter], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wunused-parameter], [WARNING_CFLAGS])
	CFLAGS_ADD([-Wwrite-strings], [WARNING_CFLAGS])
else
	AC_MSG_RESULT(no)
fi

# Enable or disable use of git version in pacman version string
AC_MSG_CHECKING(whether to use git version if available)
if test "x$wantgitver" = "xyes" ; then
	AC_CHECK_PROGS([GIT], [git])
	AC_CHECK_FILE([.git/], hasgitdir=yes)
	usegitver=$ac_cv_file__git_
	if test $GIT -a "x$hasgitdir" = "xyes"; then
		AC_DEFINE([USE_GIT_VERSION], , [Use GIT version in version string])
	fi
else
	AC_MSG_RESULT([no, disabled by configure])
	usegitver=no
fi
AM_CONDITIONAL(USE_GIT_VERSION, test "x$usegitver" = "xyes")

# Set root directory
AC_SUBST(ROOTDIR)
AC_DEFINE_UNQUOTED([ROOTDIR], "$ROOTDIR", [The location of the root operating directory])
# Set package file extension
AC_SUBST(PKGEXT)
AC_DEFINE_UNQUOTED([PKGEXT], "$PKGEXT", [The file extension used by pacman packages])
# Set source package file extension
AC_SUBST(SRCEXT)
AC_DEFINE_UNQUOTED([SRCEXT], "$SRCEXT", [The file extension used by pacman source packages])
# Set makepkg build script name
AC_SUBST(BUILDSCRIPT)
AC_DEFINE_UNQUOTED([BUILDSCRIPT], "$BUILDSCRIPT", [The build script name used by makepkg])
# Set makepkg-template template directory
AC_SUBST(TEMPLATE_DIR)
AC_DEFINE_UNQUOTED([TEMPLATE_DIR], "$TEMPLATE_DIR", [The template directory used by makepkg-teplate])
# Set makepkg split debugging symbol package suffix
AC_SUBST(DEBUGSUFFIX)
AC_DEFINE_UNQUOTED([DEBUGSUFFIX], "$DEBUGSUFFIX", [The suffix for debugging symbol packages used by makepkg])
# Set shell used by install scriptlets
AC_SUBST(SCRIPTLET_SHELL)
AC_DEFINE_UNQUOTED([SCRIPTLET_SHELL], "$SCRIPTLET_SHELL", [The full path of the shell used to run install scriptlets])
# Set ldconfig path
AC_SUBST(LDCONFIG)
AC_DEFINE_UNQUOTED([LDCONFIG], "$LDCONFIG", [The full path to ldconfig])


# Configuration files
AC_CONFIG_FILES([
lib/libalpm/Makefile
lib/libalpm/po/Makefile.in
lib/libalpm/libalpm.pc
src/common/Makefile
src/pacman/Makefile
src/pacman/po/Makefile.in
src/util/Makefile
scripts/Makefile
scripts/po/Makefile.in
doc/Makefile
etc/Makefile
test/pacman/Makefile
test/pacman/tests/Makefile
test/scripts/Makefile
test/util/Makefile
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
    preprocessor flags     : ${CPPFLAGS}
    compiler flags         : ${WARNING_CFLAGS} ${CFLAGS}
    library flags          : ${LIBS} ${LIBSSL_LIBS} ${NETTLE_LIBS} ${LIBARCHIVE_LIBS} ${LIBCURL_LIBS} ${GPGME_LIBS}
    linker flags           : ${LDFLAGS}

    Architecture           : ${CARCH}
    Host Type              : ${CHOST}
    File inode command     : ${INODECMD}
    In-place sed command   : ${SEDPATH} ${SEDINPLACEFLAGS}
    File seccomp command   : ${FILECMD}

    libalpm version        : ${LIB_VERSION}
    libalpm version info   : ${LIB_VERSION_INFO}
    pacman version         : ${PACKAGE_VERSION}
    using git version      : ${usegitver}

  Directory and file information:
    root working directory : ${ROOTDIR}
    package extension      : ${PKGEXT}
    source pkg extension   : ${SRCEXT}
    build script name      : ${BUILDSCRIPT}
    template directory     : ${TEMPLATE_DIR}

  Compilation options:
    Use libcurl            : ${have_libcurl}
    Use GPGME              : ${have_gpgme}
    Use OpenSSL            : ${have_openssl}
    Use nettle             : ${have_nettle}
    Run make in doc/ dir   : ${wantdoc} ${asciidoc}
    Doxygen support        : ${usedoxygen}
    debug support          : ${debug}
    extra warning flags    : ${warningflags}
    use git version        : ${wantgitver}
"
