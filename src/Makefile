OBJS = datachunk.o membuf.o folder.o requestbuf.o respbuf.o \
	   fmconfig.o filemanager.o cmdline.o main.o

HEADERS = fmconfig.h datachunk.h filemanager.h membuf.h requestbuf.h \
		  respbuf.h folder.h cmdline.h

filemanager-httpd: $(OBJS)
	gcc $(OBJS) -o filemanager-httpd

.c.o:
	gcc -g -c $< -Wall

clean:
	rm -f $(OBJS) filemanager-httpd

$(OBJS): $(HEADERS)
