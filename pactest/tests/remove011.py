self.description = "Remove a package, with a modified file marked for backup"

p1 = pmpkg("dummy")
p1.files = ["etc/dummy.conf*"]
p1.backup = ["etc/dummy.conf"]
self.addpkg2db("local", p1)

self.args = "-R dummy"

self.addrule("!PKG_EXIST=dummy")
self.addrule("!FILE_EXIST=etc/dummy.conf")
self.addrule("FILE_PACSAVE=etc/dummy.conf")
