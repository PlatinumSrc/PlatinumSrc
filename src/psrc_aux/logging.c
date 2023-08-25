#include "logging.h"
#include "threading.h"

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdbool.h>

mutex_t loglock;

bool initLogging(void) {
    if (!createMutex(&loglock)) return false;
    return true;
}

static FILE* logfile = NULL;

void plog_setfile(char* f) {
    lockMutex(&loglock);
    if (logfile) {
        fclose(logfile);
    }
    if (!f) {
        logfile = NULL;
        plog(LL_ERROR | LF_FUNC, LE_RECVNULL);
    } else {
        logfile = fopen(f, "w");
        int e = errno;
        if (!logfile) {
            unlockMutex(&loglock);
            plog(LL_WARN | LF_FUNC, LE_CANTOPEN(f, e));
            return;
        }
    }
    unlockMutex(&loglock);
}

static void writelog(enum loglevel lvl, FILE* f, const char* func, const char* file, unsigned line, char* s, va_list v) {
    bool writedate;
    #if PLATFORM != PLAT_XBOX
    writedate = !isatty(fileno(f));
    #else
    writedate = true;
    #endif
    if (writedate) {
        static char tmpstr[32];
        time_t t = time(NULL);
        struct tm* bdt = localtime(&t);
        if (bdt) {
            strftime(tmpstr, sizeof(tmpstr), "[ %b %d %Y %H:%M:%S ]: ", bdt);
            fputs(tmpstr, f);
        } else {
            fputs("[ ? ]: ", f);
        }
    }
    switch (lvl & 0xFF) {
        default:
            break;
        case LL_INFO:
            fputs(LP_INFO, f);
            break;
        case LL_WARN:
            fputs(LP_WARN, f);
            break;
        case LL_ERROR:
            fputs(LP_ERROR, f);
            break;
        case LL_CRIT:
            fputs(LP_CRIT, f);
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
        default:
            f = stdout;
            break;
        case LL_WARN:
        case LL_ERROR:
        case LL_CRIT:
            f = stderr;
            break;
    }
    va_start(v, s);
    writelog(lvl, f, func, file, line, s, v);
    va_end(v);
    if (logfile != NULL) {
        va_start(v, s);
        writelog(lvl, logfile, func, file, line, s, v);
        va_end(v);
    }
}

#if PLATFORM == PLAT_XBOX

bool plog__wrote = true;

void plog__info(enum loglevel lvl, const char* func, const char* file, unsigned line) {
    #if 0
    static char tmpstr[32];
    time_t t = time(NULL);
    struct tm* bdt = localtime(&t);
    if (bdt) {
        strftime(tmpstr, sizeof(tmpstr), "[ %b %d %Y %H:%M:%S ]: ", bdt);
        pb_print(tmpstr);
    } else {
        pb_print("[ ? ]: ");
    }
    #endif
    switch (lvl & 0xFF) {
        default:
            break;
        case LL_INFO:
            pb_print(LP_INFO);
            break;
        case LL_WARN:
            pb_print(LP_WARN);
            break;
        case LL_ERROR:
            pb_print(LP_ERROR);
            break;
        case LL_CRIT:
            pb_print(LP_CRIT);
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
