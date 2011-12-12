# enforce that all scripts have a --help and --version option
AUTOMAKE_OPTIONS = std-options

bin_SCRIPTS = \
  $(OURSCRIPTS)

OURSCRIPTS = \
	bacman \
	paccache \
	pacdiff \
	paclist \
	paclog-pkglist \
	pacscripts \
	pacsearch \
	pacsysclean

OURFILES = \
	bash_completion \
	zsh_completion

EXTRA_DIST = \
	PKGBUILD.vim \
	bacman.in \
	bash_completion.in \
	paccache.in \
	paclog-pkglist.in \
	pacdiff.in \
	paclist.in \
	pacscripts.in \
	pacsearch.in \
	pacsysclean.in \
	vimprojects \
	zsh_completion.in \
	README

# Files that should be removed, but which Automake does not know.
MOSTLYCLEANFILES = $(OURSCRIPTS) $(OURFILES) *.tmp

if USE_GIT_VERSION
GIT_VERSION := $(shell sh -c 'git describe --abbrev=4 --dirty | sed s/^v//')
REAL_PACKAGE_VERSION = $(GIT_VERSION)
else
REAL_PACKAGE_VERSION = $(PACKAGE_VERSION)
endif

edit = sed \
	-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
	-e 's|@localstatedir[@]|$(localstatedir)|g' \
	-e 's|@PACKAGE_VERSION[@]|$(REAL_PACKAGE_VERSION)|g' \
	-e 's|@SIZECMD[@]|$(SIZECMD)|g' \
	-e '1s|!/bin/bash|!$(BASH_SHELL)|g'

$(OURSCRIPTS): Makefile
	$(AM_V_at)$(RM) $@ $@.tmp
	$(AM_V_GEN)$(edit) $(srcdir)/$@.in >$@.tmp
	$(AM_V_at)chmod +x,a-w $@.tmp
	$(AM_V_at)mv $@.tmp $@

$(OURFILES): Makefile
	$(AM_V_at)$(RM) $@ $@.tmp
	$(AM_V_GEN)$(edit) $(srcdir)/$@.in >$@.tmp
	$(AM_V_at)chmod a-w $@.tmp
	$(AM_V_at)mv $@.tmp $@

all-am: $(OURSCRIPTS) $(OURFILES)

bacman: $(srcdir)/bacman.in
bash_completion: $(srcdir)/bash_completion.in
paccache: $(srcdir)/paccache.in
pacdiff: $(srcdir)/pacdiff.in
paclist: $(srcdir)/paclist.in
paclog-pkglist: $(srcdir)/paclog-pkglist.in
pacscripts: $(srcdir)/pacscripts.in
pacsearch: $(srcdir)/pacsearch.in
pacsysclean: $(srcdir)/pacsysclean.in
pactree: $(srcdir)/pactree.in
zsh_completion: $(srcdir)/zsh_completion.in

# vim:set ts=2 sw=2 noet:
