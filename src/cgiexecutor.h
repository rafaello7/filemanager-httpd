#ifndef CGIEXECUTOR_H
#define CGIEXECUTOR_H

#include "requestheader.h"
#include "respbuf.h"


typedef struct CgiExecutor CgiExecutor;


CgiExecutor *cgiexe_new(const RequestHeader*, const char *exePath);

void cgiexe_consumeBodyBytes(CgiExecutor*, const char *data, unsigned len);

RespBuf *cgiexe_requestReadCompleted(CgiExecutor*);

void cgiexe_free(CgiExecutor*);

#endif /* CGIEXECUTOR_H */
