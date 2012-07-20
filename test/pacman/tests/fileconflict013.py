self.description = "file->file path change with same effective path (/lib as symlink)"

lp1 = pmpkg("filesystem", "1.0-1")
lp1.files = ["usr/",
             "usr/lib/",
             "lib -> usr/lib/"]
self.addpkg2db("local", lp1)

lp2 = pmpkg("pkg1", "1.0-1")
lp2.files = ["lib/libfoo.so"]
self.addpkg2db("local", lp2)

sp1 = pmpkg("pkg1", "1.0-2")
sp1.files = ["usr/lib/libfoo.so"]
self.addpkg2db("sync", sp1)

self.args = "-Su"

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_VERSION=pkg1|1.0-2")
