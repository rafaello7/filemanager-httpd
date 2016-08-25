SUBDIRS	= src conf.d

systemddir = $(libdir)/systemd/system
dist_systemd_DATA = filemanager-httpd.service \
					filemanager-httpd@.service \
					filemanager-httpd.socket

arch: dist
	rm -rf archlinux
	mkdir archlinux
	cp PKGBUILD filemanager-httpd-*.tar.gz archlinux
	cd archlinux && makepkg
