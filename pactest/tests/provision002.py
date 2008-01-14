self.description = "-S provision"

sp = pmpkg("pkg1")
sp.provides = ["provision=1.0-1"]
self.addpkg2db("sync", sp)

sp = pmpkg("pkg2")
sp.provides = ["provision=1.0-1"]
self.addpkg2db("sync", sp)

self.args = "-S provision"

self.addrule("PACMAN_RETCODE=1")
self.addrule("!PKG_EXIST=pkg1")
self.addrule("!PKG_EXIST=pkg2")
