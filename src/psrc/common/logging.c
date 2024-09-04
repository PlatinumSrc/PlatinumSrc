#include "logging.h"
#include "threading.h"

#include "../version.h"
#include "../debug.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    #include <unistd.h>
#endif
#include <stdarg.h>
#include <stdbool.h>

#if defined(PSRC_MODULE_ENGINE) || defined(PSRC_MODULE_EDITOR)
    #if PLATFORM == PLAT_NXDK
        #include <pbkit/pbkit.h>
        #include <pbkit/nv_regs.h>
    #elif PLATFORM == PLAT_WIN32
        #include <windows.h>
    #elif PLATFORM == PLAT_GDK
        #include <SDL.h>
    #elif defined(PSRC_USESDL1)
        #include <SDL/SDL.h>
    #else
        #include <SDL2/SDL.h>
    #endif
#endif

#include "../glue.h"

#define _STR(x) #x
#define STR(x) _STR(x)

#ifndef PSRC_NOMT
mutex_t loglock;
#endif

#if PLATFORM == PLAT_NXDK || PLATFORM == PLAT_DREAMCAST || PLATFORM == PLAT_3DS || \
PLATFORM == PLAT_GAMECUBE || PLATFORM == PLAT_WII || PLATFORM == PLAT_PS2
    #ifndef PSRC_LOG_RINGSIZE
        #if !DEBUG(1)
            #define PSRC_LOG_RINGSIZE 128
        #else
            #define PSRC_LOG_RINGSIZE 256
        #endif
    #endif
    #ifndef PSRC_LOG_LINESIZE
        #define PSRC_LOG_LINESIZE 256
    #endif
#else
    #ifndef PSRC_LOG_RINGSIZE
        #define PSRC_LOG_RINGSIZE 256
    #endif
    #ifndef PSRC_LOG_LINESIZE
        #define PSRC_LOG_LINESIZE 512
    #endif
#endif

static struct {
    char* text;
    enum loglevel level;
    time_t time;
} ring[PSRC_LOG_RINGSIZE];
static int ringstart = 0;
static int ringend = -1;
static int ringnext = 0;
static int ringsize = 0;

static void logring_step(void) {
    ringend = ringnext;
    ringnext = (ringnext + 1) % PSRC_LOG_RINGSIZE;
    if (ringsize == PSRC_LOG_RINGSIZE - 1) {
        ringstart = ringnext;
    } else {
        ++ringsize;
    }
}

bool initLogging(void) {
    #ifndef PSRC_NOMT
    if (!createMutex(&loglock)) return false;
    #endif
    ring[0].text = malloc(PSRC_LOG_LINESIZE * PSRC_LOG_RINGSIZE);
    for (int i = 1; i < PSRC_LOG_RINGSIZE; ++i) {
        ring[i].text = &ring[0].text[PSRC_LOG_LINESIZE * i];
    }
    return true;
}

static FILE* logfile = NULL;
char* logpath = NULL;

static void pl_fputdate(time_t t, FILE* f) {
    static char tmpstr[32];
    struct tm* bdt = localtime(&t);
    if (bdt) {
        strftime(tmpstr, sizeof(tmpstr), "[ %b %d %Y %H:%M:%S ]: ", bdt);
        fputs(tmpstr, f);
    } else {
        fputs("[ ? ]: ", f);
    }
}
static void pl_fputprefix(enum loglevel lvl, FILE* f) {
    switch (lvl & 0xFF) {
        default:
            return;
        case LL_INFO:
            if (lvl & LF_DEBUG) {
                fputs("<D>", f);
            } else {
                fputs("(i)", f);
            }
            break;
        case LL_WARN:
            fputs("/!\\", f);
            break;
        case LL_ERROR:
            fputs("[E]", f);
            break;
        case LL_CRIT:
            fputs("{X}", f);
            break;
    }
    fputc(':', f);
    fputc(' ', f);
}
static void pl_fputprefix_color(enum loglevel lvl, FILE* f) {
    switch (lvl & 0xFF) {
        default:
            return;
        case LL_INFO:
            if (lvl & LF_DEBUG) {
                fputs("\e[34m<\e[1mD\e[22m>:\e[0m", f);
            } else {
                fputs("\e[36m(\e[1mi\e[22m):\e[0m", f);
            }
            break;
        case LL_WARN:
            fputs("\e[33m/\e[1m!\e[22m\\:\e[0m", f);
            break;
        case LL_ERROR:
            fputs("\e[31m[\e[1mE\e[22m]:\e[0m", f);
            break;
        case LL_CRIT:
            fputs("\e[1;31m{X}:\e[0m", f);
            break;
    }
    fputc(' ', f);
}

static void plog_internal(enum loglevel lvl, const char* func, const char* file, unsigned line, const char* s, va_list v) {
    time_t t = time(NULL);
    ring[ringnext].level = lvl;
    ring[ringnext].time = t;
    int l = PSRC_LOG_LINESIZE;
    #if DEBUG(1)
        if ((lvl & 0xFF) > LL_INFO) lvl |= LF_FUNCLN;
    #endif
    if (lvl & LF_FUNC) {
        if ((lvl & ~LF_FUNC) & LF_FUNCLN) {
            l -= snprintf(ring[ringnext].text, PSRC_LOG_LINESIZE, "%s (%s:%u): ", func, file, line);
        } else {
            l -= snprintf(ring[ringnext].text, PSRC_LOG_LINESIZE, "%s: ", func);
        }
        if (l < 1) l = 1;
    }
    vsnprintf(ring[ringnext].text + (PSRC_LOG_LINESIZE - l), l, s, v);
    if (logfile) {
        pl_fputdate(t, logfile);
        pl_fputprefix(lvl, logfile);
        fputs(ring[ringnext].text, logfile);
        fputc('\n', logfile);
        fflush(logfile);
    }
    FILE* f;
    #if PLATFORM != PLAT_EMSCR
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
    #else
        f = stdout;
    #endif
    bool istty;
    #if (PLATFLAGS & PLATFLAG_UNIXLIKE)
        istty = isatty(fileno(f));
    #else
        istty = true;
    #endif
    if (istty) {
        pl_fputprefix_color(lvl, f);
    } else {
        pl_fputdate(t, f);
        pl_fputprefix(lvl, f);
    }
    fputs(ring[ringnext].text, f);
    fputc('\n', f);
    fflush(f);
    #if (defined(PSRC_MODULE_ENGINE) || defined(PSRC_MODULE_EDITOR)) && PLATFORM != PLAT_NXDK
        if (lvl & LF_MSGBOX) {
            #if PLATFORM != PLAT_WIN32
                #ifndef PSRC_USESDL1
                    int flags;
                    switch (lvl & 0xFF) {
                        default:
                            flags = SDL_MESSAGEBOX_INFORMATION;
                            break;
                        case LL_WARN:
                            flags = SDL_MESSAGEBOX_WARNING;
                            break;
                        case LL_ERROR:
                        case LL_CRIT:
                            flags = SDL_MESSAGEBOX_ERROR;
                            break;
                    }
                    SDL_ShowSimpleMessageBox(flags, titlestr, ring[ringnext].text, NULL);
                #endif
            #else
                unsigned flags;
                switch (lvl & 0xFF) {
                    default:
                        flags = MB_ICONINFORMATION;
                        break;
                    case LL_WARN:
                        flags = MB_ICONWARNING;
                        break;
                    case LL_ERROR:
                    case LL_CRIT:
                        flags = MB_ICONERROR;
                        break;
                }
                MessageBox(NULL, ring[ringnext].text, titlestr, flags);
            #endif
        }
    #endif
    logring_step();
}

#undef plog
void plog(enum loglevel lvl, const char* func, const char* file, unsigned line, const char* s, ...) {
    #ifndef PSRC_NOMT
    lockMutex(&loglock);
    #endif
    va_list v;
    va_start(v, s);
    plog_internal(lvl, func, file, line, s, v);
    va_end(v);
    #ifndef PSRC_NOMT
    unlockMutex(&loglock);
    #endif
}

static void plog_nolock(enum loglevel lvl, const char* func, const char* file, unsigned line, const char* s, ...) {
    va_list v;
    va_start(v, s);
    plog_internal(lvl, func, file, line, s, v);
    va_end(v);
}
#define plog_nolock(lvl, ...) plog_nolock(lvl, __func__, __FILE__, __LINE__, __VA_ARGS__)

bool plog_setfile(const char* f) {
    #ifndef PSRC_NOMT
    lockMutex(&loglock);
    #endif
    if (f) {
        FILE* tmp = fopen(f, "w");
        if (tmp) {
            if (logfile) fclose(logfile);
            #if DEBUG(1)
            logfile = NULL;
            plog_nolock(LL_INFO | LF_DEBUG, "Set log to %s", f);
            #endif
            logfile = tmp;
            free(logpath);
            logpath = strdup(f);
            fputs(verstr, logfile);
            fputc('\n', logfile);
            fputs(platstr, logfile);
            fputc('\n', logfile);
            if (ringsize) {
                int i = ringstart;
                while (1) {
                    pl_fputdate(ring[i].time, logfile);
                    pl_fputprefix(ring[i].level, logfile);
                    fputs(ring[i].text, logfile);
                    fputc('\n', logfile);
                    if (i == ringend) break;
                    i = (i + 1) % PSRC_LOG_RINGSIZE;
                }
            }
            fflush(logfile);
        } else {
            #if DEBUG(1)
            plog_nolock(LL_WARN | LF_DEBUG | LF_FUNC, LE_CANTOPEN(f, errno));
            #endif
            #ifndef PSRC_NOMT
            unlockMutex(&loglock);
            #endif
            return false;
        }
    } else {
        if (logfile) fclose(logfile);
        logfile = NULL;
    }
    #ifndef PSRC_NOMT
    unlockMutex(&loglock);
    #endif
    return true;
}
