#include <stdbool.h>
#include "fmlog.h"
#include "cmdline.h"
#include <stdarg.h>
#include <stdio.h>


void log_debug(const char *fmt, ...)
{
    va_list args;

    if( cmdline_getLogLevel() ) {
        va_start(args, fmt);
        vfprintf(stdout, fmt, args);
        va_end(args);
        printf("\n");
        fflush(stdout);
    }
}
