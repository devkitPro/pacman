" Vim syntax file
" Language:     PKGBUILD
" Maintainer:   Alessio 'mOLOk' Bolognino <themolok at gmail.com>
" Last Change:  2007/05/08
" Version Info: PKGBUILD-0.1

" For version 5.x: Clear all syntax items
" For version 6.x: Quit when a syntax file was already loaded
if version < 600
	syntax clear
elseif exists("b:current_syntax")
    finish
endif


" case on
syn case match

" pkgname
" FIXME if '=' is in pkgname/pkgver, it highlights whole string, not just '='
syn keyword pb_k_pkgname pkgname contained
syn match pbValidPkgname /\([[:alnum:]]\|+\|-\|_\){,32}/ contained contains=pbIllegalPkgname
syn match pbIllegalPkgname /[[:upper:]]\|[^[:alnum:]-+_=]\|=.*=\|=['"]\?.\{33,\}['"]\?/ contained
"syn match pbIllegalPkgname /=.\{33,\}/ contains=pbValidPkgname contained
"syn match pbIllegalPkgname /[^=]/ contains=pbValidPkgname contained
"syn match pbValidPkgname   /=\([[:lower:][:digit:]-_+]\)\{,32\}/ contained
"syn match pbIllegalPkgname /[^=]/ contains=pbValidPkgname contained
"syn match pbValidPkgname /=\([[:digit:][:lower:]]\|+\|-\|_\)\{,32\}/ contained
syn match pbPkgnameGroup /^pkgname=.*/ contains=pbIllegalPkgname,pb_k_pkgname ",pbValidPkgname

" pkgver
syn keyword pb_k_pkgver pkgver contained
syn match pbValidPkgver /\([[:alnum:]]\|\.\|+\|_\)/ contained contains=pbIllegalPkgver
syn match pbIllegalPkgver /[^[:alnum:]+=\.\_]\|=.*=/ contained
syn match pbPkgverGroup /^pkgver=.*/ contains=pbIllegalPkgver,pbValidPkgver,pb_k_pkgver

" pkgrel
syn keyword pb_k_pkgrel pkgrel contained
syn match pbValidPkgrel /[[:digit:]]*/ contained contains=pbIllegalPkgver
"syn match pbIllegalPkgrel /[^[:alnum:]=]\|[[:alpha:]]/ contained
syn match pbIllegalPkgrel /[^[:digit:]=]\|=.*=/ contained
syn match pbPkgrelGroup /^pkgrel=.*/ contains=pbIllegalPkgrel,pbValidPkgrel,pb_k_pkgrel

" pkgdesc
syn keyword pb_k_desc pkgdesc contained
" 90 chars: 80 for description, 8 for pkgdesc and 2 for ''
syn match pbIllegalPkgdesc /.\{90,}\|=['"]\?.*['" ]\+[iI][sS] [aA]/ contained contains=pbPkgdescSign
syn match pbValidPkgdesc /[^='"]\.\{,80}/ contained contains=pbIllegalPkgdesc
syn match pbPkgdescGroup /^pkgdesc=.*/ contains=pbIllegalPkgdesc,pb_k_desc,pbValidPkgdesc
syn match pbPkgdescSign /[='"]/ contained


" url
syn keyword pb_k_url url contained
syn match pbValidUrl /['"]*\(https\|http\|ftp\)\:\/.*\.\+.*/ contained

syn match pbIllegalUrl /[^=]/ contained contains=pbValidUrl
"syn match pbIllegalUrl /\(https\|http\|ftp\)\:\/\/.*/ contained
syn match pbUrlGroup /^url=.*/ contains=pbValidUrl,pb_k_url,pbIllegalUrl
"syn match pbEq /=/ contained

" license
syn keyword pb_k_license license contained
syn keyword pbLicense  APACHE CDDL EPL FDL GPL LGPL MPL PHP RUBY ZLIB ISC MIT BSD contained
syn match pbLicenseCustom /custom\(:[[:alnum:]]*\)*/ contained
"syn match pbValidLicense /[^=][('")]*/  contained
"syn match pbLicenseGroup /^license=.*/ contains=pb_k_license,pbLicense,pbValidLicense,pbLicenseCustom
syn match pbIllegalLicense /[^='"() ]/ contained contains=pbLicenseCustom,pbLicense
syn match pbLicenseGroup /^license=.*/ contains=pb_k_license,pbLicenseCustom,pbLicense,pbIllegalLicense

" backup
syn keyword pb_k_backup backup contained
syn match pbValidBackup   /\.\?[[:alpha:]]*\/[[:alnum:]\{\}+._$-]*]*/ contained
syn match pbBackupGroup /^backup=.*/ contains=pb_k_backup,pbValidBackup

" arch
syn keyword pb_k_arch arch contained
syn keyword pbArch i686 x86_64 ppc contained
syn match pbIllegalArch /[^='() ]/ contained contains=pbArch
syn match pbArchGroup /^arch=.*/ contains=pb_k_arch,pbArch,pbIllegalArch

" makedepends
syn keyword pb_k_makedepends makedepends contained
syn match pbValidMakedepends /\([[:alnum:]]\|+\|-\|_\)*/ contained
"syn match pbMakedependsGroup /^makedepends=.*/ contains=pb_k_makedepends,pbValidMakedepends
syn region pbMakedependsGroup start=/^makedepends=(/ end=/)/ contains=pb_k_makedepends,pbValidMakedepends

" depends
syn keyword pb_k_depends depends contained
syn match pbValidDepends /\([[:alnum:]]\|+\|-\|_\)*/ contained
"syn match pbDependsGroup /^depends=.*/ contains=pb_k_depends,pbValidDepends
syn region pbDependsGroup start=/^depends=(/ end=/)/ contains=pb_k_depends,pbValidDepends

" XXX little hack to color conflicts/provides/replaces keyword even without =()
syn match  pbkw /^\(conflicts\|provides\|replaces\)/ contains=pb_k_conflicts,pb_k_provides,pb_k_replaces
hi link pbkw keyword

" conflicts
"syn keyword pb_k_conflicts conflicts
syn keyword pb_k_conflicts conflicts contained
syn match pbValidConflicts /\([[:alnum:]]\|+\|-\|_\)*/ contained
"syn match pbConflictsGroup /^conflicts=.*/ contains=pb_k_conflicts,pbValidConflicts
syn region pbConflictsGroup start=/^conflicts=(/ end=/)/ contains=pb_k_conflicts,pbValidConflicts
"syn region pbConflictsGroup start=/^conflicts=(/ end=/)/ contains=pbValidConflicts

" provides
"syn keyword pb_k_provides provides
syn keyword pb_k_provides provides contained
syn match pbValidProvides /\([[:alnum:]]\|+\|-\|_\)*/ contained
"syn match pbProvidesGroup /^provides=.*/ contains=pb_k_provides,pbValidProvides
syn region pbProvidesGroup start=/^provides=(/ end=/)/ contains=pb_k_provides,pbValidProvides
"syn region pbProvidesGroup start=/^provides=(/ end=/)/ contains=pbValidProvides

" replaces
"syn keyword pb_k_replaces replaces
syn keyword pb_k_replaces replaces contained
syn match pbValidReplaces /\([[:alnum:]]\|+\|-\|_\)*/ contained
"syn match pbReplacesGroup /^replaces=.*/ contains=pb_k_replaces,pbValidReplaces
syn region pbReplacesGroup start=/^replaces=(/  end=/)/ contains=pb_k_replaces,pbValidReplaces
"syn region pbReplacesGroup start=/^replaces=(/  end=/)/ contains=pbValidReplaces

" install
syn keyword pb_k_install install contained
syn match pbValidInstall /\([[:alnum:]]\|\$\|+\|-\|_\)*\.install/ contained
syn match pbIllegalInstall /[^=]/ contained contains=pbValidInstall
"syn match pbInstall /\([a-z]\|+\|-\)*\.install/ contained
syn match pbInstallGroup /^install=.*/ contains=pb_k_install,pbValidInstall,pbIllegalInstall

" source
syn keyword pb_k_source source contained

" search for specific sf.net mirrors
syn match pbIllegalSource /\(http\|ftp\|https\).*\.\+\(dl\|download.\?\)\.\(sourceforge\|sf\).net/ contained
syn match pbSourceRemote /['"]*\(https\|http\|ftp\)\:\/\/.*[[:alnum:]"']/   contained contains=pbIllegalSource
"syn match pbSourceLocal /[[:alnum:]+._${}\/-]\+/ contained
"syn match pbSourceLocal /[[:alnum:]+._${}-]\+/ contained
syn region pbSourceGroup  start=/^source=(/ end=/)/ contains=pb_k_source,pbSourceRemote
",pbSourceLocal
"syn match pbSourceGroup /^source=.*/ contains=pb_k_source,pbSourceRemote,pbSourceLocal


" md5sums
syn keyword pb_k_md5sums md5sums contained
syn match pbValidMd5sums /[[:alnum:]]\{32\}/ contained
syn match pbIllegalMd5sums /[^='"()\/ ]/ contained contains=pbValidMd5sums
syn region pbMd5sumsGroup start=/^md5sums/ end=/)/ contains=pb_k_md5sums,pbValidMd5sums,pbIllegalMd5sums

" sha1sums
syn keyword pb_k_sha1sums sha1sums contained
syn match pbValidSha1sums /[[:alnum:]]\{40\}/ contained
syn match pbIllegalSha1sums /[^='"()\/ ]/ contained contains=pbValidSha1sums
syn region pbSha1sumsGroup start=/^sha1sums/ end=/)/ contains=pb_k_sha1sums,pbValidSha1sums,pbIllegalSha1sums

" options
syn keyword pb_k_options options contained
"syn keyword pbOptions strip docs libtool emptydirs ccache distcc makeflags force contained
syn match pbOptions /\(no\)\?\(strip\|docs\|libtool\|emptydirs\|ccache\|distcc\|makeflags\|force\)/ contained
" syn match   pbOptionsNeg   /\(\!\|no\)/ contained
syn match   pbOptionsNeg     /\!/ contained
syn match   pbOptionsDeprec  /no/ contained
syn region pbOptionsGroup start=/^options=(/ end=/)/ contains=pb_k_options,pbOptions,pbOptionsNeg,pbOptionsDeprec,pbIllegalOption
syn match pbIllegalOption /[^!"'()= ]/ contained contains=pbOptionsDeprec,pbOptions

" noextract
syn match pbNoextract /[[:alnum:]+._${}-]\+/ contained
syn keyword pb_k_noextract noextract  contained
syn region pbNoextractGroup  start=/^noextract=(/ end=/)/ contains=pb_k_noextract,pbNoextract

" comments
syn keyword    pb_k_maintainer Maintainer Contributor contained
"syn match      pbMaintainer /:.*/ contained
syn match      pbMaintainerGroup /Maintainer.*/ contains=pbMaintainer contained

syn match pbDate /[0-9]\{4}\/[0-9]\{2}\/[0-9]\{2}/ contained

syn cluster    pbCommentGroup	contains=pbTodo,pb_k_maintainer,pbMaintainerGroup,pbDate
syn keyword    pbTodo	contained	COMBAK FIXME TODO XXX
syn match      pbComment	"^#.*$"	contains=@pbCommentGroup
syn match      pbComment	"[^0-9]#.*$"	contains=@pbCommentGroup

hi link pbComment Comment
hi link pbTodo Todo

hi link pbValidPkgname Special
hi link pbPkgnameGroup Normal
hi link pbIllegalPkgname Error
hi link pb_k_pkgname Keyword

hi link pbValidPkgver StorageClass
hi link pbPkgverGroup Normal
hi link pbIllegalPkgver Error
hi link pb_k_pkgver Keyword

hi link pbValidPkgrel Number
hi link pbPkgrelGroup Normal
hi link pbIllegalPkgrel Error
hi link pb_k_pkgrel Keyword

hi link pbValidPkgdesc Special
hi link pbPkgdescGroup Normal
hi link pbIllegalPkgdesc Error
hi link pb_k_desc Keyword
hi link pbPkgdescSign Normal

hi link pbIllegalUrl Error
hi link pbValidUrl Comment
hi link pbUrlGroup Normal
hi link pbEq Normal
hi link pb_k_url Keyword

hi link pb_k_license Keyword
hi link pbLicense Number
hi link pbLicenseCustom Number
hi link pbLicenseGroup Normal
hi link pbValidLicense Normal
hi link pbIllegalLicense Error

hi link pbBackupGroup Normal
hi link pbValidBackup Identifier
hi link pb_k_backup Keyword

hi link pbArchGroup Normal
hi link pb_k_arch Keyword
hi link pbArch Number
hi link pbIllegalArch Error

hi link pbMakedependsGroup Normal
hi link pb_k_makedepends Keyword
hi link pbValidMakedepends Comment

hi link pbDependsGroup Normal
hi link pb_k_depends Keyword
hi link pbValidDepends StorageClass

hi link pbReplacesGroup Normal
hi link pb_k_replaces Keyword
hi link pbValidReplaces Identifier

hi link pbConflictsGroup Normal
hi link pb_k_conflicts Keyword
hi link pbValidConflicts Number

hi link pbProvidesGroup Normal
hi link pb_k_provides Keyword
hi link pbValidProvides Special

hi link pbValidInstall Normal
hi link pbIllegalInstall Error
hi link pb_k_install Keyword

"hi link pbSourceLocal Identifier
hi link pb_k_source Keyword
hi link pbSourceRemote Number
hi link pbSourceGroup Normal
hi link pbIllegalSource Error

hi link pb_k_md5sums Keyword
hi link pbMd5sumsGroup Normal
hi link pbValidMd5sums StorageClass
hi link pbIllegalMd5sums Error

hi link pb_k_sha1sums Keyword
hi link pbSha1sumsGroup Normal
hi link pbValidSha1sums Number
hi link pbIllegalSha1sums Error

hi link pb_k_options Keyword
hi link pbOptions StorageClass
hi link pbOptionsNeg StorageClass
hi link pbOptionsGroup Normal
hi link pbOptionsDeprec Todo
hi link pbIllegalOption Error

hi link pb_k_noextract Keyword
hi link pbNoextract Identifier
hi link pbNoextractGroup Normal

hi link pb_k_maintainer Keyword
"hi link pbMaintainer Normal

hi link pbDate Special

syntax include @SHELL syntax/sh.vim
syntax region BUILD start=/^build()/ end=/^}/ contains=@SHELL
let b:current_syntax = "PKGBUILD"

" vim: ft=vim
