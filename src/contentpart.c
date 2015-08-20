#include <stdbool.h>
#include "contentpart.h"
#include "dataheader.h"
#include "fmlog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>


struct ContentPart {
    DataHeader *header;
    char *destDir;
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
    cpart->destDir = destDir ? strdup(destDir) : NULL;
    cpart->filePathName = NULL;
    cpart->name = NULL;
    cpart->fileName = NULL;
    cpart->fileDesc = -1;
    cpart->sysErrNo = 0;
    cpart->body = NULL;
    return cpart;
}

void cpart_appendData(ContentPart *cpart, const char *data, unsigned len)
{
    int offset = 0, wr, pathNameLen;
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
            if( dch_shiftAfterChr(&dchContentDisp, ';') ) {
                while( dch_extractParam(&dchContentDisp,
                            &dchName, &dchValue, ';'))
                {
                    if( dch_equalsStrIgnoreCase(&dchName, "name") ) {
                        cpart->name = dch_dupToStr(&dchValue);
                    }else if( dch_equalsStrIgnoreCase(&dchName, "filename") &&
                            dchValue.len )
                    {
                        cpart->fileName = dch_dupToStr(&dchValue);
                    }
                }
            }
        }else{
            log_debug("no Content-Disposition in part");
        }
        if( cpart->fileName != NULL ) {
            if( cpart->destDir != NULL ) {
                mb_free(cpart->filePathName);
                cpart->filePathName = mb_newWithStr(cpart->destDir);
                mb_ensureEndsWithSlash(cpart->filePathName);
                pathNameLen = mb_dataLen(cpart->filePathName);
                mb_appendStr(cpart->filePathName, cpart->fileName);
                cpart->fileDesc = open(mb_data(cpart->filePathName),
                        O_RDWR | O_CREAT | O_EXCL,
                        S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
                if( cpart->fileDesc == -1 ) {
                    if( errno == EEXIST ) {
                        mb_setStrEnd(cpart->filePathName, pathNameLen,
                                "fmgrXXXXXX");
                        cpart->fileDesc = mb_mkstemp(cpart->filePathName);
                        if( cpart->fileDesc == -1 )
                            cpart->sysErrNo = errno;
                    }else
                        cpart->sysErrNo = errno;
                }
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
    return cpart->filePathName ? mb_data(cpart->filePathName) : NULL;
}

const char *cpart_getDataStr(const ContentPart *cpart)
{
    return cpart && cpart->body ? mb_data(cpart->body) : NULL;
}

bool cpart_finishUpload(ContentPart *cpart, const char *targetName,
        bool replaceIfExists, int *sysErrNo)
{
    bool res = cpart->fileDesc != -1;
    MemBuf *targetPathName = NULL;

    if( res ) {
        if( targetName == NULL )
            targetName = cpart->fileName;
        if( targetName[0] != '/' ) {
            targetPathName = mb_newWithStr(cpart->destDir);
            mb_ensureEndsWithSlash(targetPathName);
            mb_appendStr(targetPathName, targetName);
            targetName = mb_data(targetPathName);
        }
        if( strcmp(mb_data(cpart->filePathName), targetName) ) {
            if( replaceIfExists ) {
                if( rename(mb_data(cpart->filePathName), targetName) != 0 ) {
                    res = false;
                    cpart->sysErrNo = errno;
                }
            }else{
                if( link(mb_data(cpart->filePathName),
                            mb_data(targetPathName)) == 0 )
                    unlink(mb_data(cpart->filePathName));
                else{
                    res = false;
                    cpart->sysErrNo = errno;
                }
            }
        }
        if( res ) {
            close(cpart->fileDesc);
            cpart->fileDesc = -1;
        }
        mb_free(targetPathName);
    }
    if( ! res )
        *sysErrNo = cpart->sysErrNo;
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
        free(cpart->destDir);
        free(cpart->name);
        free(cpart->fileName);
        mb_free(cpart->body);
    }
}

