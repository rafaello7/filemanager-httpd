#ifndef CGIEXECUTOR_H
#define CGIEXECUTOR_H

#include "requestheader.h"
#include "respbuf.h"
#include "datareadyselector.h"


typedef struct CgiExecutor CgiExecutor;


/* Creates a new CgiExecutor object.
 */
CgiExecutor *cgiexe_new(const RequestHeader*, const char *exePath);


/* Processes the data chunk arrived.
 * Returns number of bytes processed. If less than len, the DataReadySelector
 * is set with file descriptor needed to wait for I/O ready for processing
 * more data.
 */
unsigned cgiexe_processData(CgiExecutor*, const char *data, unsigned len,
        DataReadySelector*);


/* Sends to CGI end of request data.
 */
void cgiexe_requestReadCompleted(CgiExecutor*);


/* Returns response if available. If not yet, sets appropriate fd in
 * DataReadySelector and returns NULL.
 * The moment of response availability is unrelated to request processing
 * progress.
 */
RespBuf *cgiexe_getResponse(CgiExecutor*, DataReadySelector*);


/* Ends use of CgiExecutor.
 */
void cgiexe_free(CgiExecutor*);

#endif /* CGIEXECUTOR_H */
