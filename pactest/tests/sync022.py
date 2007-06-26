self.description = "Installs a group with two conflicting packages, one replacing the other"

sp1 = pmpkg("pkg1")
sp1.groups = ["grp"]
self.addpkg2db("sync", sp1);

sp2 = pmpkg("pkg2")
sp2.groups = ["grp"]
sp2.provides = ["pkg1"]
sp2.conflicts = ["pkg1"]
sp2.replaces = ["pkg1"]
self.addpkg2db("sync", sp2);

lp1 = pmpkg("pkg2")
lp1.groups = ["grp"]
self.addpkg2db("local", lp1);

self.args = "-S %s" % "grp"

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=pkg2");
self.addrule("!PKG_EXIST=pkg1");
