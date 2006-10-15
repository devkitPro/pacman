self.description = "Install a package from a sync db"

sp = pmpkg("dummy")
sp.files = ["bin/dummy",
            "usr/man/man1/dummy.1"]
self.addpkg2db("sync", sp)

self.args = "-S dummy"

self.addrule("PKG_EXIST=dummy")
for f in sp.files:
	self.addrule("FILE_EXIST=%s" % f)
