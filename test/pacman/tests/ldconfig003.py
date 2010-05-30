# quick note here - chroot() is expected to fail.  We're not checking the
# validity of the scripts, only that they fire (or try to)
self.description = "Make sure ldconfig runs on a sync operation"

sp = pmpkg("dummy")
self.addpkg2db("sync", sp)

self.args = "-S %s" % sp.name

# --debug is necessary to check PACMAN_OUTPUT
self.args = "--debug -S %s" % sp.name

self.addrule("PACMAN_RETCODE=0")
self.addrule("PACMAN_OUTPUT=ldconfig")
