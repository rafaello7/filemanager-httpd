filemanager_cgi
===============

A simple file manager CGI. Tested with
[mini-httpd](http://www.acme.com/software/mini_httpd/).

Compilation
-----------

Run _make_.

Installation
------------

Copy _filemanager.cgi_ binary to _cgi-bin_ directory of your httpd server,
or whatever directory configured as containing CGI scripts.

Configuration
-------------

Program reads shares from file _/etc/filemanager-cgi.conf_. The
file syntax is as follows:

 * empty lines are ignored
 * lines starting with '#' are treated as comment
 * remaining lines are assumed to be shares, in format:

            share_name=/share/root

Example file:

        homes=/home
        mounts=/mnt

