self.description = "Install a thousand packages in a single transaction"

p = pmpkg("pkg1000")

self.addpkg2db("local", p)

for i in range(1000):
	p = pmpkg("pkg%03d" % i)
	p.depends = ["pkg%03d" % (i+1)]
	p.files = ["usr/share/pkg%03d" % i]
	self.addpkg(p)

_list = []
[_list.append(p.filename()) for p in self.localpkgs]
self.args = "-A %s" % " ".join(_list)

self.addrule("PACMAN_RETCODE=0")
#for i in range(1000):
#	self.addrule("PKG_EXIST=pkg%03d" %i)
