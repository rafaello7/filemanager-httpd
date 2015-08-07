#ifndef REQHANDLER_H
#define REQHANDLER_H


#include "requestbuf.h"
#include "respbuf.h"


/* Produces response for the specified HTTP request.
 */
RespBuf *reqhdlr_processRequest(const RequestBuf*);


#endif /* REQHANDLER_H */
