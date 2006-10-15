self.description = "Install a package from a sync db, with a filesystem conflict"

sp = pmpkg("dummy")
sp.files = ["bin/dummy",
            "usr/man/man1/dummy.1"]
self.addpkg2db("sync", sp)

self.filesystem = ["bin/dummy"]

self.args = "-S dummy"

self.addrule("!PKG_EXIST=dummy")
