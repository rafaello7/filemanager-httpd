# Maintainer: Rafal <fatwildcat@gmail.com>
pkgname=filemanager-httpd
pkgver=0.3.5
pkgrel=1
pkgdesc="Tiny http server with file management via web"
arch=('x86_64')
url="https://github.com/rafaello7/filemanager-httpd"
license=('GPL')
depends=('glibc')
source=("$pkgname-$pkgver.tar.gz")
md5sums=('SKIP')

build() {
	cd "$pkgname-$pkgver"
	./configure --prefix=/usr --sysconfdir=/etc
	make
}

package() {
	cd "$pkgname-$pkgver"
	make DESTDIR="$pkgdir/" install
}
