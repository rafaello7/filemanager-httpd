#ifndef REQHANDLER_H
#define REQHANDLER_H


#include "requestbuf.h"
#include "datasource.h"


typedef struct RequestHandler RequestHandler;


/* Creates a new request handler.
 */
RequestHandler *reqhdlr_new(const RequestHeader*);


/* Processes the piece of request body.
 */
void reqhdlr_consumeBodyBytes(RequestHandler*, const char *data, unsigned len);


/* Signals the handler that request is completely read.
 */
void reqhdlr_requestReadCompleted(RequestHandler*, const RequestHeader*);


/* Writes response data to file (socket) given by fd.
 * Returns true when all data were written, false otherwise. When false,
 * sysErrNo is set to errno value.
 */
bool reqhdlr_emitResponseBytes(RequestHandler*, int fd, int *sysErrNo);


/* Ends use of RequestHandler.
 */
void reqhdlr_free(RequestHandler*);

#endif /* REQHANDLER_H */
