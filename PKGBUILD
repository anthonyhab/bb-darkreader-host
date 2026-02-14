# Maintainer: Anthony Habash <bb@hab.rip>
# Contributor: Based on seaglass by alexhulbert

pkgname=bb-darkreader-host
pkgver=1.0.0
pkgrel=1
pkgdesc="Native messaging host for syncing pywal colors with Dark Reader (bb-auth fork)"
arch=('x86_64')
url="https://github.com/bb-auth/bb-darkreader-host"
license=('MIT')
depends=('glibc')
makedepends=('gcc')
options=('!strip')

build() {
    cd "$startdir"
    gcc -O2 -s -o bb-darkreader-host coloreader-native.c
}

package() {
    cd "$startdir"
    install -Dm755 bb-darkreader-host "$pkgdir/usr/bin/bb-darkreader-host"
    install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}

post_install() {
    echo "==> To complete installation, run:"
    echo "==>   bb-darkreader-host --setup"
    echo "==> Then restart Firefox"
}
