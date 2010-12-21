self.description = "Install packages with huge descriptions"

p1 = pmpkg("pkg1")
p1.desc = 'A' * 500 * 1024
self.addpkg(p1)

p2 = pmpkg("pkg2")
p2.desc = 'A' * 600 * 1024
self.addpkg(p2)

self.args = "-U %s %s" % (p1.filename(), p2.filename())

# Note that the current cutoff on line length is 512K, so the first package
# will succeed while the second one will fail to record the description.
self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=pkg1")
self.addrule("PKG_DESC=pkg1|%s" % p1.desc)
self.addrule("PKG_EXIST=pkg1")
self.addrule("!PKG_DESC=pkg1|%s" % p2.desc)
