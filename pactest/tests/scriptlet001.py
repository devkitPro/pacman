# quick note here - chroot() is expected to fail.  We're not checking the
# validity of the scripts, only that they fire (or try to)
self.description = "Scriptlet test (pre/post install)"

lpsh = pmpkg("sh")
lpsh.files = ['bin/sh']
self.addpkg2db("local", lpsh)

p1 = pmpkg("dummy")
p1.files = ['etc/dummy.conf']
p1.install['pre_install'] = "ls /etc";
p1.install['post_install'] = "ls /etc";
self.addpkg(p1)

# --debug is necessary to check PACMAN_OUTPUT
self.args = "--debug -U %s" % p1.filename()

self.addrule("PACMAN_RETCODE=0")
self.addrule("FILE_EXIST=bin/sh")
self.addrule("PACMAN_OUTPUT=pre_install")
self.addrule("PACMAN_OUTPUT=post_install")
