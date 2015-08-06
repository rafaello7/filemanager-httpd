#include <stdbool.h>
#include "datasource.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>


struct DataSource {
    MemBuf *data;
    int fileDesc;
    unsigned dataOffset;    /* index of first unwritten byte in data */
    unsigned long long nbytes;  /* total number of bytes to write */
};

/* Fill data buffer startig at offset filledCount, up to end of the buffer
 * or up to nbytes, whatever is smaller.
 */
static void fillBuffer(DataSource *ds, unsigned filledCount)
{
    int toFill, rd;

    toFill = mb_dataLen(ds->data) - filledCount;
    if( filledCount + toFill > ds->nbytes )
        toFill = ds->nbytes - filledCount;
    while( toFill > 0 && ds->fileDesc != -1 ) {
        rd = mb_readFile(ds->data, ds->fileDesc, filledCount, toFill);
        if( rd > 0 ) {
            filledCount += rd;
            toFill -= rd;
        }else{
            if( rd < 0 )
                perror("fillBuffer");
            close(ds->fileDesc);
            ds->fileDesc = -1;
        }
    }
    if( toFill > 0 )
        mb_fillWithZeros(ds->data, filledCount, toFill);
}

DataSource *ds_new(MemBuf *data, int fileDesc, unsigned long long nbytes)
{
    DataSource *ds = malloc(sizeof(DataSource));
    unsigned filledCount;

    ds->data = data;
    mb_newIfNull(&ds->data);
    ds->fileDesc = fileDesc;
    ds->dataOffset = 0;
    filledCount = mb_dataLen(ds->data);
    /* make buffer size a multiple of 64k */
    mb_resize(ds->data, 65536 * (1 + filledCount/65536));
    ds->nbytes = nbytes + filledCount;
    fillBuffer(ds, filledCount);
    return ds;
}

bool ds_write(DataSource *ds, int fd, int *sysErrNo)
{
    int toWrite, wr;

    while( ds->nbytes > 0 ) {
        toWrite = mb_dataLen(ds->data) - ds->dataOffset;
        if( toWrite > ds->nbytes )
            toWrite = ds->nbytes;
        while( toWrite > 0 &&
            (wr = write(fd, mb_data(ds->data) + ds->dataOffset, toWrite)) >= 0)
        {
            ds->dataOffset += wr;
            toWrite -= wr;
            ds->nbytes -= wr;
        }
        if( toWrite > 0 ) {
            *sysErrNo = errno;
            return false;
        }
        ds->dataOffset = 0;
        fillBuffer(ds, 0);
    }
    return true;
}

void ds_free(DataSource *ds)
{
    if( ds != NULL ) {
        mb_free(ds->data);
        if( ds->fileDesc >= 0 )
            close(ds->fileDesc);
        free(ds);
    }
}

