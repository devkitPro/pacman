CONFTESTS = $(patsubst %py.in,%py,$(wildcard *.py.in))

check_SCRIPTS =	\
	$(wildcard *.py) \
	$(CONFTESTS)

noinst_SCRIPTS = $(check_SCRIPTS)

EXTRA_DIST = $(check_SCRIPTS)

CLEANFILES = $(CONFTESTS)

#### Taken from the autoconf scripts Makefile.am ####
edit = sed \
  -e 's|@LIBCURL[@]|$(LIBCURL)|g' \
  -e 's|@configure_input[@]|Generated from $@.in; do not edit by hand.|g'


$(CONFTESTS): Makefile
	$(AM_V_at)$(RM) $@ $@.tmp
	$(AM_V_GEN)test -f $(srcdir)/$@.in && $(edit) $(srcdir)/$@.in >$@.tmp || true
	$(AM_V_at)chmod a-w $@.tmp
	$(AM_V_at)mv $@.tmp $@

# vim:set noet:
