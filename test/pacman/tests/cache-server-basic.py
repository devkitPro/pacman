self.description = "basic cache server test"
self.require_capability("curl")

pkgs = [ self.addpkg2db('sync', pmpkg("pkg{}".format(i))) for i in range(0, 5) ]

# TODO: hack to prevent pacman trying to validate the downloaded packages
p404 = pmpkg('pkg404')
self.addpkg2db('sync', p404)

cache_url = self.add_simple_http_server({
    '/{}'.format(pkgs[0].filename()): { 'body': 'CacheServer' },
    # 404 for packages 1-3
    '/{}'.format(pkgs[4].filename()): { 'body': 'CacheServer' },
})
normal_url = self.add_simple_http_server({
    '/{}'.format(pkgs[0].filename()): { 'body': 'Server' },
    '/{}'.format(pkgs[1].filename()): { 'body': 'Server' },
    '/{}'.format(pkgs[2].filename()): { 'body': 'Server' },
    '/{}'.format(pkgs[3].filename()): { 'body': 'Server' },
    '/{}'.format(pkgs[4].filename()): { 'body': 'Server' },
})

self.db['sync'].option['CacheServer'] = [ cache_url ]
self.db['sync'].option['Server'] = [ normal_url ]
self.db['sync'].syncdir = False
self.cachepkgs = False

self.args = '-S pkg0 pkg1 pkg2 pkg3 pkg4 pkg404'

#self.addrule("PACMAN_RETCODE=0") # TODO
self.addrule("PACMAN_OUTPUT={}".format(normal_url.replace("http://", "")))
self.addrule("!PACMAN_OUTPUT={}".format(cache_url.replace("http://", "")))
self.addrule("CACHE_FCONTENTS={}|CacheServer".format(pkgs[0].filename()))
self.addrule("CACHE_FCONTENTS={}|Server".format(pkgs[1].filename()))
self.addrule("CACHE_FCONTENTS={}|Server".format(pkgs[2].filename()))
self.addrule("CACHE_FCONTENTS={}|Server".format(pkgs[3].filename()))
self.addrule("CACHE_FCONTENTS={}|CacheServer".format(pkgs[4].filename()))
