self.description = "Remove a package listed 5 times"

p = pmpkg("foo")
self.addpkg2db("local", p)

self.args = "-R " + "foo "*5

self.addrule("PACMAN_RETCODE=1")
self.addrule("PKG_EXISTS=foo")
