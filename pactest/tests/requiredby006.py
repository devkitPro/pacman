self.description = "Broken requiredby/depends list"

lp1 = pmpkg("pkg1")
lp1.depends = ["pkg2"]
self.addpkg2db("local", lp1)

lp2 = pmpkg("pkg2")
lp2.requiredby = ["foo", "pkg1"]
self.addpkg2db("local", lp2)

p = pmpkg("pkg1", "1.1-1")
p.depends = ["pkg2"]
self.addpkg(p)

self.args = "-U %s" % p.filename()

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=pkg1")
self.addrule("PKG_VERSION=pkg1|1.1-1")
self.addrule("PKG_EXIST=pkg2")
self.addrule("PKG_REQUIREDBY=pkg2|pkg1")
