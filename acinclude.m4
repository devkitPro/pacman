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
dnl CFLAGS
AC_DEFUN([GCC_FORTIFY_SOURCE_CC],[
  AC_LANG_ASSERT(C)
  if test "X$CC" != "X"; then
    AC_MSG_CHECKING(for FORTIFY_SOURCE support)
    AC_TRY_COMPILE([#include <features.h>], [
      int main() {
      #if !(__GNUC_PREREQ (4, 1) )
      #error No FORTIFY_SOURCE support
      #endif
        return 0;
      }
    ], [
      AC_MSG_RESULT(yes)
      CFLAGS="$CFLAGS -D_FORTIFY_SOURCE=2"
    ], [
      AC_MSG_RESULT(no)
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

dnl GCC_GNU89_INLINE_CC
dnl checks -fgnu89-inline with the C compiler, if it exists then defines
dnl ENABLE_GNU89_INLINE_CC in both configure script and Makefiles
AC_DEFUN([GCC_GNU89_INLINE_CC],[
  AC_LANG_ASSERT(C)
  if test "X$CC" != "X"; then
    AC_CACHE_CHECK([for -fgnu89-inline],
    gnu89_inline_cv_cc,
    [ gnu89_inline_old_cflags="$CFLAGS"
      CFLAGS="$CFLAGS -fgnu89-inline"
      AC_TRY_COMPILE(,, gnu89_inline_cv_cc=yes, gnu89_inline_cv_cc=no)
      CFLAGS="$gnu89_inline_old_cflags"
    ])
    if test $gnu89_inline_cv_cc = yes; then
      AC_DEFINE([ENABLE_GNU89_INLINE_CC], 1, [Define if gnu89 inlining semantics should be used.])
    fi
    AM_CONDITIONAL([ENABLE_GNU89_INLINE_CC], test "x$gnu89_inline_cv_cc" = "xyes")
  fi
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
