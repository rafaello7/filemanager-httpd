#ifndef CGIEXECUTOR_H
#define CGIEXECUTOR_H

#include "requestheader.h"
#include "respbuf.h"
#include "datareadyselector.h"


typedef struct CgiExecutor CgiExecutor;


CgiExecutor *cgiexe_new(const RequestHeader*, const char *exePath);

unsigned cgiexe_processData(CgiExecutor*, const char *data, unsigned len,
        DataReadySelector*);

RespBuf *cgiexe_requestReadCompleted(CgiExecutor*);

void cgiexe_free(CgiExecutor*);

#endif /* CGIEXECUTOR_H */
