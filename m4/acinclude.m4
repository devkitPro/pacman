dnl acinclude.m4 - configure macros used by pacman and libalpm
dnl Add some custom macros for pacman and libalpm

dnl GCC_STACK_PROTECT_LIB
dnl adds -lssp to LIBS if it is available
dnl ssp is usually provided as part of libc, but was previously a separate lib
dnl It does not hurt to add -lssp even if libc provides SSP - in that case
dnl libssp will simply be ignored.
AC_DEFUN([GCC_STACK_PROTECT_LIB],[
  AC_CACHE_CHECK([whether libssp exists], ssp_cv_lib,
    [ssp_old_libs="$LIBS"
     LIBS="$LIBS -lssp"
     AC_TRY_LINK(,, ssp_cv_lib=yes, ssp_cv_lib=no)
     LIBS="$ssp_old_libs"
    ])
  if test $ssp_cv_lib = yes; then
    LIBS="$LIBS -lssp"
  fi
])

dnl GCC_STACK_PROTECT_CC
dnl checks -fstack-protector-all with the C compiler, if it exists then updates
dnl CFLAGS and defines ENABLE_SSP_CC
AC_DEFUN([GCC_STACK_PROTECT_CC],[
  AC_LANG_ASSERT(C)
  if test "X$CC" != "X"; then
    AC_CACHE_CHECK([whether ${CC} accepts -fstack-protector-all],
      ssp_cv_cc,
      [ssp_old_cflags="$CFLAGS"
       CFLAGS="$CFLAGS -fstack-protector-all"
       AC_TRY_COMPILE(,, ssp_cv_cc=yes, ssp_cv_cc=no)
       CFLAGS="$ssp_old_cflags"
      ])
    if test $ssp_cv_cc = yes; then
      CFLAGS="$CFLAGS -fstack-protector-all"
      AC_DEFINE([ENABLE_SSP_CC], 1, [Define if SSP C support is enabled.])
    fi
  fi
])

dnl GCC_FORTIFY_SOURCE_CC
dnl checks -D_FORTIFY_SOURCE with the C compiler, if it exists then updates
dnl CPPFLAGS
AC_DEFUN([GCC_FORTIFY_SOURCE_CC],[
  AC_LANG_ASSERT(C)
  if test "X$CC" != "X"; then
    AC_MSG_CHECKING(for FORTIFY_SOURCE support)
    fs_old_cppflags="$CPPFLAGS"
    fs_old_cflags="$CFLAGS"
    CPPFLAGS="$CPPFLAGS -D_FORTIFY_SOURCE=2"
    CFLAGS="$CFLAGS -Werror"
    AC_TRY_COMPILE([#include <features.h>], [
      int main() {
      #if !(__GNUC_PREREQ (4, 1) )
      #error No FORTIFY_SOURCE support
      #endif
        return 0;
      }
    ], [
      AC_MSG_RESULT(yes)
      CFLAGS="$fs_old_cflags"
    ], [
      AC_MSG_RESULT(no)
      CPPFLAGS="$fs_old_cppflags"
      CFLAGS="$fs_old_cflags"
    ])
  fi
])

dnl GCC_VISIBILITY_CC
dnl checks -fvisibility=internal with the C compiler, if it exists then
dnl defines ENABLE_VISIBILITY_CC in both configure script and Makefiles
AC_DEFUN([GCC_VISIBILITY_CC],[
  AC_LANG_ASSERT(C)
  if test "X$CC" != "X"; then
    AC_CACHE_CHECK([whether ${CC} accepts -fvisibility=internal],
      visibility_cv_cc,
      [visibility_old_cflags="$CFLAGS"
       CFLAGS="$CFLAGS -fvisibility=internal"
       AC_TRY_COMPILE(,, visibility_cv_cc=yes, visibility_cv_cc=no)
       CFLAGS="$visibility_old_cflags"
      ])
    if test $visibility_cv_cc = yes; then
      AC_DEFINE([ENABLE_VISIBILITY_CC], 1, [Define if symbol visibility C support is enabled.])
    fi
    AM_CONDITIONAL([ENABLE_VISIBILITY_CC], test "x$visibility_cv_cc" = "xyes")
  fi
])

dnl CFLAGS_ADD(PARAMETER, VARIABLE)
dnl Adds parameter to VARIABLE if the compiler supports it.  For example,
dnl CFLAGS_ADD([-Wall],[WARN_FLAGS]).
AC_DEFUN([CFLAGS_ADD],
[AS_VAR_PUSHDEF([my_cflags], [cflags_cv_warn_$1])dnl
AC_CACHE_CHECK([whether compiler handles $1], [my_cflags], [
  save_CFLAGS="$CFLAGS"
  CFLAGS="${CFLAGS} -Werror=unknown-warning-option"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])],
                    [],
                    [CFLAGS="$save_CFLAGS"])
  CFLAGS="${CFLAGS} $1"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])],
                    [AS_VAR_SET([my_cflags], [yes])],
                    [AS_VAR_SET([my_cflags], [no])])
  CFLAGS="$save_CFLAGS"
])
AS_VAR_PUSHDEF([new_cflags], [[$2]])dnl
AS_VAR_IF([my_cflags], [yes], [AS_VAR_APPEND([new_cflags], [" $1"])])
AS_VAR_POPDEF([new_cflags])dnl
AS_VAR_POPDEF([my_cflags])dnl
m4_ifval([$2], [AS_LITERAL_IF([$2], [AC_SUBST([$2])], [])])dnl
])

dnl Checks for getmntinfo and determines whether it uses statfs or statvfs
AC_DEFUN([FS_STATS_TYPE],
  [AC_CACHE_CHECK([filesystem statistics type], fs_stats_cv_type,
    [AC_CHECK_FUNC(getmntinfo,
      [AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM([[
# include <sys/param.h>
# include <sys/mount.h>
#if HAVE_SYS_UCRED_H
#include <sys/ucred.h>
#endif
extern int getmntinfo (struct statfs **, int);
]],
          [])],
        [fs_stats_cv_type="struct statfs"],
        [fs_stats_cv_type="struct statvfs"])],
      [AC_CHECK_FUNC(getmntent,
        [fs_stats_cv_type="struct statvfs"])]
    )]
  )
  AC_DEFINE_UNQUOTED(FSSTATSTYPE, [$fs_stats_cv_type],
    [Defined as the filesystem stats type ('statvfs' or 'statfs')])
  if test $ac_cv_func_getmntinfo = yes; then
    if test "$fs_stats_cv_type" = "struct statvfs"; then
      AC_DEFINE([HAVE_GETMNTINFO_STATVFS], 1, [Define if getmntinfo() uses statvfs.])
    else
      AC_DEFINE([HAVE_GETMNTINFO_STATFS], 1, [Define if getmntinfo() uses statfs.])
    fi
  fi
])

dnl Checks for PATH_MAX and defines it if not present
AC_DEFUN([PATH_MAX_DEFINED],
  [AC_CACHE_CHECK([PATH_MAX defined], path_max_cv_defined,
    [AC_EGREP_CPP(yes, [[
#include <limits.h>
#if defined(PATH_MAX)
yes
#endif
]],
      [path_max_cv_defined=yes],
      [path_max_cv_defined=no])]
  )
  if test $path_max_cv_defined = no; then
    AC_DEFINE([PATH_MAX], 4096, [Define if PATH_MAX is undefined by limits.h.])
  fi
])
