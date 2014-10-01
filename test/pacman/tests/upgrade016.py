self.description = "Install a package with an existing file (--force, new modified)"

p = pmpkg("dummy")
p.files = ["etc/dummy.conf*"]
p.backup = ["etc/dummy.conf"]
self.addpkg(p)

self.filesystem = ["etc/dummy.conf"]

self.args = "-U --force %s" % p.filename()

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=dummy")
self.addrule("!FILE_MODIFIED=etc/dummy.conf")
self.addrule("FILE_PACNEW=etc/dummy.conf")
