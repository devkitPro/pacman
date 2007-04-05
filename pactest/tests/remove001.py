# If someone else can come up with a better name, please do so
self.description = "Remove a package listed 5 times"

p = pmpkg("foo")
self.addpkg2db("local", p)

self.args = "-R " + "foo "*5

self.addrule("PACMAN_RETCODE=0")
self.addrule("!PKG_EXISTS=foo")
