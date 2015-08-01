#include <stdbool.h>
#include "cmdline.h"
#include <unistd.h>
#include <stdio.h>

static const char *gConfigFileName = "/etc/filemanager-httpd.conf";

static void usage(void)
{
    printf(
        "\n"
        "usage: filemanager-httpd [-h] [-c configfile]\n"
        "\n");
}

bool cmdline_parse(int argc, char *argv[])
{
    int opt;

    while( (opt = getopt(argc, argv, "c:h")) != -1 ) {
        switch( opt ) {
        case 'c':
            gConfigFileName = optarg;
            break;
        default:
            usage();
            return false;
        }
    }
    return true;
}

const char *cmdline_getConfigFileName(void)
{
    return gConfigFileName;
}

