# Maintainer: anthonyhab <bb@hab.rip>
# Contributor: Based on seaglass by alexhulbert

pkgname=bb-darkreader-host
pkgver=0.2.0
pkgrel=1
pkgdesc="Native messaging host for syncing pywal colors with Dark Reader"
arch=('x86_64')
url="https://github.com/anthonyhab/bb-darkreader-host"
license=('MIT')
depends=('glibc' 'json-c')
makedepends=('gcc')
options=('!strip')
source=("$pkgname-$pkgver.tar.gz::https://github.com/anthonyhab/bb-darkreader-host/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('529b0addb9bdb01023717fe29a86d48785b91db9f29daec51b27d081de052a30')

build() {
    cd "$pkgname-$pkgver"
    make
}

package() {
    cd "$pkgname-$pkgver"
    install -Dm755 bb-darkreader-host "$pkgdir/usr/bin/bb-darkreader-host"
    install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}

post_install() {
    echo "==> To complete installation, run:"
    echo "==>   bb-darkreader-host install"
    echo "==> Then restart Firefox"
}
