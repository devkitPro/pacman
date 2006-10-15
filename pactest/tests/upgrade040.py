self.description = "file relocation 1"

lp1 = pmpkg("dummy")
lp1.files = ["bin/dummy",
             "usr/share/file"]

lp2 = pmpkg("foobar")
lp2.files = ["bin/foobar"]

for p in lp1, lp2:
	self.addpkg2db("local", p)

p1 = pmpkg("dummy")
p1.files = ["bin/dummy"]

p2 = pmpkg("foobar")
p2.files = ["bin/foobar",
            "usr/share/file"]

for p in p1, p2:
	self.addpkg(p)

self.args = "-U %s" % " ".join([p.filename() for p in p1, p2])

self.addrule("PACMAN_RETCODE=0")
