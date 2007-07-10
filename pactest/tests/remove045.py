self.description = "-Rs advanced test"

lp1 = pmpkg("pkg1")
lp1.depends = ["pkg2" , "pkg3"]
self.addpkg2db("local", lp1)

lp2 = pmpkg("pkg2")
lp2.reason = 1
lp2.depends = ["pkg4"]
lp2.requiredby = ["pkg1"]
self.addpkg2db("local", lp2)

lp3 = pmpkg("pkg3")
lp3.reason = 1
lp3.requiredby = ["pkg1", "pkg4"]
self.addpkg2db("local", lp3)

lp4 = pmpkg("pkg4")
lp4.reason = 1
lp4.depends = ["pkg3"]
lp4.requiredby = ["pkg2"]
self.addpkg2db("local", lp4)

self.args = "-Rs %s" % lp1.name

self.addrule("PACMAN_RETCODE=0")
self.addrule("!PKG_EXIST=pkg1")
self.addrule("!PKG_EXIST=pkg2")
self.addrule("!PKG_EXIST=pkg3")
self.addrule("!PKG_EXIST=pkg4")
