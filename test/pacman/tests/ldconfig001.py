# quick note here - chroot() is expected to fail.  We're not checking the
# validity of the scripts, only that they fire (or try to)
self.description = "Make sure ldconfig runs on an add operation"

p = pmpkg("dummy")
self.addpkg(p)

# --debug is necessary to check PACMAN_OUTPUT
self.args = "--debug -U %s" % p.filename()

self.addrule("PACMAN_RETCODE=0")
self.addrule("PACMAN_OUTPUT=ldconfig")
