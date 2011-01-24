OURFILES = \
	bacman \
	bash_completion \
	pacdiff \
	paclist \
	pacscripts \
	pacsearch \
	wget-xdelta.sh \
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
	wget-xdelta.sh.in \
	zsh_completion.in \
	README

# Files that should be removed, but which Automake does not know.
MOSTLYCLEANFILES = $(OURFILES) *.tmp

edit = sed \
	-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
	-e 's|@localstatedir[@]|$(localstatedir)|g' \
	-e 's|@BASH_SHELL[@]|$(BASH_SHELL)|g'

$(OURFILES): Makefile
	@echo '    ' GEN $@;
	@rm -f $@ $@.tmp
	@cp -a $@.in $@.tmp
	@$(edit) $@.in >$@.tmp
	@mv $@.tmp $@

all-am: $(OURFILES)

bacman: $(srcdir)/bacman.in
bash_completion: $(srcdir)/bash_completion.in
pacdiff: $(srcdir)/pacdiff.in
paclist: $(srcdir)/paclist.in
pacscripts: $(srcdir)/pacscripts.in
pacsearch: $(srcdir)/pacsearch.in
pactree: $(srcdir)/pactree.in
wget-xdelta.sh: $(srcdir)/wget-xdelta.sh.in
zsh_completion: $(srcdir)/zsh_completion.in

# vim:set ts=2 sw=2 noet:
