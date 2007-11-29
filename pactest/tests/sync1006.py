self.description = "Conflicting package names in sync repos (diff versions)"

sp1 = pmpkg("pkg", "1.0-1")
sp1.provides = [ "provision1" ]
self.addpkg2db("sync1", sp1)

sp2 = pmpkg("pkg", "2.0-1")
sp2.provides = [ "provision2" ]
self.addpkg2db("sync2", sp2)

self.args = "-S provision1 provision2"

self.addrule("PACMAN_RETCODE=1")
self.addrule("!PKG_EXIST=pkg")
