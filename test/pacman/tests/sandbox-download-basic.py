self.description = "--sync with DownloadUser set"
self.require_capability("curl")

p1 = pmpkg('pkg1', '1.0-1')
self.addpkg2db('sync', p1)

url = self.add_simple_http_server({
    '/{}'.format(p1.filename()): p1.makepkg_bytes(),
})

self.option['DownloadUser'] = ['root']
self.db['sync'].option['Server'] = [ url ]
self.db['sync'].syncdir = False
self.cachepkgs = False

self.args = '-S pkg1'

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=pkg1")
self.addrule("CACHE_EXISTS=pkg1|1.0-1")
