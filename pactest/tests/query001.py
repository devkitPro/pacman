self.description = "Query a package"

p = pmpkg("foobar")
p.files = ["bin/foobar"]
self.addpkg2db("local", p)

self.args = "-Q foobar"

self.addrule("PACMAN_OUTPUT=foobar")
