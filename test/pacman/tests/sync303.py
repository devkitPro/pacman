self.description = "Sysupgrade: upgrade pacman with deps as provisions"

sp = pmpkg("pacman", "1.0-2")
sp.depends = ["zlib"]
self.addpkg2db("sync", sp)

glibcdep = pmpkg("glibc", "2.13-1")
self.addpkg2db("sync", glibcdep)

zlibdep = pmpkg("zlib", "1.2.5-3")
zlibdep.depends = ["glibc"]
self.addpkg2db("sync", zlibdep)


lp = pmpkg("pacman", "1.0-1")
lp.depends = ["zlib"]
self.addpkg2db("local", lp)

lp2 = pmpkg("glibc-awesome", "2.13-2")
lp2.provides = ["glibc=2.13"]
lp2.conflicts = ["glibc"]
self.addpkg2db("local", lp2)

lp3 = pmpkg("zlib", "1.2.5-3")
self.addpkg2db("local", lp3)

self.option["SyncFirst"] = ["pacman"]

self.args = "-Su"

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_VERSION=pacman|1.0-2")
self.addrule("PKG_EXIST=glibc-awesome")
self.addrule("PKG_VERSION=glibc-awesome|2.13-2")
self.addrule("PKG_EXIST=zlib")
