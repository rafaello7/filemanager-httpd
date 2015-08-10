#ifndef CGIRESPHEADER_H
#define CGIRESPHEADER_H

#include "fmconfig.h"


/* CGI response header
 */
typedef struct CgiRespHeader CgiRespHeader;


/* Creates a ne CGI response header object.
 */
CgiRespHeader *cgirhdr_new(void);


/* Response header build helper. Appends new data which arrived from CGI
 * as a part of the response header.
 * Returns -1 when the response header is not complete. Otherwise
 * (value >= 0) - number of bytes consumed from data.
 */
int cgirhdr_appendData(CgiRespHeader*, const char *data, unsigned len);


/* If the idx does exceed the size of header array, returns false.
 * Otherwise stores in nameBuf and valueBuf the header name and value
 * and returns true.
 */
bool cgirhdr_getHeaderAt(const CgiRespHeader*, unsigned idx,
        const char **nameBuf, const char **valueBuf);


/* Returns value of the specified header; returns NULL when header with the
 * specified name does not exist.
 */
const char *cgirhdr_getHeaderVal(const CgiRespHeader*, const char *headerName);


/* Ends use of the the header.
 */
void cgirhdr_free(CgiRespHeader*);


#endif /* CGIRESPHEADER_H */
