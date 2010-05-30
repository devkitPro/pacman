CONFTESTS = $(patsubst %py.in,%py,$(wildcard *.py.in))

check_SCRIPTS =	\
	$(wildcard *.py) \
	$(CONFTESTS)

noinst_SCRIPTS = $(check_SCRIPTS)

EXTRA_DIST = $(check_SCRIPTS)

CLEANFILES = $(CONFTESTS)

#### Taken from the autoconf scripts Makefile.am ####
edit = sed \
  -e 's|@LIBS[@]|$(LIBS)|g' \
  -e 's|@configure_input[@]|Generated from $@.in; do not edit by hand.|g'


$(CONFTESTS): Makefile
	@echo '    ' GEN $@;
	@rm -f $@ $@.tmp
	@test -f $(srcdir)/$@.in && $(edit) $(srcdir)/$@.in >$@.tmp || true
	@test -f $@.tmp || false
	@chmod a-w $@.tmp
	@mv $@.tmp $@

# vim:set ts=2 sw=2 noet:
