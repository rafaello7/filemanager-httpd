#ifndef RESPONSESENDER_H
#define RESPONSESENDER_H

#include "membuf.h"
#include "datareadyselector.h"


typedef struct ResponseSender ResponseSender;


/* Creates a new sender of response.
 * Parameters:
 *   header    - the response header. The header shall not contain the final
 *               "\r\n" sequence. It cannot also contain "Content-Length" or
 *               "Transfer-Encoding" field.
 *   body      - the response body; NULL if response is for HEAD request.
 *   fileDesc  - open file descriptor for further body content. -1 if none.
 *               If no file descriptor is passed or when it is a regular
 *               file, the Content-Length header is added with total body
 *               length. Otherwise - the body is sent using chunked
 *               Transfer-Encoding.
 */
ResponseSender *rsndr_new(MemBuf *header, MemBuf *body, int fileDesc);


/* Sends a piece of response. If whole response is sent, returns true.
 * Otherwise sets the selector appropriately and returns false.
 */
bool rsndr_send(ResponseSender*, int fd, DataReadySelector*);


/* Ends use of the sender.
 */
void rsndr_free(ResponseSender*);



#endif /* RESPONSESENDER_H */
