OURSCRIPTS = \
	bacman \
	pacdiff \
	paclist \
	pacscripts \
	pacsearch

OURFILES = \
	bash_completion \
	zsh_completion

EXTRA_DIST = \
	PKGBUILD.vim \
	bacman.in \
	bash_completion.in \
	pacdiff.in \
	paclist.in \
	pacscripts.in \
	pacsearch.in \
	vimprojects \
	zsh_completion.in \
	README

# Files that should be removed, but which Automake does not know.
MOSTLYCLEANFILES = $(OURSCRIPTS) $(OURFILES) *.tmp

edit = sed \
	-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
	-e 's|@localstatedir[@]|$(localstatedir)|g' \
	-e 's|@BASH_SHELL[@]|$(BASH_SHELL)|g'

$(OURSCRIPTS): Makefile
	@echo '    ' GEN $@;
	@rm -f $@ $@.tmp
	@$(edit) $(srcdir)/$@.in >$@.tmp
	@chmod +x $@.tmp
	@chmod a-w $@.tmp
	@mv $@.tmp $@

$(OURFILES): Makefile
	@echo '    ' GEN $@;
	@rm -f $@ $@.tmp
	@$(edit) $(srcdir)/$@.in >$@.tmp
	@chmod a-w $@.tmp
	@mv $@.tmp $@

all-am: $(OURSCRIPTS) $(OURFILES)

bacman: $(srcdir)/bacman.in
bash_completion: $(srcdir)/bash_completion.in
pacdiff: $(srcdir)/pacdiff.in
paclist: $(srcdir)/paclist.in
pacscripts: $(srcdir)/pacscripts.in
pacsearch: $(srcdir)/pacsearch.in
pactree: $(srcdir)/pactree.in
zsh_completion: $(srcdir)/zsh_completion.in

# vim:set ts=2 sw=2 noet:
