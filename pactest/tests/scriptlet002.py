# quick note here - chroot() is expected to fail.  We're not checking the
# validity of the scripts, only that they fire (or try to)
self.description = "Scriptlet test (pre/post remove)"

lpsh = pmpkg("sh")
lpsh.files = ['bin/sh']
self.addpkg2db("local", lpsh)

p1 = pmpkg("dummy")
p1.files = ['etc/dummy.conf']
p1.install['pre_remove'] = "ls /etc";
p1.install['post_remove'] = "ls /etc";
self.addpkg2db("local", p1)

# --debug is necessary to check PACMAN_OUTPUT
self.args = "--debug -R %s" % p1.name

self.addrule("PACMAN_RETCODE=0")
self.addrule("FILE_EXIST=bin/sh")
self.addrule("PACMAN_OUTPUT=pre_remove")
self.addrule("PACMAN_OUTPUT=post_remove")
