self.description = "Verify a signature in a sync DB (failure)"

sp = pmpkg("pkg1")
sp.pgpsig = "iEYEABECAAYFAkhMOggACgkQXC5GoPU6du2WVQCffVxF8GKXJIY4juJBIw/ljLrQxygAnj2QlvsUd7MdFekLX18+Ov/xzgZ1"
self.addpkg2db("sync+Always", sp)

self.args = "-S %s" % sp.name

self.addrule("PACMAN_RETCODE=1")
self.addrule("!PKG_EXIST=pkg1")
