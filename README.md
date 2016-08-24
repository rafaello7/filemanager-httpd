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
and http server in home network. The extended directory lising
feature may be easily used to serve e.g. audio and video files for
tablets and phones having Android system on board.

Intallation
-----------

Run _configure_ script, then make && make install

Configuration
-------------

Configuration is read from files located in `/etc/filemanager-httpd.d/`
directory. Server reads all files with _.conf_ suffix.  Installation
script places one file there, named `default.conf`. The file contains
all the possible options, commented well.

Instead of editing the `default.conf` file, you may place a new one.
Configuration has some reasonable defaults. The only thing which might
be needed to set up is the location of directory with files to serve.
By default `/srv/http` directory is used as document root. This directory
is usually empty, or even may not exist on some systems. The simplest
configuration file may look like:

        / = /

The above configuration causes to serve root directory as document root.

Running
-------

Invoke _filemanager-httpd_ (without options). Server runs in foreground
(does not daemonize). Installation contains also _systemd_ service file which
allows to start the service by the _systemd_ init system. It means,
the server may be started by invoke as root:

        systemctl start filemanager-httpd

To enable start at system startup, invoke:

        systemctl enable filemanager-httpd

To check whether the server is running, invoke:

        systemctl status filemanager-httpd

and so forth.

Program may be also started by a normal user (non-root). The server
distinguishes whether it was started by root or a normal user. When
started as root, the default listen port is 80. Server also switches
to a non-privileged user after startup (_http_ by default). When started
by a non-privileged user, the default listen port is 8000. Also user
switch is not performed in this case.

It may be sometimes desirable to provide another location of configuration
files - especially when the server is started by a non-root user. The
configuration file or directory may be provided using `-c` option. The option
parameter may point to a file or directory. When points to a directory, all
files with _.conf_ extension are read from there.


File management
---------------

Built-in file management is associated with directory listing feature.
Files may be managed using a browser having JavaScript enabled.
Of course, the file management may be possible only in directories on which the
server has write access.

Entries in directory listing may contain a colored `.` (dot) or a `+` (plus)
sign on the left. When the `+` is displayed, it is possible to rename or delete
the file. When the `+` sign is clicked, a form with possible actions is
opened below the file name. The page with directory listing
may contain also another form below the list of directory entries, which
allows to upload a new file or to create a new directory.

As mentioned, the server may be set up to make possible the file management
only by authorized users or by everyone.

CGI support
-----------

CGI support is rather straightforward. It is disabled by default. To
enable it, the _cgi_ option must be set in configuration file.

For example, the line below causes to treat all files with _cgi_ extension
as CGI programs to execute:

        cgi = *.cgi

Sample CGI script below prints out the CGI program whole environment:

        #!/bin/sh
        cat <<End
        HTTP/1.1 200 Ok
        Content-Type: text/html; charset=utf-8
        
        <html>
        <title>Environment</title>
        <body>
        <pre>
        End
        set
        cat <<End
        </pre>
        </body>
        </html>
        End


