#include "logging.h"
//#include "../psrc_aux/threads.h"

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdbool.h>

//static pthread_mutex_t loglock = PTHREAD_MUTEX_INITIALIZER;

static FILE* logfile = NULL;

void plog_setfile(char* f) {
    //pthread_mutex_lock(&loglock);
    if (logfile) {
        fclose(logfile);
    }
    if (!f) {
        logfile = NULL;
        plog(LL_ERROR | LF_FUNC, LE_RECVNULL);
    } else {
        logfile = fopen(f, "w");
        if (!logfile) {
            //pthread_mutex_unlock(&loglock);
            plog(LL_WARN | LF_FUNC, LE_CANTOPEN(f));
            return;
        }
    }
    //pthread_mutex_unlock(&loglock);
}

static void writelog(enum loglevel lvl, FILE* f, const char* func, const char* file, unsigned line, char* s, va_list v) {
    bool writedate;
    #if PLATFORM != PLAT_XBOX
    writedate = !isatty(fileno(f));
    #else
    // writing to stdout or stderr causes a crash
    if (f == stdout || f == stderr) return;
    writedate = true;
    #endif
    if (writedate) {
        static char tmpstr[4];
        time_t t = time(NULL);
        struct tm* bdt = localtime(&t);
        strftime(tmpstr, sizeof(tmpstr), "[ %b %d %Y %H:%M:%S ]: ", bdt);
        fputs(tmpstr, f);
    }
    switch (lvl & 0xFF) {
        default:;
            break;
        case LL_TASK:;
            fputs("### ", f);
            break;
        case LL_INFO:;
            fputs("(i): ", f);
            break;
        case LL_WARN:;
            fputs("/!\\: ", f);
            break;
        case LL_ERROR:;
            fputs("[E]: ", f);
            break;
        case LL_CRIT:;
            fputs("{X}: ", f);
            lvl |= LF_FUNCLN;
            break;
    }
    if (lvl & LF_FUNC) {
        if ((lvl & ~LF_FUNC) & LF_FUNCLN) {
            fprintf(f, "%s (%s:%u): ", func, file, line);
        } else {
            fprintf(f, "%s: ", func);
        }
    }
    vfprintf(f, s, v);
    fputc('\n', f);
}

void plog__write(enum loglevel lvl, const char* func, const char* file, unsigned line, char* s, ...) {
    va_list v;
    FILE* f;
    switch (lvl & 0xFF) {
        default:;
            f = stdout;
            break;
        case LL_WARN:;
        case LL_ERROR:;
        case LL_CRIT:;
            f = stderr;
            break;
    }
    //pthread_mutex_lock(&loglock);
    va_start(v, s);
    writelog(lvl, f, func, file, line, s, v);
    va_end(v);
    if (logfile != NULL) {
        va_start(v, s);
        writelog(lvl, logfile, func, file, line, s, v);
        va_end(v);
    }
    //pthread_mutex_unlock(&loglock);
}

#if PLATFORM == PLAT_XBOX

bool plog__wrote = true;

void plog__info(enum loglevel lvl, const char* func, const char* file, unsigned line) {
    switch (lvl & 0xFF) {
        default:;
            break;
        case LL_TASK:;
            pb_print("### ");
            break;
        case LL_INFO:;
            pb_print("(i): ");
            break;
        case LL_WARN:;
            pb_print("/!\\: ");
            break;
        case LL_ERROR:;
            pb_print("[E]: ");
            break;
        case LL_CRIT:;
            pb_print("{X}: ");
            lvl |= LF_FUNCLN;
            break;
    }
    if (lvl & LF_FUNC) {
        // pb_print uses a small buffer
        pb_print("%s", func);
        if ((lvl & ~LF_FUNC) & LF_FUNCLN) {
            pb_print(" (");
            pb_print("%s", file);
            pb_print(":%u)", line);
        }
        pb_print(": ");
    }
}

#endif
