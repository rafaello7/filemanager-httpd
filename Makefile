filemanager.cgi: filemanager_cgi.o
	gcc filemanager_cgi.o -o filemanager.cgi

.c.o:
	gcc -g -c -Wall $<
