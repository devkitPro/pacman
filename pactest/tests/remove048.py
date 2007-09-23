self.description = "Remove a package with a broken required by"

lp1 = pmpkg("pkg1")
lp1.requiredby = [ "dep" ]
self.addpkg2db("local", lp1)

self.args = "-R %s" % lp1.name

self.addrule("PACMAN_RETCODE=0")
self.addrule("!PKG_EXIST=pkg1")
