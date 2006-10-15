self.description = "Install a set of the same package at different versions"

p1 = pmpkg("dummy", "1.0-2")
p2 = pmpkg("dummy", "2.0-1")
p3 = pmpkg("dummy")
for p in p1, p2, p3:
	self.addpkg(p)

self.args = "-A %s" % " ".join([p.filename() for p in p1, p2, p3])

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_VERSION=dummy|2.0-1")
