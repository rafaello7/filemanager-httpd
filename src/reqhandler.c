#include <stdbool.h>
#include "reqhandler.h"
#include "respbuf.h"
#include "responsesender.h"
#include "fmlog.h"
#include "auth.h"
#include "filemanager.h"
#include "cgiexecutor.h"
#include "membuf.h"
#include "contenttype.h"
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
    ResponseSender *response;
};


static RespBuf *printMovedAddSlash(const char *urlPath, bool onlyHead)
{
    char hostname[HOST_NAME_MAX], *newPath;
    int len;
    RespBuf *resp;

    resp = resp_new(HTTP_301_MOVED_PERMANENTLY, onlyHead);
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

    resp = resp_new(status, onlyHead);
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

static RespBuf *processFileReq(const char *urlPath, const char *sysPath,
        bool onlyHead)
{
    int fd;
    RespBuf *resp;

    if( (fd = open(sysPath, O_RDONLY)) >= 0 ) {
        log_debug("opened %s", sysPath);
        resp = resp_new(HTTP_200_OK, onlyHead);
        resp_appendHeader(resp, "Content-Type",
                cttype_getContentTypeByFileExt(sysPath));
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

static RespBuf *doProcessRequest(RequestHandler *hdlr,
        const RequestHeader *rhdr)
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
    return resp;
}

RequestHandler *reqhdlr_new(const RequestHeader *rhdr)
{
    RequestHandler *handler = malloc(sizeof(RequestHandler));
    const char *meth = reqhdr_getMethod(rhdr);
    RespBuf *resp = NULL;
    bool isHeadReq = ! strcmp(meth, "HEAD");

    handler->filemgr = NULL;
    handler->cgiexe = NULL;
    if( strcmp(meth, "GET") && strcmp(meth, "POST") && ! isHeadReq ) {
        resp = resp_new(HTTP_405_METHOD_NOT_ALLOWED, isHeadReq);
        resp_appendHeader(resp, "Allow", "GET, HEAD, POST");
    }else{
        resp = doProcessRequest(handler, rhdr);
    }
    handler->response = resp == NULL ? NULL : resp_finish(resp);
    return handler;
}

unsigned reqhdlr_processData(RequestHandler *hdlr, const char *data,
        unsigned len, DataReadySelector *drs)
{
    unsigned processed = len;

    if( hdlr->filemgr != NULL ) {
        filemgr_consumeBodyBytes(hdlr->filemgr, data, len);
    }else if( hdlr->cgiexe != NULL ) {
        processed = cgiexe_processData(hdlr->cgiexe, data, len, drs);
    }
    return processed;
}

void reqhdlr_requestReadCompleted(RequestHandler *hdlr,
        const RequestHeader *rhdr)
{
    const char *meth = reqhdr_getMethod(rhdr);
    int isHeadReq = ! strcmp(meth, "HEAD");
    RespBuf *resp = NULL;

    if( hdlr->cgiexe != NULL ) {
        cgiexe_requestReadCompleted(hdlr->cgiexe);
    }else if( hdlr->response == NULL ) {
        if( hdlr->filemgr != NULL ) {
            resp = processFolderReq(rhdr, hdlr->filemgr);
        }else
            resp = printMesgPage(HTTP_500,
                    "reqhandler: unspecified handler",
                    reqhdr_getPath(rhdr), isHeadReq, false);
        hdlr->response = resp_finish(resp);
    }
}

bool reqhdlr_progressResponse(RequestHandler *hdlr, int fd,
        DataReadySelector *drs)
{
    bool isFinished = false;

    if( hdlr->response == NULL && hdlr->cgiexe != NULL ) {
        RespBuf *resp = cgiexe_getResponse(hdlr->cgiexe, drs);
        if( resp != NULL )
            hdlr->response = resp_finish(resp);
    }
    if( hdlr->response != NULL ) {
        isFinished = rsndr_send(hdlr->response, fd, drs);
    }
    return isFinished;
}

void reqhdlr_free(RequestHandler *hdlr)
{
    if( hdlr != NULL ) {
        filemgr_free(hdlr->filemgr);
        cgiexe_free(hdlr->cgiexe);
        rsndr_free(hdlr->response);
        free(hdlr);
    }
}

