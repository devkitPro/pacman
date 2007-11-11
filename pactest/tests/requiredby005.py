self.description = "Remove lp1, requiredby should move from lp1 to lp2"

lp1 = pmpkg("foo")
lp1.requiredby = ["pkg3"]
self.addpkg2db("local", lp1)

lp2 = pmpkg("pkg2")
lp2.provides = ["foo"]
self.addpkg2db("local", lp2)

lp3 = pmpkg("pkg3")
lp3.depends = ["foo"]
self.addpkg2db("local", lp3)


self.args = "-R %s" % lp1.name

self.addrule("PACMAN_RETCODE=0")
self.addrule("!PKG_EXIST=foo")
self.addrule("PKG_EXIST=pkg2")
self.addrule("PKG_REQUIREDBY=pkg2|pkg3")
