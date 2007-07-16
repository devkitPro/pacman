self.description = "Cascade remove a package with a broken required by"

lp1 = pmpkg("pkg1")
lp1.requiredby = [ "fake" ]
self.addpkg2db("local", lp1)

self.args = "-Rc %s" % lp1.name

self.addrule("PACMAN_RETCODE=0")
self.addrule("!PKG_EXIST=pkg1")
