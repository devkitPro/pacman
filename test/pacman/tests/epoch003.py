self.description = "Sysupgrade with an epoch package overriding a force package"

sp = pmpkg("dummy", "1.4-1")
sp.epoch = 2
self.addpkg2db("sync", sp)

lp = pmpkg("dummy", "2.0-1")
lp.force = True
self.addpkg2db("local", lp)

self.args = "-Su"

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_VERSION=dummy|1.4-1")
self.addrule("PKG_EPOCH=dummy|2")
