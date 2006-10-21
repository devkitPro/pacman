#if defined(SWIGPERL)
%module "Alpm::Core"
#else
%module alpm
#endif
%include "cpointer.i"

/* Wrap a class interface around a "long *" */
%pointer_class(long, longp);

/* Create casting functions */

%pointer_cast(void *, long *, void_to_long);
%pointer_cast(void *, char *, void_to_char);
%pointer_cast(void *, unsigned long, void_to_unsigned_long);
%pointer_cast(void *, PM_LIST *, void_to_PM_LIST);
%pointer_cast(void *, PM_PKG *, void_to_PM_PKG);
%pointer_cast(void *, PM_GRP *, void_to_PM_GRP);
%pointer_cast(void *, PM_SYNCPKG *, void_to_PM_SYNCPKG);
%pointer_cast(void *, PM_DB *, void_to_PM_DB);
%pointer_cast(void *, PM_CONFLICT *, void_to_PM_CONFLICT);

%include "alpm.h"
