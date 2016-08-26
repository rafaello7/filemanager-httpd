#ifndef CGIEXECUTOR_H
#define CGIEXECUTOR_H

#include "requestheader.h"
#include "respbuf.h"
#include "dataprocessingresult.h"


typedef struct CgiExecutor CgiExecutor;


/* Creates a new CgiExecutor object.
 * Parameters:
 *      exePath     - path to CGI executable
 *      peerAddr    - CGI $REMOTE_ADDR value
 *      scriptName  - CGI $SCRIPT_NAME value
 *      pathInfo    - CGI $PATH_INFO value
 */
CgiExecutor *cgiexe_new(const RequestHeader*, const char *exePath,
        const char *peerAddr, const char *scriptName, const char *pathInfo);


/* Processes the data chunk arrived.
 * Returns number of bytes processed. If less than len, the
 * DataProcessingResult is set with file descriptor needed to wait
 * for I/O ready for processing more data.
 */
unsigned cgiexe_processData(CgiExecutor*, const char *data, unsigned len,
        DataProcessingResult*);


/* Sends to CGI end of request data.
 */
void cgiexe_requestReadCompleted(CgiExecutor*);


/* Returns response if available. If not yet, sets appropriate fd in
 * DataProcessingResult and returns NULL.
 * The moment of response availability is unrelated to request processing
 * progress.
 */
RespBuf *cgiexe_getResponse(CgiExecutor*, DataProcessingResult*);


/* Ends use of CgiExecutor.
 */
void cgiexe_free(CgiExecutor*);

#endif /* CGIEXECUTOR_H */
