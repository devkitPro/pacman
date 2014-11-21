# enforce that all scripts have a --help and --version option
AUTOMAKE_OPTIONS = std-options

bin_SCRIPTS = \
  $(OURSCRIPTS)

BASHSCRIPTS = \
	bacman \
	checkupdates \
	paccache \
	pacdiff \
	paclist \
	paclog-pkglist \
	pacscripts \
	rankmirrors \
	updpkgsums

OTHERSCRIPTS = \
	pacsearch

OURSCRIPTS = \
	$(BASHSCRIPTS) \
	$(OTHERSCRIPTS)

OURFILES = \
	bash_completion \
	zsh_completion

EXTRA_DIST = \
	PKGBUILD.vim \
	bacman.sh.in \
	bash_completion.in \
	checkupdates.sh.in \
	paccache.sh.in \
	paclog-pkglist.sh.in \
	pacdiff.sh.in \
	paclist.sh.in \
	pacscripts.sh.in \
	pacsearch.in \
	rankmirrors.sh.in \
	updpkgsums.sh.in \
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
	-e 's|@SCRIPTNAME[@]|$@|g' \
	-e '1s|!/bin/bash|!$(BASH_SHELL)|g'

$(OTHERSCRIPTS): Makefile
	$(AM_V_at)$(RM) $@ $@.tmp
	$(AM_V_GEN)$(edit) $(srcdir)/$@.in >$@.tmp
	$(AM_V_at)chmod +x,a-w $@.tmp
	$(AM_V_at)mv $@.tmp $@

$(BASHSCRIPTS): Makefile
	$(AM_V_at)$(RM) $@
	$(AM_V_GEN)test -f $(srcdir)/$@.sh.in && m4 -P -I $(srcdir) $(srcdir)/$@.sh.in | $(edit) >$@
	$(AM_V_at)chmod +x,a-w $@
	@$(BASH_SHELL) -O extglob -n $@

$(OURFILES): Makefile
	$(AM_V_at)$(RM) $@ $@.tmp
	$(AM_V_GEN)$(edit) $(srcdir)/$@.in >$@.tmp
	$(AM_V_at)chmod a-w $@.tmp
	$(AM_V_at)mv $@.tmp $@

all-am: $(OURSCRIPTS) $(OURFILES)

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(sysconfdir)/bash_completion.d/
	$(INSTALL_DATA) bash_completion $(DESTDIR)$(sysconfdir)/bash_completion.d/pacman
	$(MKDIR_P) $(DESTDIR)$(datarootdir)/zsh/site-functions/
	$(INSTALL_DATA) zsh_completion $(DESTDIR)$(datarootdir)/zsh/site-functions/_pacman

uninstall-local:
	$(RM) $(DESTDIR)$(sysconfdir)/bash_completion.d/pacman
	$(RM) $(DESTDIR)$(datarootdir)/zsh/site-functions/_pacman

bacman: $(srcdir)/bacman.sh.in
bash_completion: $(srcdir)/bash_completion.in
checkupdates: $(srcdir)/checkupdates.sh.in
paccache: $(srcdir)/paccache.sh.in $(top_srcdir)/scripts/library/parseopts.sh $(top_srcdir)/scripts/library/size_to_human.sh
pacdiff: $(srcdir)/pacdiff.sh.in
paclist: $(srcdir)/paclist.sh.in
paclog-pkglist: $(srcdir)/paclog-pkglist.sh.in
pacscripts: $(srcdir)/pacscripts.sh.in
pacsearch: $(srcdir)/pacsearch.in
rankmirrors: $(srcdir)/rankmirrors.sh.in
updpkgsums: $(srcdir)/updpkgsums.sh.in
zsh_completion: $(srcdir)/zsh_completion.in

# vim:set noet:
