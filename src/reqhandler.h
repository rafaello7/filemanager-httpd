#ifndef REQHANDLER_H
#define REQHANDLER_H


#include "requestheader.h"
#include "datareadyselector.h"


typedef struct RequestHandler RequestHandler;


/* Creates a new request handler.
 */
RequestHandler *reqhdlr_new(const RequestHeader*);


/* Processes the piece of request body.
 */
unsigned reqhdlr_processData(RequestHandler*, const char *data,
        unsigned len, DataReadySelector*);


/* Signals the handler that request is completely read.
 */
void reqhdlr_requestReadCompleted(RequestHandler*, const RequestHeader*);


/* Writes response data to file (socket) given by fd.
 * Returns true when all data were written, false otherwise. When false,
 * selector value is set appropriately.
 */
bool reqhdlr_progressResponse(RequestHandler*, int fileDesc,
        DataReadySelector*);

/* Ends use of RequestHandler.
 */
void reqhdlr_free(RequestHandler*);

#endif /* REQHANDLER_H */
