self.description = "Sysupgrade : pacman needs to be upgraded and has new deps"

sp = pmpkg("pacman", "1.0-2")
sp.depends = ["dep"]
self.addpkg2db("sync", sp)

spdep = pmpkg("dep")
self.addpkg2db("sync", spdep)

sp1 = pmpkg("pkg1", "1.0-2")
self.addpkg2db("sync", sp1)

lp = pmpkg("pacman", "1.0-1")
self.addpkg2db("local", lp)

lp1 = pmpkg("pkg1", "1.0-1")
self.addpkg2db("local", lp1)

self.option["SyncFirst"] = ["pacman"]

self.args = "-Su"

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=pacman")
self.addrule("PKG_VERSION=pacman|1.0-2")
self.addrule("PKG_VERSION=pkg1|1.0-1")
self.addrule("PKG_EXIST=dep")
