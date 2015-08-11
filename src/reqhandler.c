#include <stdbool.h>
#include "reqhandler.h"
#include "respbuf.h"
#include "fmlog.h"
#include "auth.h"
#include "filemanager.h"
#include "cgiexecutor.h"
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>


struct RequestHandler {
    FileManager *filemgr;
    CgiExecutor *cgiexe;
    RespBuf *respBuf;
    DataSource *response;
};


static RespBuf *printMovedAddSlash(const char *urlPath, bool onlyHead)
{
    char hostname[HOST_NAME_MAX], *newPath;
    int len;
    RespBuf *resp;

    resp = resp_new(HTTP_301_MOVED_PERMANENTLY);
    len = strlen(urlPath);
    newPath = malloc(len+2);
    memcpy(newPath, urlPath, len);
    strcpy(newPath+len, "/");
    resp_appendHeader(resp, "Location", newPath);
    free(newPath);
    resp_appendHeader(resp, "Content-Type", "text/html; charset=utf-8");
    if( ! onlyHead ) {
        gethostname(hostname, sizeof(hostname));
        resp_appendStr(resp, "<html><head><title>");
        resp_appendStrEscapeHtml(resp, urlPath);
        resp_appendStr(resp, " on ");
        resp_appendStrEscapeHtml(resp, hostname);
        resp_appendStr(resp, "</title></head><body>\n<h3>Moved to <a href=\"");
        resp_appendStrEscapeHtml(resp, urlPath);
        resp_appendStr(resp, "/\">");
        resp_appendStrEscapeHtml(resp, urlPath);
        resp_appendStr(resp, "/</a></h3>\n</body></html>\n");
    }
    return resp;
}

static RespBuf *printMesgPage(HttpStatus status, const char *mesg,
        const char *path, bool onlyHead, bool showLoginButton)
{
    char hostname[HOST_NAME_MAX];
    RespBuf *resp;

    resp = resp_new(status);
    resp_appendHeader(resp, "Content-Type", "text/html; charset=utf-8");
    if( ! onlyHead ) {
        gethostname(hostname, sizeof(hostname));
        resp_appendStr(resp, "<html><head><title>");
        resp_appendStrEscapeHtml(resp, path);
        resp_appendStr(resp, " on ");
        resp_appendStrEscapeHtml(resp, hostname);
        resp_appendStr(resp, "</title></head><body>\n");
        if( showLoginButton )
            resp_appendStr(resp, filemgr_getLoginForm());
        resp_appendStr(resp,
            "&nbsp;<div style=\"text-align: center; margin: 150px 0px\">\n"
            "<span style=\"font-size: x-large; border: 1px solid #FFF0B0; "
            "background-color: #FFFCF0; padding: 50px 100px\">\n");
        resp_appendStr(resp, resp_getErrorMessage(resp)),
        resp_appendStr(resp, "</span></div>\n");
        if( mesg != NULL ) {
            resp_appendStr(resp, "<p>");
            resp_appendStrEscapeHtml(resp, mesg);
            resp_appendStr(resp, "/<p>");
        }
        resp_appendStr(resp, "</body></html>\n");
    }
    return resp;
}

static RespBuf *printErrorPage(int sysErrno, const char *path,
        bool onlyHead, bool showLoginButton)
{
    HttpStatus status;
    const char *mesg = NULL;

    switch(sysErrno) {
    case 0:
        status = HTTP_200_OK;
        break;
    case ENOENT:
    case ENOTDIR:
        status = HTTP_404_NOT_FOUND;
        break;
    case EPERM:
    case EACCES:
        status = HTTP_403_FORBIDDEN;
        break;
    default:
        status = HTTP_500;
        mesg = strerror(sysErrno);
        break;
    }
    return printMesgPage(status, mesg, path, onlyHead, showLoginButton);
}

static RespBuf *printUnauthorized(const char *urlPath, bool onlyHead)
{
    char *authHeader;
    RespBuf *resp;

    resp = printMesgPage(HTTP_401_UNAUTHORIZED, NULL, urlPath, onlyHead, false);
    authHeader = auth_getAuthResponseHeader();
    resp_appendHeader(resp, "WWW-Authenticate", authHeader);
    free(authHeader);
    resp_appendHeader(resp, "Content-Type", "text/html; charset=utf-8");
    return resp;
}

static const char *getContentTypeByFileExt(const char *fname)
{
    static struct mime_type { const char *ext, *mime; } mime_types[] = {
        { "aac",     "audio/x-hx-aac-adts" },
        { "avi",     "video/x-msvideo" },
        { "bmp",     "image/bmp" },
        { "bz2",     "application/x-bzip2" },
        { "c",       "text/x-c" },
        { "css",     "text/css" },
        { "deb",     "application/x-debian-package" },
        { "doc",     "application/msword" },
        { "flv",     "video/x-flv" },
        { "gif",     "image/gif" },
        { "gz",      "application/gzip" },
        { "htm",     "text/html; charset=utf-8" },
        { "html",    "text/html; charset=utf-8" },
        { "java",    "text/plain" },
        { "jar",     "application/jar" },
        { "jpe",     "image/jpeg" },
        { "jpeg",    "image/jpeg" },
        { "jpg",     "image/jpeg" },
        { "m3u",     "audio/x-mpegurl" },
        { "mid",     "audio/midi" },
        { "midi",    "audio/midi" },
        { "mov",     "video/quicktime" },
        { "mp2",     "audio/mpeg" },
        { "mp3",     "audio/mpeg" },
        { "mp4",     "video/mp4" },
        { "mpe",     "video/mpeg" },
        { "mpeg",    "video/mpeg" },
        { "mpg",     "video/mpeg" },
        { "ogg",     "application/x-ogg" },
        { "pdf",     "application/pdf" },
        { "png",     "image/png" },
        { "ppt",     "application/vnd.ms-powerpoint" },
        { "ps",      "application/postscript" },
        { "qt",      "video/quicktime" },
        { "ra",      "audio/x-realaudio" },
        { "ram",     "audio/x-pn-realaudio" },
        { "rtf",     "text/rtf" },
        { "sh",      "text/plain; charset=utf-8" },
        { "svg",     "image/svg+xml" },
        { "svgz",    "image/svg+xml" },
        { "tar",     "application/x-tar" },
        { "tif",     "image/tiff" },
        { "tiff",    "image/tiff" },
        { "tsv",     "text/tab-separated-values; charset=utf-8" },
        { "txt",     "text/plain; charset=utf-8" },
        { "wav",     "audio/x-wav" },
        { "wma",     "audio/x-ms-wma" },
        { "wmv",     "video/x-ms-wmv" },
        { "wmx",     "video/x-ms-wmx" },
        { "xls",     "application/vnd.ms-excel" },
        { "xml",     "text/xml; charset=utf-8" },
        { "xpm",     "image/x-xpmi" },
        { "xsl",     "text/xml; charset=utf-8" },
        { "zip",     "application/zip" }
    };
    const char *ext, *res = NULL;
    int i;

    if( (ext = strrchr(fname, '/')) == NULL )
        ext = fname;
    if( (ext = strrchr(ext, '.')) == NULL || ext == fname || ext[-1] == '/' )
        ext = ".txt";
    ++ext;
    for(i = 0; i < sizeof(mime_types) / sizeof(mime_types[0]) &&
            res == NULL; ++i)
    {
        if( !strcasecmp(ext, mime_types[i].ext) )
            res = mime_types[i].mime;
    }
    if( res == NULL )
        res = "application/octet-stream";
    return res;
}

static RespBuf *processFileReq(const char *urlPath, const char *sysPath,
        bool onlyHead)
{
    int fd;
    RespBuf *resp;

    if( (fd = open(sysPath, O_RDONLY)) >= 0 ) {
        log_debug("opened %s", sysPath);
        resp = resp_new(HTTP_200_OK);
        resp_appendHeader(resp, "Content-Type",
                getContentTypeByFileExt(sysPath));
        if( onlyHead ) {
            close(fd);
        }else{
            // TODO: escape filename
            MemBuf *header = mb_new();
            mb_appendStrL(header, "inline; filename=\"", 
                    strrchr(urlPath, '/')+1, "\"", NULL);
            resp_appendHeader(resp, "Content-Disposition", mb_data(header));
            mb_free(header);
            resp_enqFile(resp, fd);
        }
    }else{
        resp = printErrorPage(errno, urlPath, onlyHead, false);
    }
    return resp;
}

static RespBuf *processFolderReq(const RequestHeader *rhdr,
        FileManager *filemgr)
{
    const char *queryFile = reqhdr_getPath(rhdr);
    RespBuf *resp;
    bool isHeadReq = !strcmp(reqhdr_getMethod(rhdr), "HEAD");
    int sysErrNo = 0;

    if( !strcmp(reqhdr_getMethod(rhdr), "POST") && filemgr_processPost(
                filemgr, rhdr) == PR_REQUIRE_AUTH)
    {
        resp = printUnauthorized(queryFile, isHeadReq);
    }else{
        if( reqhdr_isActionAllowed(rhdr, PA_LIST_FOLDER) ) {
            resp = filemgr_printFolderContents(filemgr, rhdr, &sysErrNo);
            if( resp == NULL ) {
                resp = printErrorPage(sysErrNo, queryFile, isHeadReq, false);
            }
        }else{
            resp = printErrorPage(ENOENT, queryFile, isHeadReq,
                    reqhdr_isWorthPuttingLogOnButton(rhdr));
        }
    }
    return resp;
}

static void doProcessRequest(RequestHandler *hdlr, const RequestHeader *rhdr)
{
    unsigned queryFileLen, isHeadReq;
    const char *queryFile;
    RespBuf *resp = NULL;

    isHeadReq = !strcmp(reqhdr_getMethod(rhdr), "HEAD");
    queryFile = reqhdr_getPath(rhdr);
    queryFileLen = strlen(queryFile);
    if( reqhdr_getLoginState(rhdr) == LS_LOGIN_FAIL ||
            ! reqhdr_isActionAllowed(rhdr, PA_SERVE_PAGE) )
    {
        if( reqhdr_getLoginState(rhdr) == LS_LOGIN_FAIL ) {
            log_debug("authorization fail: sleep 2");
            sleep(2); /*make a possible dictionary attack harder to overcome*/
        }
        resp = printUnauthorized(reqhdr_getPath(rhdr), isHeadReq);
    }else if( queryFileLen >= 3 && (strstr(queryFile, "/../") != NULL ||
            !strcmp(queryFile+queryFileLen-3, "/.."))) 
    {
        resp = printMesgPage(HTTP_403_FORBIDDEN, NULL, queryFile,
                isHeadReq, false);
    }else{
        int sysErrNo = 0;
        struct stat st;
        char *sysPath = config_getSysPathForUrlPath(queryFile), *indexFile;
        Folder *folder = NULL;
        bool isFolder = false;

        if( sysPath != NULL ) {
            if( stat(sysPath, &st) == 0 )
                isFolder = S_ISDIR(st.st_mode);
            else
                sysErrNo = errno;
        }else{
            if( (folder = config_getSubSharesForPath(queryFile)) == NULL )
                sysErrNo = ENOENT;
            else
                isFolder = true;
        }

        if( sysErrNo == 0 && isFolder && queryFile[strlen(queryFile)-1] != '/')
        {
            resp = printMovedAddSlash(queryFile, isHeadReq);
        }else{
            if( sysErrNo == 0 && isFolder && folder == NULL &&
                (indexFile = config_getIndexFile(sysPath, &sysErrNo)) != NULL )
            {
                free(sysPath);
                sysPath = indexFile;
                isFolder = false;
            }
            if( sysErrNo == 0 ) {
                if( isFolder ) {
                    hdlr->filemgr = filemgr_new(sysPath, rhdr);
                }else if( config_isCGI(queryFile) ) {
                    hdlr->cgiexe = cgiexe_new(rhdr, sysPath);
                }else{
                    resp = processFileReq(queryFile, sysPath, isHeadReq);
                }
            }else{
                resp = printErrorPage(sysErrNo, queryFile, isHeadReq, false);
            }
        }
        folder_free(folder);
        free(sysPath);
    }
    hdlr->respBuf = resp;
}

RequestHandler *reqhdlr_new(const RequestHeader *rhdr)
{
    RequestHandler *handler = malloc(sizeof(RequestHandler));
    const char *meth = reqhdr_getMethod(rhdr);
    int isHeadReq = ! strcmp(meth, "HEAD");

    handler->filemgr = NULL;
    handler->cgiexe = NULL;
    handler->respBuf = NULL;
    handler->response = NULL;
    if( strcmp(meth, "GET") && strcmp(meth, "POST") && ! isHeadReq ) {
        handler->respBuf = resp_new(HTTP_405_METHOD_NOT_ALLOWED);
        resp_appendHeader(handler->respBuf, "Allow", "GET, HEAD, POST");
    }else{
        doProcessRequest(handler, rhdr);
    }
    return handler;
}

void reqhdlr_consumeBodyBytes(RequestHandler *hdlr, const char *data,
        unsigned len)
{
    if( hdlr->filemgr != NULL ) {
        filemgr_consumeBodyBytes(hdlr->filemgr, data, len);
    }else if( hdlr->cgiexe != NULL ) {
        cgiexe_consumeBodyBytes(hdlr->cgiexe, data, len);
    }
}

void reqhdlr_requestReadCompleted(RequestHandler *hdlr,
        const RequestHeader *rhdr)
{
    const char *meth = reqhdr_getMethod(rhdr);
    int isHeadReq = ! strcmp(meth, "HEAD");

    if( hdlr->respBuf == NULL ) {
        if( hdlr->filemgr != NULL )
            hdlr->respBuf = processFolderReq(rhdr, hdlr->filemgr);
        else if( hdlr->cgiexe != NULL ) {
            hdlr->respBuf = cgiexe_requestReadCompleted(hdlr->cgiexe);
        }else
            hdlr->respBuf = printMesgPage(HTTP_500,
                    "reqhandler: unspecified handler",
                    reqhdr_getPath(rhdr), isHeadReq, false);

    }
    resp_appendHeader(hdlr->respBuf, "Connection", "close");
    resp_appendHeader(hdlr->respBuf, "Server", "filemanager-httpd");
    hdlr->response = resp_finish(hdlr->respBuf, isHeadReq);
    hdlr->respBuf = NULL;
}

bool reqhdlr_emitResponseBytes(RequestHandler *hdlr, int fd, int *sysErrNo)
{
    return ds_write(hdlr->response, fd, sysErrNo);
}

void reqhdlr_free(RequestHandler *hdlr)
{
    if( hdlr != NULL ) {
        filemgr_free(hdlr->filemgr);
        cgiexe_free(hdlr->cgiexe);
        ds_free(hdlr->response);
        free(hdlr);
    }
}

