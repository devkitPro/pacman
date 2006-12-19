EXTRA_DIST = community current extra release unstable

install-data-hook:
	mkdir -p $(DESTDIR)$(sysconfdir)/pacman.d ; \
	for j in $(EXTRA_DIST); do \
	    $(INSTALL) -c -m 644 $(srcdir)/$$j $(DESTDIR)$(sysconfdir)/pacman.d/$$j ; \
	done

uninstall-hook:
	for j in $(EXTRA_DIST); do \
	    rm -f $(DESTDIR)$(sysconfdir)/pacman.d/$$j ; \
	done
