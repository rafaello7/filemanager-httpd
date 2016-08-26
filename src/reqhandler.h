#ifndef REQHANDLER_H
#define REQHANDLER_H


#include "requestheader.h"
#include "dataprocessingresult.h"


typedef struct RequestHandler RequestHandler;


/* Creates a new request handler.
 */
RequestHandler *reqhdlr_new(const RequestHeader*, const char *peerAddr);


/* Processes the piece of request body.
 */
unsigned reqhdlr_processData(RequestHandler*, const char *data,
        unsigned len, DataProcessingResult*);


/* Signals the handler that request is completely read.
 */
void reqhdlr_requestReadCompleted(RequestHandler*, const RequestHeader*);


/* Writes response data to file (socket) given by fileDesc.
 * Returns true when data processing has been completed, false otherwise.
 * Upon returnm the DataProcessingResult is set appropriately.
 * More precisely:
 *  - when await some data from a file descriptor, fills
 *    DataProcessingResult.respState and respAwaitFd and returns false
 *  - otherwise returns true; if some error occurred during socket write,
 *    the DataProcessingResult.closeConn is also set to true
 */
bool reqhdlr_progressResponse(RequestHandler*, int fileDesc,
        DataProcessingResult*);

/* Ends use of RequestHandler.
 */
void reqhdlr_free(RequestHandler*);

#endif /* REQHANDLER_H */
