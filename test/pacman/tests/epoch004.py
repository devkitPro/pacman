self.description = "Sysupgrade with same version, different epochs"

sp = pmpkg("dummy", "2.0-1")
sp.files = ["bin/dummynew"]
sp.epoch = 2
self.addpkg2db("sync", sp)

lp = pmpkg("dummy", "2.0-1")
lp.files = ["bin/dummyold"]
lp.force = True
self.addpkg2db("local", lp)

self.args = "-Su"

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_VERSION=dummy|2.0-1")
self.addrule("FILE_EXIST=bin/dummynew")
self.addrule("!FILE_EXIST=bin/dummyold")
self.addrule("PKG_EPOCH=dummy|2")
