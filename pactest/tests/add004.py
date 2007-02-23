self.description = "Install three of the same package at different versions"

p1 = pmpkg("dummy", "1.0-2")
p1.files = ["bin/dummy"]
p2 = pmpkg("dummy", "2.0-1")
p2.files = ["bin/dummy"]
p3 = pmpkg("dummy")
p3.files = ["bin/dummy"]

for p in p1, p2, p3:
	self.addpkg(p)

self.args = "-A %s" % " ".join([p.filename() for p in p1, p2, p3])

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_VERSION=dummy|2.0-1")
for f in p2.files:
	self.addrule("FILE_EXIST=%s" % f)
