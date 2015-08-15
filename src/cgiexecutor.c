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


struct CgiExecutor {
    bool onlyHead;
    int outFd, inFd;
    DataHeader *header;
};

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
        "Content-Length", "Content-Type", "Authorization", "Connection", NULL
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

static void runCgi(const RequestHeader *hdr, const char *exePath)
{
    const char *arg0;
    char **argv, **envp = NULL;
    const char *headerName, *headerVal;
    unsigned i;

    argv = malloc(2 * sizeof(char*));
    if( (arg0 = strrchr(exePath, '/')) == NULL )
        arg0 = exePath;
    else
        ++arg0;
    argv[0] = strdup(arg0);
    argv[1] = NULL;
    appendEnv(&envp, "GATEWAY_INTERFACE", "1.1");
    appendEnv(&envp, "REQUEST_METHOD", reqhdr_getMethod(hdr));
    for(i = 0; reqhdr_getHeaderAt(hdr, i, &headerName, &headerVal); ++i)
        putHeader(&envp, headerName, headerVal);
    execve(exePath, argv, envp);
    exit(1);
}

CgiExecutor *cgiexe_new(const RequestHeader *hdr, const char *exePath)
{
    CgiExecutor *cgiexe = malloc(sizeof(CgiExecutor));
    int fdToCgi[2], fdFromCgi[2], fdFlags;

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
        runCgi(hdr, exePath);   /* does not return */
    default:
        break;
    }
    cgiexe->outFd = fdToCgi[1];
    close(fdToCgi[0]);
    if( (fdFlags = fcntl(cgiexe->outFd, F_GETFL)) == -1 )
        log_fatal("fcntl(F_GETFL)");
    if( fcntl(cgiexe->outFd, F_SETFL, fdFlags | O_NONBLOCK) < 0 )
        log_fatal("fcntl(F_SETFL)");
    if( (fdFlags = fcntl(cgiexe->outFd, F_GETFD)) == -1 )
        log_fatal("fcntl(F_GETFD)");
    if( fcntl(cgiexe->outFd, F_SETFD, fdFlags | FD_CLOEXEC) < 0 )
        log_fatal("fcntl(F_SETFD)");
    cgiexe->inFd = fdFromCgi[0];
    close(fdFromCgi[1]);
    if( (fdFlags = fcntl(cgiexe->inFd, F_GETFL)) == -1 )
        log_fatal("fcntl(F_GETFL)");
    if( fcntl(cgiexe->inFd, F_SETFL, fdFlags | O_NONBLOCK) < 0 )
        log_fatal("fcntl(F_SETFL)");
    if( (fdFlags = fcntl(cgiexe->inFd, F_GETFD)) == -1 )
        log_fatal("fcntl(F_GETFD)");
    if( fcntl(cgiexe->inFd, F_SETFD, fdFlags | FD_CLOEXEC) < 0 )
        log_fatal("fcntl(F_SETFD)");
    cgiexe->onlyHead = !strcmp(reqhdr_getMethod(hdr), "HEAD");
    cgiexe->header = datahdr_new();
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
        offset = datahdr_appendData(cgiexe->header, buf, rd, "CGI response");
    }
    if( offset >= 0 ) {
        resp = resp_new(HTTP_200_OK, cgiexe->onlyHead);
        headerVal = datahdr_getHeaderVal(cgiexe->header, "Content-Type");
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
        resp_appendStr(resp, "<html><head>Internal Server Error</head>\n"
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
        datahdr_free(cgiexe->header);
        free(cgiexe);
    }
}

