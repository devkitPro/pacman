self.description = "package name with invalid characters cannot be installed (file)"

p = pmpkg("-foo")
self.addpkg(p)

self.args = "-U -- %s" % p.filename()

self.addrule("!PACMAN_RETCODE=0")
self.addrule("!PKG_EXIST=-foo")
