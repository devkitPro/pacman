self.description = "Sysupgrade : pacman needs to be upgraded and has updated deps"

sp = pmpkg("pacman", "1.0-2")
sp.depends = ["zlib", "curl", "libarchive"]
self.addpkg2db("sync", sp)

libcdep = pmpkg("glibc", "2.15-1")
self.addpkg2db("sync", libcdep)

curldep = pmpkg("curl", "7.22-1")
self.addpkg2db("sync", curldep)

libadep = pmpkg("libarchive", "2.8.5-1")
self.addpkg2db("sync", libadep)

zlibdep = pmpkg("zlib", "1.2.5-3")
zlibdep.depends = ["glibc"]
self.addpkg2db("sync", zlibdep)


lp = pmpkg("pacman", "1.0-1")
self.addpkg2db("local", lp)

lp1 = pmpkg("curl", "7.21.7-1")
self.addpkg2db("local", lp1)

lp2 = pmpkg("glibc", "2.13-1")
self.addpkg2db("local", lp2)

lp3 = pmpkg("libarchive", "2.8.4-1")
self.addpkg2db("local", lp3)

lp4 = pmpkg("zlib", "1.2.5-3")
self.addpkg2db("local", lp4)

self.option["SyncFirst"] = ["pacman"]

self.args = "-Su"

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=pacman")
self.addrule("PKG_VERSION=pacman|1.0-2")
self.addrule("PKG_EXIST=glibc")
self.addrule("PKG_VERSION=glibc|2.15-1")
self.addrule("PKG_EXIST=curl")
self.addrule("PKG_VERSION=curl|7.22-1")
self.addrule("PKG_EXIST=libarchive")
self.addrule("PKG_VERSION=libarchive|2.8.5-1")
self.addrule("PKG_EXIST=zlib")
