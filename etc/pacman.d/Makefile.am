pkgsysconfdir = ${sysconfdir}/pacman.d
dist_pkgsysconf_DATA = community core extra release testing unstable

$(dist_pkgsysconf_DATA): mirrorlist
	sed "s|@@REPO@@|$@|g" <$< >$@

clean-local:
	rm -f $(dist_pkgsysconf_DATA)

