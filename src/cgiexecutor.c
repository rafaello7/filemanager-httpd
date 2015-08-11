#include <stdbool.h>
#include "cgiexecutor.h"
#include "dataheader.h"
#include "fmlog.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>


struct CgiExecutor {
    int outFd, inFd;
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
    int fdToCgi[2], fdFromCgi[2];

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
        runCgi(hdr, exePath);
    default:
        break;
    }
    cgiexe->outFd = fdToCgi[1];
    close(fdToCgi[0]);
    cgiexe->inFd = fdFromCgi[0];
    close(fdFromCgi[1]);
    return cgiexe;
}

void cgiexe_consumeBodyBytes(CgiExecutor *cgiexe, const char *data,
        unsigned len)
{
    write(cgiexe->outFd, data, len);
}

RespBuf *cgiexe_requestReadCompleted(CgiExecutor *cgiexe)
{
    RespBuf *resp = NULL;
    char buf[4096];
    int offset, rd;
    DataHeader *hdr;
    const char *headerVal;

    close(cgiexe->outFd);
    cgiexe->outFd = -1;
    hdr = datahdr_new();
    while( (rd = read(cgiexe->inFd, buf, sizeof(buf))) > 0 ) {
        offset = 0;
        if( resp == NULL && (offset = datahdr_appendData(hdr, buf, rd,
                        "CGI response")) >= 0)
            resp = resp_new(HTTP_200_OK);
        if( resp != NULL )
            resp_appendData(resp, buf + offset, rd - offset);
    }
    close(cgiexe->inFd);
    cgiexe->inFd = -1;
    if( resp == NULL ) /* incomplete header in CGI response */
        resp = resp_new(HTTP_200_OK);
    headerVal = datahdr_getHeaderVal(hdr, "Content-Type");
    resp_appendHeader(resp, "Content-Type",
            headerVal ? headerVal : "text/plain");
    datahdr_free(hdr);
    return resp;
}

void cgiexe_free(CgiExecutor *cgiexe)
{
    if( cgiexe != NULL ) {
        if( cgiexe->outFd != -1 )
            close(cgiexe->outFd);
        if( cgiexe->inFd != -1 )
            close(cgiexe->inFd);
        free(cgiexe);
    }
}

