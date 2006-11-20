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
%pointer_cast(void *, pmlist_t *, void_to_pmlist);
%pointer_cast(void *, pmpkg_t *, void_to_pmpkg);
%pointer_cast(void *, pmgrp_t *, void_to_pmgrp);
%pointer_cast(void *, pmsyncpkg_t *, void_to_pmsyncpkg);
%pointer_cast(void *, pmdb_t *, void_to_pmdb);
%pointer_cast(void *, pmconflict_t *, void_to_pmconflict);

%include "alpm.h"
