#ifndef AUX_LOGGING_H
#define AUX_LOGGING_H

#include "../platform.h"

#include <stdbool.h>
#include <string.h>
#include <errno.h>

enum loglevel {
    LOGLVL_PLAIN,
    LOGLVL_TASK,
    LOGLVL_INFO,
    LOGLVL_WARN,
    LOGLVL_ERROR,
    LOGLVL_CRIT,
};

#define LOGERR_MEMALLOC "Memory allocation error"
#define LOGERR_CANTOPEN(p) "Failed to open %s: %s", (p), strerror(errno)

#if PLATFORM != PLAT_XBOX
    void plog__write(enum loglevel, const char*, const char*, unsigned, bool, char*, ...);
    #define plog(lvl, ...) plog__write(lvl, __func__, __FILE__, __LINE__, false, __VA_ARGS__)
    #define plogerr(lvl, ...) plog__write(lvl, __func__, __FILE__, __LINE__, true, __VA_ARGS__)
    void plog_setfile(char*);
#else
    #include <pbkit/pbkit.h>
    #include <stdbool.h>
    extern bool plog__wrote;
    void plog__info(enum loglevel, const char*, const char*, unsigned, bool);
    #define plog(lvl, ...) do {\
        plog__info(lvl, __func__, __FILE__, __LINE__, false);\
        pb_print(__VA_ARGS__); pb_print("\n");\
        plog__wrote = true;\
    } while (0)
    #define plogerr(lvl, ...) do {\
        plog__info(lvl, __func__, __FILE__, __LINE__, true);\
        pb_print(__VA_ARGS__); pb_print("\n");\
        plog__wrote = true;\
    } while (0)
#endif

#endif
