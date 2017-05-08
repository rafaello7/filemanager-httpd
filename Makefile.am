SUBDIRS	= src conf.d debian

dist_html_DATA = welcome.html


arch: dist
	rm -rf archlinux
	mkdir archlinux
	cp PKGBUILD filemanager-httpd-*.tar.gz archlinux
	cd archlinux && makepkg

deb: dist
	rm -rf dpkg-deb
	mkdir dpkg-deb
	cd dpkg-deb && tar xf ../$(distdir).tar.gz
	cd dpkg-deb/$(distdir) && dpkg-buildpackage -us -uc

