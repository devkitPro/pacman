self.description = "Remove a package with an empty directory needed by another package"

p1 = pmpkg("pkg1")
p1.files = ["bin/pkg1", "opt/"]

p2 = pmpkg("pkg2")
p2.files = ["bin/pkg2", "opt/"]

for p in p1, p2:
	self.addpkg2db("local", p)

self.args = "-R %s" % p1.name

self.addrule("PACMAN_RETCODE=0")
self.addrule("!PKG_EXIST=pkg1")
self.addrule("PKG_EXIST=pkg2")
self.addrule("!FILE_EXIST=bin/pkg1")
self.addrule("FILE_EXIST=bin/pkg2")
self.addrule("FILE_EXIST=opt/")

self.expectfailure = True
