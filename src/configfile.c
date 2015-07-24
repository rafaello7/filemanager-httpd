#include "configfile.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static Share *gShares;


void config_parse(void)
{
    static const char config_fname[] = "/etc/filemanager-httpd.conf";
    FILE *fp;
    char buf[1024];
    char *val;
    int count = 0;

    if( gShares == NULL ) {
        if( (fp = fopen(config_fname, "r")) != NULL ) {
            while( fgets(buf, sizeof(buf), fp) != NULL ) {
                if( buf[0] == '\0' || buf[0] == '#' )
                    continue;
                buf[strlen(buf)-1] = '\0';
                if( (val = strchr(buf, '=')) != NULL ) {
                    *val++ = '\0';
                    gShares = realloc(gShares, (count+1) * sizeof(Share));
                    gShares[count].name = strdup(buf);
                    gShares[count].rootdir = strdup(val);
                    ++count;
                }
            }
        }else{
            gShares = malloc(sizeof(Share));
            gShares->name = "root";
            gShares->rootdir = "/";
            count = 1;
        }
        gShares = realloc(gShares, (count+1) * sizeof(Share));
        gShares[count].name = NULL;
        gShares[count].rootdir = NULL;
    }
}

const Share *config_getShares(void)
{
    return gShares;
}

