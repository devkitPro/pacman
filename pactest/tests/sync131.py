self.description = "Sysupgrade with a sync package replacing a set of local ones"

sp = pmpkg("pkg4")
sp.replaces = ["pkg1", "pkg2", "pkg3"]

self.addpkg2db("sync", sp)

lp1 = pmpkg("pkg1")
lp2 = pmpkg("pkg2")

for p in lp1, lp2:
	self.addpkg2db("local", p)

self.args = "-Su"

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=pkg4")
for p in lp1, lp2:
	self.addrule("!PKG_EXIST=%s" % p.name)
