self.description = "Install a virtual target (provided by a sync package)"

sp1 = pmpkg("pkg1")
sp1.provides = ["pkg2"]
self.addpkg2db("sync", sp1);

self.args = "-S pkg2"

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=pkg1")
