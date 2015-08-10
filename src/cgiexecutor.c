#include <stdbool.h>
#include "cgiexecutor.h"
#include "fmlog.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


struct CgiExecutor {
    int outFd, inFd;
};

static void runCgi(const RequestHeader *hdr, const char *exePath)
{
    const char *arg0;
    char **argv, **envp;

    argv = malloc(2 * sizeof(char*));
    if( (arg0 = strrchr(exePath, '/')) == NULL )
        arg0 = exePath;
    else
        ++arg0;
    argv[0] = strdup(arg0);
    argv[1] = NULL;
    envp = malloc(2 * sizeof(char*));
    envp[0] = "HELLO=world";
    envp[1] = NULL;
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

RespBuf *cgiexe_bodyBytesComplete(CgiExecutor *cgiexe, const RequestHeader *hdr)
{
    RespBuf *resp = resp_new(HTTP_200_OK);
    char buf[4096];
    int rd;

    close(cgiexe->outFd);
    cgiexe->outFd = -1;
    while( (rd = read(cgiexe->inFd, buf, sizeof(buf))) > 0 ) {
        resp_appendData(resp, buf, rd);
    }
    close(cgiexe->inFd);
    cgiexe->inFd = -1;
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

