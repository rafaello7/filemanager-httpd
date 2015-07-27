#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include "folder.h"

typedef struct {
    const char *urlpath;
    const char *syspath;
} Share;


/* Parses configuration file.
 */
void config_parse(void);

const Share *config_getShareForPath(const char*);

Folder *config_getSubSharesForPathAsFolder(const char *path);


#endif /* CONFIGFILE_H */
