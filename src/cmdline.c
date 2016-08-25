#include <stdbool.h>
#include "cmdline.h"
#include "fmconfig.h"
#include <unistd.h>
#include <stdio.h>

static const char *gConfigLoc = "/etc/filemanager-httpd.d";
unsigned gLogLevel;
bool gIsInetdMode;

static void usage(void)
{
    printf(
    "\n"
    "usage: filemanager-httpd [OPTIONS]\n"
    "\n"
    "options:\n"
    "\t-h             - prints this help\n"
    "\n"
    "\t-c file|dir    - read configuration from the specified location\n"
    "\t                 instead of standard one. It may be a file or\n"
    "\t                 a directory. In case of a directory all files\n"
    "\t                 within, with \".conf\" extension are read.\n"
    "\n"
    "\t-d             - print some debug logs to standard output\n"
    "\n"
    "\t-p user:pass   - print the user and password in encoded form,\n"
    "\t                 suitable for \"credentials\" option and exit\n"
    "\n"
    "\t-i             - inetd mode (socket passed as standard input)\n"
    "\n"
    );
}

bool cmdline_parse(int argc, char *argv[])
{
    int opt;

    while( (opt = getopt(argc, argv, "c:dhip:")) != -1 ) {
        switch( opt ) {
        case 'c':
            gConfigLoc = optarg;
            break;
        case 'd':
            ++gLogLevel;
            break;
        case 'i':
            gIsInetdMode = true;
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

bool cmdline_isInetdMode(void)
{
    return gIsInetdMode;
}

