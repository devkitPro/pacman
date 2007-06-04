EXTRA_DIST = \
	abs.conf \
	supfile.arch \
	supfile.community \
	supfile.extra \
	supfile.testing \
	supfile.unstable

install-data-hook:
	mkdir -p $(DESTDIR)$(sysconfdir)/abs ; \
	for j in $(EXTRA_DIST); do \
		$(INSTALL) -c -m 644 $(srcdir)/$$j $(DESTDIR)$(sysconfdir)/abs/$$j ; \
	done

uninstall-hook:
	for j in $(EXTRA_DIST); do \
		rm -f $(DESTDIR)$(sysconfdir)/abs/$$j ; \
	done

# vim:set ts=2 sw=2 noet:
