#ifndef DATASOURCE_H
#define DATASOURCE_H

#include "membuf.h"


typedef struct DataSource DataSource;


/* Creates a new data source.
 * Data bytes are taken first from MemBuf, then from open file descriptor
 * given by fileDesc.
 * Always nbytes bytes are added after data. If fileDesc is -1, or nbytes
 * is greater than file size, further bytes are set to 0.
 */
DataSource *ds_new(MemBuf *data, int fileDesc, unsigned long long nbytes);


/* Writes data to file (socket) given by fd.
 * Returns true when all data were written, false otherwise. When false,
 * sysErrNo is set to errno value.
 */
bool ds_write(DataSource*, int fd, int *sysErrNo);


/* Ends use of data source
 */
void ds_free(DataSource*);

#endif /* DATASOURCE_H */
