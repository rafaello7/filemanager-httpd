pkgname=filemanager-httpd
pkgver=0.3.3
pkgrel=1
pkgdesc="Tiny http server with file management via web"
arch=('x86_64')
url="https://github.com/rafaello7/filemanager-httpd"
license=('GPL')
depends=('gtk3')
source=("$pkgname-$pkgver.tar.gz")
md5sums=('a411f1ab20111f4648b0480dffd7833b')

build() {
	cd "$pkgname-$pkgver"
	./configure --prefix=/usr --sysconfdir=/etc
	make
}

package() {
	cd "$pkgname-$pkgver"
	make DESTDIR="$pkgdir/" install
}
