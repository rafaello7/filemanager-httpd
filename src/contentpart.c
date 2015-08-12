#include <stdbool.h>
#include "contentpart.h"
#include "dataheader.h"
#include "fmlog.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>


struct ContentPart {
    DataHeader *header;
    MemBuf *filePathName;   /* body output full path - if body store is file */
    char *name;             /* Content-Disposition "name" value */
    char *fileName;         /* Content-Disposition "filename" value */
    int fileDesc;           /* body output - used when "filename" is set */
    int sysErrNo;           /* errno value set when open the file was failed */
    MemBuf *body;           /* body output - uset when "filename" is not set */
};


ContentPart *cpart_new(const char *destDir)
{
    ContentPart *cpart = malloc(sizeof(ContentPart));

    cpart->header = datahdr_new();
    cpart->filePathName = destDir ? mb_newWithStr(destDir) : NULL;
    cpart->name = NULL;
    cpart->fileName = NULL;
    cpart->fileDesc = -1;
    cpart->sysErrNo = 0;
    cpart->body = NULL;
    return cpart;
}

void cpart_appendData(ContentPart *cpart, const char *data, unsigned len)
{
    int offset = 0, wr;
    const char *contentDisp;
    DataChunk dchContentDisp, dchName, dchValue;

    if( cpart->fileName == NULL && cpart->body == NULL && (offset =
            datahdr_appendData(cpart->header, data, len, "form data")) >= 0)
    {
        /* complete header received */
        contentDisp = datahdr_getHeaderVal(cpart->header,
                "Content-Disposition");
        /* Content-Disposition: form-data; name="file"; filename="Test.xml" */
        if( contentDisp != NULL ) {
            dch_initWithStr(&dchContentDisp, contentDisp);
            while( dch_shiftAfterChr(&dchContentDisp, ';') ) {
                dch_skipLeading(&dchContentDisp, " ");
                if( !dch_extractTillStrStripWS(&dchContentDisp, &dchName, "="))
                    break;
                if( dch_startsWithStr(&dchContentDisp, "\"") ) {
                    dch_shift(&dchContentDisp, 1);
                    if( ! dch_extractTillStr(&dchContentDisp, &dchValue, "\""))
                        break;
                }else{
                    dch_init(&dchValue, dchContentDisp.data,
                            dch_endOfCSpan(&dchContentDisp, 0, ';'));
                    dch_trimWS(&dchValue);
                }
                if( dch_equalsStrIgnoreCase(&dchName, "name") ) {
                    cpart->name = dch_dupToStr(&dchValue);
                }else if( dch_equalsStrIgnoreCase(&dchName, "filename") ) {
                    cpart->fileName = dch_dupToStr(&dchValue);
                }
            }
        }else{
            log_debug("no Content-Disposition in part");
        }
        if( cpart->fileName != NULL ) {
            if( cpart->filePathName != NULL ) {
                mb_ensureEndsWithSlash(cpart->filePathName);
                mb_appendStr(cpart->filePathName, cpart->fileName);
                cpart->fileDesc = open(mb_data(cpart->filePathName),
                        O_RDWR | O_CREAT | O_EXCL,
                        S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
                if( cpart->fileDesc == -1 )
                    cpart->sysErrNo = errno;
            }
        }else{
            cpart->body = mb_new();
        }
    }
    if( offset >= 0 ) {
        if( cpart->fileName != NULL ) {
            if( cpart->fileDesc != -1 ) {
                while( offset < len ) {
                    if( (wr = write(cpart->fileDesc, data + offset,
                                    len - offset)) < 0 )
                    {
                        cpart->sysErrNo = errno;
                        close(cpart->fileDesc);
                        cpart->fileDesc = -1;
                        if( unlink(mb_data(cpart->filePathName)) != 0 )
                            log_error("remove %s fail",
                                    mb_data(cpart->filePathName));
                    }
                    offset += wr;
                }
            }
        }else{
            mb_appendData(cpart->body, data + offset, len - offset);
        }
    }
}

const char *cpart_getName(const ContentPart *cpart)
{
    return cpart->name;
}

bool cpart_nameEquals(const ContentPart *cpart, const char *name)
{
    return cpart->name != NULL && !strcmp(cpart->name, name);
}

const char *cpart_getFileName(const ContentPart *cpart)
{
    return cpart->fileName;
}

const char *cpart_getFilePathName(const ContentPart *cpart)
{
    return cpart->filePathName && cpart->fileName ?
        mb_data(cpart->filePathName) : NULL;
}

const char *cpart_getDataStr(const ContentPart *cpart)
{
    return cpart->body ? mb_data(cpart->body) : NULL;
}

bool cpart_finishUpload(ContentPart *cpart, int *sysErrNo)
{
    bool res = cpart->fileDesc != -1;

    if( res ) {
        close(cpart->fileDesc);
        cpart->fileDesc = -1;
    }else{
        *sysErrNo = cpart->sysErrNo;
    }
    return res;
}

void cpart_free(ContentPart *cpart)
{
    if( cpart != NULL ) {
        if( cpart->fileDesc != -1 ) {
            close(cpart->fileDesc);
            if( unlink(mb_data(cpart->filePathName)) != 0 )
                log_error("remove %s fail", mb_data(cpart->filePathName));
        }
        datahdr_free(cpart->header);
        mb_free(cpart->filePathName);
        free(cpart->name);
        free(cpart->fileName);
        mb_free(cpart->body);
    }
}

