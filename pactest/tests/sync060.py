self.description = "Replace a package with a broken required by"

lp1 = pmpkg("pkg1")
lp1.replaces = [ "pkg2" ]
self.addpkg2db("sync", lp1)

lp2 = pmpkg("pkg2")
lp2.requiredby = [ "fake" ]
self.addpkg2db("local", lp2)

self.args = "-Su"

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=pkg1")
self.addrule("!PKG_EXIST=pkg2")
