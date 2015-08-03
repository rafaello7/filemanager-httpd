#include <stdbool.h>
#include "cmdline.h"
#include <unistd.h>
#include <stdio.h>

static const char *gConfigLoc = "/etc/filemanager-httpd.d";

static void usage(void)
{
    printf(
    "\n"
    "usage: filemanager-httpd [OPTIONS]\n"
    "\n"
    "options:\n"
    "\t-h             - prints this help\n"
    "\t-c file|dir    - configuration file name or a directory containing the\n"
    "\t                 configuration files. In case of a directory all files\n"
    "\t                 within, with \".conf\" extenstion are read.\n"
    "\n"
    );
}

bool cmdline_parse(int argc, char *argv[])
{
    int opt;

    while( (opt = getopt(argc, argv, "c:h")) != -1 ) {
        switch( opt ) {
        case 'c':
            gConfigLoc = optarg;
            break;
        default:
            usage();
            return false;
        }
    }
    return true;
}

const char *cmdline_getConfigLoc(void)
{
    return gConfigLoc;
}

