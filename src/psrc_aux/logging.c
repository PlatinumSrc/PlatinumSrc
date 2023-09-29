#include "logging.h"
#include "threading.h"

#include "../version.h"
#include "../debug.h"

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdbool.h>

#if PLATFORM != PLAT_XBOX
    #include <SDL2/SDL.h>
#endif

#define _STR(x) #x
#define STR(x) _STR(x)

mutex_t loglock;

bool initLogging(void) {
    if (!createMutex(&loglock)) return false;
    return true;
}

static FILE* logfile = NULL;

static void plog_writedate(FILE* f) {
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

bool plog_setfile(char* f) {
    lockMutex(&loglock);
    if (logfile) {
        fclose(logfile);
    }
    if (!f) {
        logfile = NULL;
        #if DEBUG(1)
        plog(LL_ERROR | LF_DEBUG | LF_FUNC, LE_RECVNULL);
        #endif
        unlockMutex(&loglock);
        return false;
    } else {
        logfile = fopen(f, "w");
        if (logfile) {
            bool writelog;
            #if PLATFORM != PLAT_XBOX
            writelog = !isatty(fileno(logfile));
            #else
            writelog = true;
            #endif
            if (writelog) plog_writedate(logfile);
            fprintf(logfile, "%s\n", verstr);
            if (writelog) plog_writedate(logfile);
            fprintf(logfile, "%s\n", platstr);
        } else {
            unlockMutex(&loglock);
            #if DEBUG(1)
            int e = errno;
            plog(LL_WARN | LF_DEBUG | LF_FUNC, LE_CANTOPEN(f, e));
            #endif
            return false;
        }
    }
    unlockMutex(&loglock);
    return true;
}

static void writelog(enum loglevel lvl, FILE* f, const char* func, const char* file, unsigned line, char* s, va_list v) {
    #if PLATFORM != PLAT_XBOX
    if (!isatty(fileno(f))) plog_writedate(f);
    #else
    plog_writedate(f);
    #endif
    switch (lvl & 0xFF) {
        default:
            break;
        case LL_INFO:
            if (lvl & LF_DEBUG) {
                fputs(LP_DEBUG, f);
            } else {
                fputs(LP_INFO, f);
            }
            break;
        case LL_WARN:
            fputs(LP_WARN, f);
            #if DEBUG(1)
            lvl |= LF_FUNCLN;
            #endif
            break;
        case LL_ERROR:
            fputs(LP_ERROR, f);
            #if DEBUG(1)
            lvl |= LF_FUNCLN;
            #endif
            break;
        case LL_CRIT:
            fputs(LP_CRIT, f);
            #if DEBUG(1)
            lvl |= LF_FUNCLN;
            #endif
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
    if (lvl & LF_MSGBOX) {
        char* tmpstr = malloc(4096);
        va_start(v, s);
        vsnprintf(tmpstr, 4096, s, v);
        va_end(v);
        int flags;
        switch (lvl & 0xFF) {
            default:;
                flags = SDL_MESSAGEBOX_INFORMATION;
                break;
            case LL_WARN:;
                flags = SDL_MESSAGEBOX_WARNING;
                break;
            case LL_ERROR:;
            case LL_CRIT:;
                flags = SDL_MESSAGEBOX_ERROR;
                break;
        }
        SDL_ShowSimpleMessageBox(flags, titlestr, tmpstr, NULL);
        free(tmpstr);
    }
}

#if PLATFORM == PLAT_XBOX

#include <pbgl.h>
#include <GL/gl.h>

bool plog__nodraw;
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
            if (lvl & LF_DEBUG) {
                pb_print(LP_DEBUG);
            } else {
                pb_print(LP_INFO);
            }
            break;
        case LL_WARN:
            pb_print(LP_WARN);
            #if DEBUG(1)
            lvl |= LF_FUNCLN;
            #endif
            break;
        case LL_ERROR:
            pb_print(LP_ERROR);
            #if DEBUG(1)
            lvl |= LF_FUNCLN;
            #endif
            break;
        case LL_CRIT:
            pb_print(LP_CRIT);
            #if DEBUG(1)
            lvl |= LF_FUNCLN;
            #endif
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

void plog__draw(void) {
    if (!plog__nodraw) {
        // glClear doesn't work for some reason
        glBegin(GL_QUADS);
            glColor3f(0.0, 0.0, 0.0);
            glVertex3f(-1.0, 1.0, 0.0);
            glColor3f(0.0, 0.0, 0.0);
            glVertex3f(1.0, 1.0, 0.0);
            glColor3f(0.0, 0.0, 0.0);
            glVertex3f(1.0, -1.0, 0.0);
            glColor3f(0.0, 0.0, 0.0);
            glVertex3f(-1.0, -1.0, 0.0);
        glEnd();
        pb_draw_text_screen();
        while (pb_busy()) {}
        pbgl_swap_buffers();
    }
}

#endif
