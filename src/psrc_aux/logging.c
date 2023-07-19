#include "logging.h"
//#include "../psrc_aux/threads.h"

#if PLATFORM != PLAT_XBOX

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

//static pthread_mutex_t loglock = PTHREAD_MUTEX_INITIALIZER;

static FILE* logfile = NULL;

void plog_setfile(char* f) {
    #if PLATFORM != PLAT_XBOX
    //pthread_mutex_lock(&loglock);
    if (logfile) {
        fclose(logfile);
    }
    if (!f) {
        logfile = NULL;
    } else {
        logfile = fopen(f, "w");
        if (!logfile) {
            //pthread_mutex_unlock(&loglock);
            plog(LOGLVL_WARN, "Failed to open config file: %s: %s", f, strerror(errno));
            return;
        }
    }
    //pthread_mutex_unlock(&loglock);
    #else
    (void)f;
    #endif
}

static void writelog(enum loglevel lvl, FILE* f, const char* func, const char* file, unsigned line, bool putfunc, char* s, va_list v) {
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
            if (putfunc) fprintf(f, "%s: ", func);
            break;
        case LOGLVL_INFO:;
            fputs("(i): ", f);
            if (putfunc) fprintf(f, "%s: ", func);
            break;
        case LOGLVL_WARN:;
            fputs("/!\\: ", f);
            if (putfunc) fprintf(f, "%s: ", func);
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

void plog__write(enum loglevel lvl, const char* func, const char* file, unsigned line, bool putfunc, char* s, ...) {
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
    //pthread_mutex_lock(&loglock);
    va_start(v, s);
    writelog(lvl, f, func, file, line, putfunc, s, v);
    va_end(v);
    if (logfile != NULL) {
        va_start(v, s);
        writelog(lvl, logfile, func, file, line, putfunc, s, v);
        va_end(v);
    }
    //pthread_mutex_unlock(&loglock);
}

#else

bool plog__wrote = true;

void plog__info(enum loglevel lvl, const char* func, const char* file, unsigned line, bool putfunc) {
    switch (lvl) {
        default:;
            break;
        case LOGLVL_TASK:;
            pb_print("### ");
            if (putfunc) pb_print("%s: ", func);
            break;
        case LOGLVL_INFO:;
            pb_print("(i): ");
            if (putfunc) pb_print("%s: ", func);
            break;
        case LOGLVL_WARN:;
            pb_print("/!\\: ");
            if (putfunc) pb_print("%s: ", func);
            break;
        case LOGLVL_ERROR:;
            pb_print("[E]: %s (%s:%u): ", func, file, line);
            break;
        case LOGLVL_CRIT:;
            pb_print("{X}: %s (%s:%u): ", func, file, line);
            break;
    }
}

#endif
