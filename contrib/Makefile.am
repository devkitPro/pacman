OURFILES = \
	PKGBUILD.vim \
	bacman \
	bash_completion \
	pacdiff \
	paclist \
	pacscripts \
	pacsearch \
	pactree \
	vimprojects \
	wget-xdelta.sh \
	zsh_completion

EXTRA_DIST = \
	PKGBUILD.vim.in \
	bacman.in \
	bash_completion.in \
	pacdiff.in \
	paclist.in \
	pacscripts.in \
	pacsearch.in \
	pactree.in \
	vimprojects.in \
	wget-xdelta.sh.in \
	zsh_completion.in \
	README

# Files that should be removed, but which Automake does not know.
MOSTLYCLEANFILES = $(OURFILES) *.tmp

edit = sed \
	-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
	-e 's|@localstatedir[@]|$(localstatedir)|g' \
	-e 's|@BASH[@]|$(BASH)|g'

$(OURFILES): Makefile
	@rm -f $@ $@.tmp
	@cp -a $@.in $@.tmp
	$(edit) $@.in >$@.tmp
	@mv $@.tmp $@

all-am: $(OURFILES)

# vim:set ts=2 sw=2 noet:
