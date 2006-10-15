self.description = "Install a package (already installed)"

lp = pmpkg("dummy")
lp.files = ["bin/dummy",
            "usr/man/man1/dummy.1"]
self.addpkg2db("local", lp)

p = pmpkg("dummy")
p.files = ["bin/dummy",
           "usr/man/man1/dummy.1"]
self.addpkg(p)

self.args = "-A %s" % p.filename()

self.addrule("PACMAN_RETCODE=1")
self.addrule("!PKG_MODIFIED=dummy")
for f in lp.files:
	self.addrule("!FILE_MODIFIED=%s" % f)
