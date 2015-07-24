filemanager-httpd
=================

A tiny http server (serves only static contents) with built-in
file management through http.


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

