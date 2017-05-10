bin_PROGRAMS = filemanager-httpd

filemanager_httpd_SOURCES = datachunk.c membuf.c folder.c requestheader.c \
							dataprocessingresult.c \
							datareadyselector.c serverconnection.c \
							respbuf.c responsesender.c \
							fmconfig.c contenttype.c \
							contentpart.c multipartdata.c \
							filemanager.c \
							dataheader.c cgiexecutor.c cmdline.c \
							md5calc.c auth.c fmlog.c \
							reqhandler.c main.c \
							\
							dataprocessingresult.h \
							fmconfig.h datachunk.h contenttype.h \
							contentpart.h multipartdata.h \
							datareadyselector.h filemanager.h \
							dataheader.h cgiexecutor.h membuf.h \
							requestheader.h serverconnection.h \
							respbuf.h responsesender.h \
							folder.h cmdline.h \
							md5calc.h auth.h fmlog.h \
							reqhandler.h

filemanager_httpd_CPPFLAGS = -Wall -DHTMLDIR='"$(htmldir)"' \
							 -DSYSCONFDIR='"$(sysconfdir)"'
