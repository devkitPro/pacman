self.description = "Sysupgrade with same version for local and sync packages"

sp = pmpkg("dummy")
lp = pmpkg("dummy")

self.addpkg2db("sync", sp)
self.addpkg2db("local", lp)

self.args = "-Su"

self.addrule("PACMAN_RETCODE=0")
self.addrule("!PKG_MODIFIED=dummy")
