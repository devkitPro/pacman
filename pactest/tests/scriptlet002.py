self.description = "Scriptlet test (pre/post remove)"

p1 = pmpkg("dummy")
p1.files = ['etc/dummy.conf']
pre = "OUTPUT FROM PRE_REMOVE";
post = "OUTPUT FROM POST_REMOVE";
p1.install['pre_remove'] = "echo " + pre
p1.install['post_remove'] = "echo " + post
self.addpkg2db("local", p1)

# --debug is necessary to check PACMAN_OUTPUT
self.args = "--debug -R %s" % p1.name

self.addrule("PACMAN_RETCODE=0")
self.addrule("PACMAN_OUTPUT=" + pre)
self.addrule("PACMAN_OUTPUT=" + post)

fakechroot = which("fakechroot")
if not fakechroot:
	self.expectfailure = True
