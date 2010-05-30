self.description = "Sysupgrade with a newer local package"

sp = pmpkg("dummy", "0.9-1")
lp = pmpkg("dummy")

self.addpkg2db("sync", sp)
self.addpkg2db("local", lp)

self.args = "-Su"

self.addrule("PACMAN_RETCODE=0")
self.addrule("!PKG_MODIFIED=dummy")
