#include <stdbool.h>
#include "fmlog.h"
#include "cmdline.h"
#include <stdarg.h>
#include <stdio.h>


bool log_isLevel(unsigned level)
{
    return cmdline_getLogLevel() >= level;
}

void log_debug(const char *fmt, ...)
{
    va_list args;

    if( log_isLevel(1) ) {
        va_start(args, fmt);
        vfprintf(stdout, fmt, args);
        va_end(args);
        printf("\n");
        fflush(stdout);
    }
}
