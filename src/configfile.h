#ifndef CONFIGFILE_H
#define CONFIGFILE_H

typedef struct {
    const char *name;
    const char *rootdir;
} Share;

void config_parse(void);

const Share *config_getShares(void);


#endif /* CONFIGFILE_H */
