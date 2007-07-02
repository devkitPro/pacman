self.description = "Check the types of default files in a package"

p = pmpkg("pkg1")
p.files = ["bin/foo"
           "bin/bar"]
self.addpkg(p)

self.args = "-U %s" % p.filename()

self.addrule("PACMAN_RETCODE=0")
for f in p.files:
	self.addrule("FILE_TYPE=%s|file" % f)
self.addrule("FILE_TYPE=bin/|dir")
