self.description = "Install packages from a sync db with circular dependencies"

sp1 = pmpkg("pkg1")
sp1.depends = ["pkg2"]

sp2 = pmpkg("pkg2")
sp2.depends = ["pkg3"]

sp3 = pmpkg("pkg3")
sp3.depends = ["pkg1"]

for p in sp1, sp2, sp3:
	self.addpkg2db("sync", p);

self.args = "-S %s" % sp1.name

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=pkg1")
self.addrule("PKG_EXIST=pkg2")
self.addrule("PKG_EXIST=pkg3")
