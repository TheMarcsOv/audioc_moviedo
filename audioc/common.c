#include "common.h"

#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

bool DEBUG_TRACES_ENABLED = 0;

//Prints a debug message formatted by fmt and its arguments.
//Only prints in verbose mode, when DEBUG_TRACES_ENABLED = 1
void trace(const char* fmt, ...)
{
    if (DEBUG_TRACES_ENABLED) {
        fprintf(stderr, "[DEBUG]: ");
        va_list list;
        va_start(list, fmt);
        vfprintf(stderr, fmt, list);
        va_end(list);
        fputs("\n", stderr);
    }
}

void _printError(const char* file, int line, const char* fmt, ...)
{
    int err = errno;
    fprintf(stderr, "[ERROR] (at %s line %d): ", file, line);
    va_list list;
    va_start(list, fmt);
    vfprintf(stderr, fmt, list);
    va_end(list);
    fprintf(stderr, "\n\tLast error(errno=%d): %s\n", err, strerror(err));
}