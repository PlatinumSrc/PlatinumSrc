#ifndef AUX_LOGGING_H
#define AUX_LOGGING_H

#include "../platform.h"

#include <string.h>
#include <errno.h>

enum loglevel {
    LL_PLAIN,
    LL_TASK,
    LL_INFO,
    LL_WARN,
    LL_ERROR,
    LL_CRIT,
    LF_FUNC = 1 << 8,
    LF_FUNCLN = 3 << 8,
};

#define LE_MEMALLOC "Memory allocation error"
#define LE_CANTOPEN(p) "Failed to open %s: %s", (p), strerror(errno)
#define LE_RECVNULL "Received NULL"

void plog__write(enum loglevel, const char*, const char*, unsigned, char*, ...);
#if PLATFORM != PLAT_XBOX
    #define plog(lvl, ...) plog__write(lvl, __func__, __FILE__, __LINE__, __VA_ARGS__)
#else
    #include <pbkit/pbkit.h>
    #include <stdbool.h>
    extern bool plog__wrote;
    void plog__info(enum loglevel, const char*, const char*, unsigned);
    #define plog(lvl, ...) do {\
        plog__info(lvl, __func__, __FILE__, __LINE__);\
        pb_print(__VA_ARGS__); pb_print("\n");\
        plog__wrote = true;\
        plog__write(lvl, __func__, __FILE__, __LINE__, __VA_ARGS__);\
    } while (0)
#endif
void plog_setfile(char*);

#endif
