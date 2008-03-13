self.description = "Remove a package in HoldPkg"

p1 = pmpkg("dummy")
self.addpkg2db("local", p1)

self.option["HoldPkg"] = ["dummy"]

self.args = "-R %s" % p1.name

self.addrule("PACMAN_RETCODE=0")
self.addrule("!PKG_EXIST=dummy")
self.addrule("!FILE_EXIST=etc/dummy.conf")
self.addrule("!FILE_PACSAVE=etc/dummy.conf")
