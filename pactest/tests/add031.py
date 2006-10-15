self.description = "Freshen a package (installed is newer)"

lp = pmpkg("dummy", "1.0-2")
lp.files = ["bin/dummy",
            "usr/man/man1/dummy.1"]
self.addpkg2db("local", lp)

p = pmpkg("dummy")
p.files = ["bin/dummy",
           "usr/man/man1/dummy.1"]
self.addpkg(p)

self.args = "-F %s" % p.filename()

self.addrule("PACMAN_RETCODE=1")
self.addrule("!PKG_MODIFIED=dummy")
for f in p.files:
	self.addrule("!FILE_MODIFIED=%s" % f)
