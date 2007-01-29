pkgsysconfdir = ${sysconfdir}/pacman.d
dist_pkgsysconf_DATA = community current extra release unstable

$(dist_pkgsysconf_DATA): mirrorlist
	sed "s|@@REPO@@|$@|g" <$< >$@

clean:
	rm $(dist_pkgsysconf_DATA)

