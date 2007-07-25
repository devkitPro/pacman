self.description = "Induced removal would break dependency"

sp1 = pmpkg("pkg1", "1.0-2")
sp1.replaces = [ "pkg2" ]
self.addpkg2db("sync", sp1)

sp2 = pmpkg("pkg2", "1.0-2")
self.addpkg2db("sync", sp2)

sp3 = pmpkg("pkg3", "1.0-2")
sp3.depends = ["pkg2=1.0-2"]
self.addpkg2db("sync", sp3)

lp1 = pmpkg("pkg1", "1.0-1")
self.addpkg2db("local", lp1)

lp2 = pmpkg("pkg2", "1.0-2")
self.addpkg2db("local", lp2)

lp3 = pmpkg("pkg3", "1.0-1")
self.addpkg2db("local", lp3)

self.args = "-Su"

self.addrule("PACMAN_RETCODE=1")
self.addrule("PKG_EXIST=pkg2")
