#ifndef DATAHEADER_H
#define DATAHEADER_H

#include "fmconfig.h"


/* Data header. A list of header lines.
 */
typedef struct DataHeader DataHeader;


/* Creates a ne CGI response header object.
 */
DataHeader *datahdr_new(void);


/* Header build helper. Appends the data as a part of the header.
 * Returns -1 when the header is not complete (empty line not reached yet).
 * Otherwise (value >= 0) - number of bytes consumed from data.
 */
int datahdr_appendData(DataHeader*, const char *data, unsigned len,
        const char *debugMsgLoc);


/* If the idx exceeds the size of header array, returns false.
 * Otherwise stores in nameBuf and valueBuf the header name and value
 * and returns true.
 */
bool datahdr_getHeaderLineAt(const DataHeader*, unsigned idx,
        const char **nameBuf, const char **valueBuf);


/* Returns value of the specified header; returns NULL when header with the
 * specified name does not exist.
 * Header name is case insensitive.
 */
const char *datahdr_getHeaderVal(const DataHeader*, const char *headerName);


/* Ends use of the the header.
 */
void datahdr_free(DataHeader*);


#endif /* DATAHEADER_H */
