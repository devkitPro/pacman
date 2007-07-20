self.description = "Test -Se (resolve the dependencies' dependencies )"

sp1 = pmpkg("pkg1")
sp1.depends = [ "pkg2" ]
self.addpkg2db("sync", sp1)

sp2 = pmpkg("pkg2")
sp2.depends = [ "pkg3" ]
self.addpkg2db("sync", sp2)

sp3 = pmpkg("pkg3")
self.addpkg2db("sync", sp3)

self.args = "-Se pkg1 pkg3"

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=pkg2")
self.addrule("PKG_EXIST=pkg3")
self.addrule("!PKG_EXIST=pkg1")
