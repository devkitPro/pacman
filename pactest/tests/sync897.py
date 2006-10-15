self.description = "System upgrade"

sp1 = pmpkg("pkg1", "1.0-2")
sp1.conflicts = ["pkg2"]
sp1.provides = ["pkg2"]
self.addpkg2db("sync", sp1);

sp2 = pmpkg("pkg2", "1.0-2")
self.addpkg2db("sync", sp2)

lp1 = pmpkg("pkg1")
lp1.conflicts = ["pkg2"]
self.addpkg2db("local", lp1)

lp2 = pmpkg("pkg2")
self.addpkg2db("local", lp2)

lp3 = pmpkg("pkg3")
lp3.conflicts = ["pkg1"]
self.addpkg2db("local", lp3)

self.args = "-Su"

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=pkg1")
self.addrule("!PKG_EXIST=pkg2")
