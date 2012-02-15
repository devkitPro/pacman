self.description = "Sysupgrade: upgrade pacman being depended on"

sp = pmpkg("pacman", "4.0.1-1")
self.addpkg2db("sync", sp)

sp2 = pmpkg("pyalpm", "2-1")
sp2.depends = ["pacman>=4.0", "pacman<4.1"]
self.addpkg2db("sync", sp2)

lp = pmpkg("pacman", "3.5.4-1")
self.addpkg2db("local", lp)

lp2 = pmpkg("pyalpm", "1-1")
lp2.depends = ["pacman>=3.5", "pacman<3.6"]
self.addpkg2db("local", lp2)

self.option["SyncFirst"] = ["pacman"]

self.args = "-Su"

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_VERSION=pacman|4.0.1-1")
self.addrule("PKG_VERSION=pyalpm|2-1")

self.expectfailure = True
