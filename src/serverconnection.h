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


/* Sets file descriptors on which the connection is awaiting for I/O ready.
 */
void conn_setFDAwaitingForReady(ServerConnection*, DataReadySelector*);


/* Advances request processing progress.
 * Returns true when the request processing is finished.
 */
bool conn_processDataReady(ServerConnection*, DataReadySelector*);


/* Ends use of the connection.
 */
void conn_free(ServerConnection*);


#endif /* SERVERCONNECTION_H */
