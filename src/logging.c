#include "logging.h"

#include <stdio.h>
#include <stdarg.h>

void bplog(const char* func, const char* file, unsigned line, enum loglevel lvl, char* s, ...) {
    va_list v;
    va_start(v, s);
    FILE* f;
    switch (lvl) {
        default:;
            f = stdout;
            break;
        case LOGLVL_TASK:;
            f = stdout;
            fputs("### ", f);
            break;
        case LOGLVL_INFO:;
            f = stdout;
            fputs("(i): ", f);
            break;
        case LOGLVL_WARN:;
            f = stderr;
            fputs("/!\\: ", f);
            break;
        case LOGLVL_ERROR:;
            f = stderr;
            fprintf(f, "[E]: %s (%s:%u): ", func, file, line);
            break;
        case LOGLVL_CRIT:;
            f = stderr;
            fprintf(f, "{X}: %s (%s:%u): ", func, file, line);
            break;
    }
    vfprintf(f, s, v);
    putchar('\n');
    va_end(v);
}
