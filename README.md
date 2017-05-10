filemanager-httpd
=================

A tiny, ligtweight http server with CGI support and extended
directory listing.

The directory listing feature has built-in possibility to manage
files using web browser. File management includes rename, removal,
moving to parent directory or to a subdirectory. Also new files may
be uploaded. File management may be available for everyone or
password protected. It is possible also to turn off the file
management possibility completely or even disable whole listing
feature. Please see [default.conf](conf.d/default.conf) file in
[conf.d](conf.d) directory for all possible options.

The mini-server may be considered as a convenient file manager
and http server in home network. The extended directory listing
feature may be easily used to serve e.g. audio and video files for
tablets and phones having Android system on board.

Intallation
-----------

Release contains binary packages for Debian/Ubintu. Just install the
package using dpkg. The server will be started by the installer.

If your system is not debian-like, you will have to compile the server.
Take filemanager-httpd.**.tar.gz file. Unpack, run _configure_ script,
then _make_ and _make install_. Note that installation made this way
does not have any init scripts.

Configuration
-------------

See [welcome file](welcome.html)

