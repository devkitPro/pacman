self.description = "test deptest (-T) functionality"

sp1 = pmpkg("pkg1")
sp1.depends = ["dep"]
self.addpkg2db("sync", sp1)

sp1dep = pmpkg("dep")
self.addpkg2db("sync", sp1dep)

sp2 = pmpkg("pkg2")
self.addpkg2db("sync", sp2)

lp2 = pmpkg("pkg2")
self.addpkg2db("local", lp2)

self.args = "-T pkg1 pkg2"

self.addrule("PACMAN_RETCODE=127")
self.addrule("PACMAN_OUTPUT=pkg1")
self.addrule("!PACMAN_OUTPUT=pkg2")
