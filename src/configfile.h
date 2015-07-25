#ifndef CONFIGFILE_H
#define CONFIGFILE_H

typedef struct {
    const char *urlpath;
    const char *syspath;
} Share;


/* Parses configuration file.
 */
void config_parse(void);

const Share *config_getShareForPath(const char*);


#endif /* CONFIGFILE_H */
