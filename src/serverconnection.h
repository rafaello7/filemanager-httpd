#ifndef SERVERCONNECTION_H
#define SERVERCONNECTION_H

#include "membuf.h"
#include "fmconfig.h"
#include "requestheader.h"
#include "datareadyselector.h"


/* HTTP server connection
 */
typedef struct ServerConnection ServerConnection;


/* Creates a new connection.
 */
ServerConnection *conn_new(int socketFd);


/* Advances request processing progress.
 * If the request processing is finished, returns true. If not,
 * sets appropriate file descriptors on selector and returns false.
 */
bool conn_processDataReady(ServerConnection*, DataReadySelector*);


/* Ends use of the connection.
 */
void conn_free(ServerConnection*);


#endif /* SERVERCONNECTION_H */
