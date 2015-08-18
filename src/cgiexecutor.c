#include <stdbool.h>
#include "cgiexecutor.h"
#include "dataheader.h"
#include "fmlog.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <limits.h>
#include <libgen.h>


struct CgiExecutor {
    bool onlyHead;
    int outFd, inFd;
    DataHeader *cgiHeader;  /* header of data received from CGI */
};

static void fatalErrorResp(const char *message, const char *status)
{
    if( status == NULL )
        status = "500 Internal Server Error";
    printf("Content-Type: text/html\nStatus: %s\n\n"
            "<html><head>\n<title>%s</title>\n</head>\n"
            "<body><h3>%s</h3>\n%s\n</body></html>",
            status, status, status, message);
    exit(1);
}

static char *handleNoContentLength(char *ctLenBuf)
{
    char tmpnamebuf[] = "/tmp/fmgrXXXXXX";
    char buf[65535];
    unsigned ctLen = 0;
    int tmpFd, rd, wr, offs;

    if( (tmpFd = mkstemp(tmpnamebuf)) == -1 )
        fatalErrorResp("unable to create tempfile", NULL);
    unlink(tmpnamebuf);
    while(ctLen <= 1000000 && (rd = read(STDIN_FILENO, buf, sizeof(buf))) > 0)
    {
        offs = 0;
        while( offs < rd ) {
            if( (wr = write(tmpFd, buf + offs, rd - offs)) < 0 )
                fatalErrorResp("tempfile write error", NULL);
            offs += wr;
        }
        ctLen += rd;
    }
    if( ctLen > 1000000 )
        fatalErrorResp("Request body too long", "413 Payload Too Large");
    if( rd < 0 )
        fatalErrorResp("stdin read fail", NULL);
    dup2(tmpFd, 0);
    sprintf(ctLenBuf, "%u", ctLen);
    return ctLenBuf;
}

static void appendEnv(char ***envpLoc, const char *name, const char *value)
{
    int envCount = 0, eqpos;
    char **envp, *env;

    envp = *envpLoc;
    if( envp != NULL ) {
        while( envp[envCount] )
            ++envCount;
    }
    envp = realloc(envp, (envCount+2) * sizeof(char*));
    eqpos = strlen(name);
    env = malloc(eqpos + strlen(value) + 2);
    strcpy(env, name);
    env[eqpos] = '=';
    strcpy(env+eqpos+1, value);
    envp[envCount++] = env;
    envp[envCount] = NULL;
    *envpLoc = envp;
}

static void putHeader(char ***envpLoc, const char *headerName,
        const char *headerValue)
{
    static const char *omitHeaders[] = {
        "Content-Length", "Content-Type", "Authorization", "Connection",
        "Transfer-Encoding", NULL
    };
    int i, len;
    char *nameBuf;

    for(i = 0; omitHeaders[i]; ++i) {
        if( !strcasecmp(headerName, omitHeaders[i]) )
            return;
    }
    len = strlen(headerName);
    nameBuf = malloc(len + 6);
    strcpy(nameBuf, "HTTP_");
    for(i = 0; headerName[i]; ++i) {
        if( headerName[i] == '-' )
            nameBuf[i+5] = '_';
        else
            nameBuf[i+5] = toupper(headerName[i]);
    }
    nameBuf[i+5] = '\0';
    appendEnv(envpLoc, nameBuf, headerValue);
    free(nameBuf);
}

static void runCgi(const RequestHeader *hdr, const char *exePath,
        const char *peerAddr, const char *scriptName, const char *pathInfo)
{
    const char *arg0, *headerName, *headerVal, *contentLength = NULL;
    char hostname[HOST_NAME_MAX], portnum[8];
    char ctLenBuf[10];
    char **argv, **envp = NULL;
    unsigned i;

    argv = malloc(2 * sizeof(char*));
    if( (arg0 = strrchr(exePath, '/')) == NULL )
        arg0 = exePath;
    else
        ++arg0;
    argv[0] = strdup(arg0);
    argv[1] = NULL;
    appendEnv(&envp, "GATEWAY_INTERFACE", "1.1");
    if( pathInfo != NULL ) {
        appendEnv(&envp, "PATH_INFO", pathInfo);
        if( (headerVal = config_getSysPathForUrlPath(pathInfo)) != NULL )
            appendEnv(&envp, "PATH_TRANSLATED", headerVal);
    }
    if( (headerVal = reqhdr_getQuery(hdr)) != NULL )
        appendEnv(&envp, "QUERY_STRING", headerVal);
    if( peerAddr != NULL ) {
        appendEnv(&envp, "REMOTE_ADDR", peerAddr);
        appendEnv(&envp, "REMOTE_HOST", peerAddr);
    }
    appendEnv(&envp, "REQUEST_METHOD", reqhdr_getMethod(hdr));
    if( scriptName != NULL )
        appendEnv(&envp, "SCRIPT_NAME", scriptName);
    for(i = 0; reqhdr_getHeaderAt(hdr, i, &headerName, &headerVal); ++i) {
        if( ! strcasecmp(headerName, "Content-Type") ) {
            appendEnv(&envp, "CONTENT_TYPE", headerVal);
        }else if( ! strcasecmp(headerName, "Content-Length") ) {
            contentLength = headerVal;
        }else
            putHeader(&envp, headerName, headerVal);
    }
    if( gethostname(hostname, sizeof(hostname)) == 0 )
        appendEnv(&envp, "SERVER_NAME", hostname);
    sprintf(portnum, "%u", config_getListenPort());
    appendEnv(&envp, "SERVER_PORT", portnum);
    appendEnv(&envp, "SERVER_PROTOCOL", "HTTP/1.1");
    appendEnv(&envp, "SERVER_SOFTWARE", "filemanager-httpd");
    if( contentLength == NULL && reqhdr_isChunkedTransferEncoding(hdr) )
        contentLength = handleNoContentLength(ctLenBuf);
    if( contentLength != NULL )
        appendEnv(&envp, "CONTENT_LENGTH", contentLength);
    chdir( dirname( strdup(exePath)) );
    execve(exePath, argv, envp);
    fatalErrorResp("unable to execute CGI", NULL);
}

CgiExecutor *cgiexe_new(const RequestHeader *hdr, const char *exePath,
        const char *peerAddr, const char *scriptName, const char *pathInfo)
{
    CgiExecutor *cgiexe = malloc(sizeof(CgiExecutor));
    int fdToCgi[2], fdFromCgi[2];
    const char *query = reqhdr_getQuery(hdr);

    log_debug("executing %s, SCRIPT_NAME=%s, PATH_INFO=%s, QUERY_STRING=%s",
            exePath, scriptName == NULL ? "" : scriptName,
            pathInfo == NULL ? "" : pathInfo, query == NULL ? "" : query);
    if( pipe(fdToCgi) != 0 )
        log_fatal("pipe");
    if( pipe(fdFromCgi) != 0 )
        log_fatal("pipe");
    switch( fork() ) {
    case -1:
        log_fatal("fork");
    case 0:
        dup2(fdToCgi[0], 0);
        close(fdToCgi[0]);
        close(fdToCgi[1]);
        dup2(fdFromCgi[1], 1);
        close(fdFromCgi[0]);
        close(fdFromCgi[1]);
        runCgi(hdr, exePath, peerAddr, scriptName, pathInfo);
        /* does not return */
    default:
        break;
    }
    cgiexe->outFd = fdToCgi[1];
    close(fdToCgi[0]);
    drs_setNonBlockingCloExecFlags(cgiexe->outFd);
    cgiexe->inFd = fdFromCgi[0];
    close(fdFromCgi[1]);
    drs_setNonBlockingCloExecFlags(cgiexe->inFd);
    cgiexe->onlyHead = !strcmp(reqhdr_getMethod(hdr), "HEAD");
    cgiexe->cgiHeader = datahdr_new();
    return cgiexe;
}

unsigned cgiexe_processData(CgiExecutor *cgiexe, const char *data,
        unsigned len, DataReadySelector *drs)
{
    int wr;
    unsigned wrtot = 0;

    while( cgiexe->outFd >= 0 && wrtot < len ) {
        if( (wr = write(cgiexe->outFd, data + wrtot, len - wrtot)) < 0 ) {
            if( errno == EWOULDBLOCK ) {
                drs_setWriteFd(drs, cgiexe->outFd);
                return wrtot;
            }else{
                if( errno != EPIPE )
                    log_error("CGI write");
                close(cgiexe->outFd);
                cgiexe->outFd = -1;
            }
        }
        wrtot += wr;
    }
    return len;
}

void cgiexe_requestReadCompleted(CgiExecutor *cgiexe)
{
    close(cgiexe->outFd);
    cgiexe->outFd = -1;
}

RespBuf *cgiexe_getResponse(CgiExecutor *cgiexe, DataReadySelector *drs)
{
    RespBuf *resp = NULL;
    char buf[4096];
    int offset = -1, rd;
    const char *headerVal;

    while( offset < 0 && (rd = read(cgiexe->inFd, buf, sizeof(buf))) > 0 ) {
        offset = datahdr_appendData(cgiexe->cgiHeader, buf, rd, "CGI response");
    }
    if( offset >= 0 ) {
        resp = resp_new(HTTP_200_OK, cgiexe->onlyHead);
        headerVal = datahdr_getHeaderVal(cgiexe->cgiHeader, "Content-Type");
        resp_appendHeader(resp, "Content-Type",
                headerVal ? headerVal : "text/plain");
        resp_appendData(resp, buf + offset, rd - offset);
        resp_enqFile(resp, cgiexe->inFd);
        cgiexe->inFd = -1;
    }else if( rd == 0 ) { /* incomplete header in CGI response */
        close(cgiexe->inFd);
        cgiexe->inFd = -1;
        resp = resp_new(HTTP_500, cgiexe->onlyHead);
        resp_appendHeader(resp, "Content-Type", "text/html");
        resp_appendStr(resp, "<html><head>\n"
                "<title>Internal Server Error</title>\n</head>\n"
                "<body><h3>Internal Server Error</h3>\n"
                "Missing end-of-header line in CGI response\n"
                "</body></html>");
    }else{
        if( errno != EWOULDBLOCK )
            log_fatal("cgiexe: read");
        drs_setReadFd(drs, cgiexe->inFd);
        return NULL;
    }
    return resp;
}

void cgiexe_free(CgiExecutor *cgiexe)
{
    if( cgiexe != NULL ) {
        if( cgiexe->outFd != -1 )
            close(cgiexe->outFd);
        if( cgiexe->inFd != -1 )
            close(cgiexe->inFd);
        datahdr_free(cgiexe->cgiHeader);
        free(cgiexe);
    }
}

