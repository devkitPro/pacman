self.description = "Test -Se (ensure specified package is not installed)"

sp1 = pmpkg("dummy")
sp1.depends = [ "dep1", "dep2" ]
self.addpkg2db("sync", sp1)

sp2 = pmpkg("dep1")
self.addpkg2db("sync", sp2)

sp3 = pmpkg("dep2")
self.addpkg2db("sync", sp3)

self.args = "-Se dummy"

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=dep1")
self.addrule("PKG_EXIST=dep2")
self.addrule("!PKG_EXIST=dummy")
