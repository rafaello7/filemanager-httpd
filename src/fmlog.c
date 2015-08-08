#include <stdbool.h>
#include "fmlog.h"
#include "cmdline.h"
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>


bool log_isLevel(unsigned level)
{
    return cmdline_getLogLevel() >= level;
}

void log_error(const char *msg, ...)
{
    int err = errno;
    va_list args;

    fflush(stdout);
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    if( err != 0 )
        fprintf(stderr, ": %s", strerror(err));
    fprintf(stderr, "\n");
}

void log_fatal(const char *msg, ...)
{
    int err = errno;
    va_list args;

    fflush(stdout);
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    if( err != 0 )
        fprintf(stderr, ": %s", strerror(err));
    fprintf(stderr, "\n");
    exit(1);
}

void log_debug(const char *msg, ...)
{
    va_list args;

    if( log_isLevel(1) ) {
        va_start(args, msg);
        vfprintf(stdout, msg, args);
        va_end(args);
        printf("\n");
        fflush(stdout);
    }
}
