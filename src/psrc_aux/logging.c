#include "logging.h"

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>

static pthread_mutex_t loglock = PTHREAD_MUTEX_INITIALIZER;

static FILE* logfile;

void plog_setfile(char* f) {
    pthread_mutex_lock(&loglock);
    if (logfile) {
        fclose(logfile);
    }
    if (!f) {
        logfile = NULL;
    } else {
        logfile = fopen(f, "w");
        if (!logfile) {
            pthread_mutex_unlock(&loglock);
            plog(LOGLVL_WARN, "Failed to open config file: %s: %s", f, strerror(errno));
            return;
        }
    }
    pthread_mutex_unlock(&loglock);
}

static void writelog(FILE* f, const char* func, const char* file, unsigned line, enum loglevel lvl, char* s, va_list v) {
    if (!isatty(fileno(f))) {
        static char tmpstr[32];
        time_t t = time(NULL);
        struct tm* bdt = localtime(&t);
        strftime(tmpstr, sizeof(tmpstr), "[ %b %d %Y %H:%M:%S ]: ", bdt);
        fputs(tmpstr, f);
    }
    switch (lvl) {
        default:;
            break;
        case LOGLVL_TASK:;
            fputs("### ", f);
            break;
        case LOGLVL_INFO:;
            fputs("(i): ", f);
            break;
        case LOGLVL_WARN:;
            fputs("/!\\: ", f);
            break;
        case LOGLVL_ERROR:;
            fprintf(f, "[E]: %s (%s:%u): ", func, file, line);
            break;
        case LOGLVL_CRIT:;
            fprintf(f, "{X}: %s (%s:%u): ", func, file, line);
            break;
    }
    vfprintf(f, s, v);
    fputc('\n', f);
}

void bplog(const char* func, const char* file, unsigned line, enum loglevel lvl, char* s, ...) {
    va_list v;
    FILE* f;
    switch (lvl) {
        default:;
            f = stdout;
            break;
        case LOGLVL_WARN:;
        case LOGLVL_ERROR:;
        case LOGLVL_CRIT:;
            f = stderr;
            break;
    }
    pthread_mutex_lock(&loglock);
    va_start(v, s);
    writelog(f, func, file, line, lvl, s, v);
    va_end(v);
    if (logfile != NULL) {
        va_start(v, s);
        writelog(logfile, func, file, line, lvl, s, v);
        va_end(v);
    }
    pthread_mutex_unlock(&loglock);
}
