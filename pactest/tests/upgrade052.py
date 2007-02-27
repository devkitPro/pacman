self.description = "Nagy's bug (I think) -- in progress"

lp1 = pmpkg("pkg1")
lp1.depends = ["imaginary"]
self.addpkg2db("local", lp1)

lp2 = pmpkg("pkg2")
lp2.provides = ["imaginary", "real"]
self.addpkg2db("local", lp2)

p = pmpkg("pkg2", "1.0-2")
p.provides = ["real"]
self.addpkg(p)

self.args = "-U %s" % p.filename()

self.addrule("PACMAN_RETCODE=1")
self.addrule("PKG_EXIST=pkg1")
self.addrule("PKG_VERSION=pkg2|1.0-1")
