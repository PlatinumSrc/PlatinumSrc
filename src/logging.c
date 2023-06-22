#include "logging.h"

#include <stdio.h>
#include <stdarg.h>

void plog(enum loglevel l, char* s, ...) {
    va_list v;
    va_start(v, s);
    FILE* f;
    switch (l) {
        default:;
            f = stdout;
            break;
        case LOG_TASK:;
            f = stdout;
            fputs("[>] ", f);
            break;
        case LOG_INFO:;
            f = stdout;
            fputs("[i] ", f);
            break;
        case LOG_WARN:;
            f = stderr;
            fputs("[!] ", f);
            break;
        case LOG_ERROR:;
            f = stderr;
            fputs("[E] ", f);
            break;
        case LOG_CRIT:;
            f = stderr;
            fputs("[X] ", f);
            break;
    }
    vfprintf(f, s, v);
    putchar('\n');
    va_end(v);
}
