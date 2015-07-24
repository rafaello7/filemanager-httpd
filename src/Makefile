OBJS = datachunk.o membuf.o requestbuf.o respbuf.o configfile.o \
	   main.o filemanager.o

HEADERS = configfile.h datachunk.h filemanager.h membuf.h requestbuf.h respbuf.h

filemanager-httpd: $(OBJS)
	gcc $(OBJS) -o filemanager-httpd

.c.o:
	gcc -g -c $< -Wall

clean:
	rm -f $(OBJS) filemanager-httpd

$(OBJS): $(HEADERS)
