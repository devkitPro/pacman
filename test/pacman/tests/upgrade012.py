self.description = "Install a package with a filesystem conflict (--force)"

p = pmpkg("dummy")
p.files = ["bin/dummy", "usr/man/man1/dummy.1"]
self.addpkg(p)

self.filesystem = ["bin/dummy"]

self.args = "-U --force %s" % p.filename()

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=dummy")
self.addrule("FILE_MODIFIED=bin/dummy")
self.addrule("FILE_EXIST=usr/man/man1/dummy.1")
