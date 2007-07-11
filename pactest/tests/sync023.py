self.description = "Install group with two conflicting packages, one replacing other (backwards)"

sp1 = pmpkg("pkg1")
sp1.groups = ["grp"]
sp1.provides = ["pkg2"]
sp1.conflicts = ["pkg2"]
sp1.replaces = ["pkg2"]
self.addpkg2db("sync", sp1);

sp2 = pmpkg("pkg2")
sp2.groups = ["grp"]
self.addpkg2db("sync", sp2);

lp1 = pmpkg("pkg2")
lp1.groups = ["grp"]
self.addpkg2db("local", lp1);

self.args = "-S %s" % "grp"

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=pkg1");
self.addrule("!PKG_EXIST=pkg2");
