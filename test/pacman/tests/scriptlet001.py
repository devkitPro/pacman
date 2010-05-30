self.description = "Scriptlet test (pre/post install)"

p1 = pmpkg("dummy")
p1.files = ['etc/dummy.conf']
pre = "OUTPUT FROM PRE_INSTALL"
post = "OUTPUT FROM POST_INSTALL"
p1.install['pre_install'] = "echo " + pre
p1.install['post_install'] = "echo " + post
self.addpkg(p1)

# --debug is necessary to check PACMAN_OUTPUT
self.args = "--debug -U %s" % p1.filename()

self.addrule("PACMAN_RETCODE=0")
self.addrule("PACMAN_OUTPUT=" + pre)
self.addrule("PACMAN_OUTPUT=" + post)

fakechroot = which("fakechroot")
if not fakechroot:
	self.expectfailure = True
