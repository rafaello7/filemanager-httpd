#ifndef REQHANDLER_H
#define REQHANDLER_H


#include "requestbuf.h"
#include "datasource.h"


/* Produces response for the specified HTTP request.
 */
DataSource *reqhdlr_processRequest(const RequestBuf*);


#endif /* REQHANDLER_H */
