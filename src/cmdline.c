#include <stdbool.h>
#include "cmdline.h"
#include "fmconfig.h"
#include <unistd.h>
#include <stdio.h>

static const char *gConfigLoc = "/etc/filemanager-httpd.d";
unsigned gLogLevel;

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
    "\t-d             - print some debug logs to standard output\n"
    "\t-p user:pass   - print encoded \"credentials\" option value and exit\n"
    "\n"
    );
}

bool cmdline_parse(int argc, char *argv[])
{
    int opt;

    while( (opt = getopt(argc, argv, "c:dhp:")) != -1 ) {
        switch( opt ) {
        case 'c':
            gConfigLoc = optarg;
            break;
        case 'd':
            ++gLogLevel;
            break;
        case 'p':
            puts(config_getCredentialsEncoded(optarg));
            return false;
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

unsigned cmdline_getLogLevel(void)
{
    return gLogLevel;
}
