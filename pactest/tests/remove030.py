self.description = "Remove a package in HoldPkg"

p1 = pmpkg("dummy")
self.addpkg2db("local", p1)

self.option["holdpkg"] = ["dummy"]

self.args = "-R dummy"

self.addrule("!PKG_EXIST=dummy")
self.addrule("!FILE_EXIST=etc/dummy.conf")
self.addrule("!FILE_PACSAVE=etc/dummy.conf")
