SUBDIRS	= src debian

pkgsysconfdir = $(sysconfdir)/filemanager-httpd.d

dist_pkgsysconf_DATA = default.conf

deb: dist
	rm -rf dpkg-deb
	mkdir dpkg-deb
	cd dpkg-deb && tar xf ../$(distdir).tar.gz
	cd dpkg-deb/$(distdir) && dpkg-buildpackage -us -uc

