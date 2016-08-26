#ifndef SERVERCONNECTION_H
#define SERVERCONNECTION_H

#include "membuf.h"
#include "fmconfig.h"
#include "requestheader.h"
#include "datareadyselector.h"

enum ConnProcessingResult {
    CONN_IDLE,
    CONN_BUSY,
    CONN_TO_CLOSE
};

/* HTTP server connection
 */
typedef struct ServerConnection ServerConnection;


/* Creates a new connection.
 */
ServerConnection *conn_new(int socketFd);


/* Advances request processing progress.
 * If the connection should be closed, returns CONN_TO_CLOSE. If not,
 * sets appropriate file descriptors on selector and returns either
 * CONN_IDLE or CONN_BUSY.
 * The closeIfIdle parameter causes to return CONN_TO_CLOSE when the connection
 * is in idle state and no data was read.
 */
enum ConnProcessingResult conn_processDataReady(ServerConnection*,
        DataReadySelector*, bool closeIfIdle);


/* Ends use of the connection.
 */
void conn_free(ServerConnection*);


#endif /* SERVERCONNECTION_H */
