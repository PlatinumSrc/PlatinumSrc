#include "logging.h"
#include "threading.h"

#include "../version.h"
#include "../debug.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdbool.h>

#if PLATFORM == PLAT_NXDK
    #include <pbkit/pbkit.h>
    #include <pbkit/nv_regs.h>
    #include <pbgl.h>
    #include <GL/gl.h>
#elif PLATFORM == PLAT_WIN32
    #include <windows.h>
#else
    #include <SDL2/SDL.h>
#endif

#define _STR(x) #x
#define STR(x) _STR(x)

#ifndef PSRC_NOMT
mutex_t loglock;
#endif

bool initLogging(void) {
    #ifndef PSRC_NOMT
    if (!createMutex(&loglock)) return false;
    #endif
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
    #ifndef PSRC_NOMT
    lockMutex(&loglock);
    #endif
    if (f) {
        FILE* tmp = fopen(f, "w");
        if (tmp) {
            if (logfile) fclose(logfile);
            logfile = tmp;
            bool writelog;
            #if PLATFORM != PLAT_NXDK
            writelog = !isatty(fileno(logfile));
            #else
            writelog = true;
            #endif
            if (writelog) plog_writedate(logfile);
            fprintf(logfile, "%s\n", verstr);
            if (writelog) plog_writedate(logfile);
            fprintf(logfile, "%s\n", platstr);
        } else {
            #ifndef PSRC_NOMT
            unlockMutex(&loglock);
            #endif
            #if DEBUG(1)
            plog(LL_WARN | LF_DEBUG | LF_FUNC, LE_CANTOPEN(f, errno));
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

static void writelog(enum loglevel lvl, FILE* f, const char* func, const char* file, unsigned line, char* s, va_list v) {
    #if PLATFORM != PLAT_NXDK
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
    #if PLATFORM == PLAT_NXDK
    fflush(f);
    #endif
}

void plog__write(enum loglevel lvl, const char* func, const char* file, unsigned line, char* s, va_list ov) {
    va_list v;
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
    va_copy(v, ov);
    writelog(lvl, f, func, file, line, s, v);
    if (logfile != NULL) {
        va_copy(v, ov);
        writelog(lvl, logfile, func, file, line, s, v);
    }
    #if PLATFORM != PLAT_NXDK
    if (lvl & LF_MSGBOX) {
        char* tmpstr = malloc(4096);
        va_copy(v, ov);
        vsnprintf(tmpstr, 4096, s, v);
        #if PLATFORM != PLAT_WIN32
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
        #else
        unsigned flags;
        switch (lvl & 0xFF) {
            default:;
                flags = MB_ICONINFORMATION;
                break;
            case LL_WARN:;
                flags = MB_ICONWARNING;
                break;
            case LL_ERROR:;
            case LL_CRIT:;
                flags = MB_ICONERROR;
                break;
        }
        MessageBox(NULL, tmpstr, titlestr, flags);
        #endif
        free(tmpstr);
    }
    #endif
}

#if PLATFORM == PLAT_NXDK

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

static void plog__draw(void) {
    if (!plog__nodraw) {
        glClear(GL_COLOR_BUFFER_BIT);
        pb_draw_text_screen();
        uint32_t* p = pb_begin();
        pb_push(p++, NV097_SET_CLEAR_RECT_HORIZONTAL, 2);
        *(p++) = 639 << 16;
        *(p++) = 479 << 16;
        pb_end(p);
        pbgl_swap_buffers();
    }
}

#undef plog
void plog(enum loglevel lvl, const char* func, const char* file, unsigned line, char* s, ...) {
    #ifndef PSRC_NOMT
    lockMutex(&loglock);
    #endif
    va_list v;
    va_start(v, s);
    plog__info(lvl, func, file, line);
    {
        va_list v2;
        va_copy(v2, v);
        static char buf[512];
        vsnprintf(buf, sizeof(buf), s, v2);
        pb_print("%s", buf);
        pb_print("\n");
    }
    plog__wrote = true;
    plog__write(lvl, func, file, line, s, v);
    va_end(v);
    plog__draw();
    #ifndef PSRC_NOMT
    unlockMutex(&loglock);
    #endif
}

#else

#undef plog
void plog(enum loglevel lvl, const char* func, const char* file, unsigned line, char* s, ...) {
    #ifndef PSRC_NOMT
    lockMutex(&loglock);
    #endif
    va_list v;
    va_start(v, s);
    plog__write(lvl, func, file, line, s, v);
    va_end(v);
    #ifndef PSRC_NOMT
    unlockMutex(&loglock);
    #endif
}

#endif
