self.description = "server failover after 404"
self.require_capability("curl")

p1 = pmpkg('pkg')
self.addpkg2db('sync', p1)

url_broke = self.add_simple_http_server({
    '/{}'.format(p1.filename()): {
        'code': 404,
        'body': 'a',
    }
})
url_good = self.add_simple_http_server({
    '/{}'.format(p1.filename()): p1.makepkg_bytes(),
})

self.db['sync'].option['Server'] = [ url_broke, url_good ]
self.db['sync'].syncdir = False
self.cachepkgs = False

self.args = '-S pkg'

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=pkg")
