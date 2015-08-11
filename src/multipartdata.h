#ifndef MULTIPARTDATA_H
#define MULTIPARTDATA_H

#include "contentpart.h"


typedef struct MultipartData MultipartData;


MultipartData *mpdata_new(const char *boundaryDelimiter, const char *destDir);


void mpdata_appendData(MultipartData*, const char *data, unsigned len);


const ContentPart *mpdata_getPart(MultipartData*, unsigned partNum);


ContentPart *mpdata_getPartByName(MultipartData*, const char *name);


bool mpdata_containsPartWithName(MultipartData*, const char *name);


void mpdata_free(MultipartData*, bool keepFile);

#endif /* MULTIPARTDATA_H */
