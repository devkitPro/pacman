self.description = "Get info on package from a sync db"

sp = pmpkg("dummy")
sp.files = ["bin/dummy",
            "usr/man/man1/dummy.1"]
sp.desc = "test description"
sp.groups = ["foo"]
sp.url = "http://www.archlinux.org"
sp.license = "GPL2"
sp.arch = "i686"
# test both old style and new style dates
sp.builddate = "Mon Oct  1 01:40:21 2007 UTC"
sp.packager = "Arch Linux"
sp.md5sum = "00000000000000000000000000000000"

self.addpkg2db("sync", sp)

self.args = "-Si %s" % sp.name

self.addrule("PACMAN_RETCODE=0")
self.addrule("PACMAN_OUTPUT=%s" % sp.name)
self.addrule("PACMAN_OUTPUT=%s" % sp.desc)
self.addrule("PACMAN_OUTPUT=2007")
