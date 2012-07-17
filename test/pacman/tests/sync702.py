self.description = "do not remove directory symlink if incoming package has file in its path (order 2)"

lp = pmpkg("pkg2")
lp.files = ["usr/lib/foo",
            "lib -> usr/lib"]
self.addpkg2db("local", lp)

p1 = pmpkg("pkg1")
p1.files = ["lib/bar"]
self.addpkg2db("sync", p1)

p2 = pmpkg("pkg2", "1.0-2")
p2.files = ["usr/lib/foo"]
self.addpkg2db("sync", p2)

self.args = "-S pkg1 pkg2"

self.addrule("PACMAN_RETCODE=1")
self.addrule("PKG_VERSION=pkg2|1.0-1")
self.addrule("!PKG_EXIST=pkg1")

self.expectfailure = True
