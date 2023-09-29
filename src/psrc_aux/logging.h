#ifndef AUX_LOGGING_H
#define AUX_LOGGING_H

#include "../platform.h"
#include "threading.h"

#include <string.h>
#include <errno.h>

enum loglevel {
    LL_PLAIN,
    LL_INFO,
    LL_WARN,
    LL_ERROR,
    LL_CRIT,
    LF_FUNC = 1 << 8,
    LF_FUNCLN = 3 << 8,
    LF_MSGBOX = 1 << 10,
    LF_DEBUG = 1 << 11,
};

#define LE_MEMALLOC "Memory allocation error"
#define LE_CANTOPEN(p, e) "Failed to open %s: %s", (p), strerror((e))
#define LE_NOEXIST(p) "The file %s does not exist", (p)
#define LE_DIRNOEXIST(p) "The directory %s does not exist", (p)
#define LE_ISDIR(p) "%s is a directory", (p)
#define LE_ISFILE(p) "%s is a file", (p)
#define LE_RECVNULL "Received NULL"

#define LW_SPECIALFILE(p) "%s is a special file", (p)

#define LP_INFO "(i): "
#define LP_WARN "/!\\: "
#define LP_ERROR "[E]: "
#define LP_CRIT "{X}: "
#define LP_DEBUG "<D>: "

extern mutex_t loglock;

bool initLogging(void);
void plog__write(enum loglevel, const char*, const char*, unsigned, char*, ...);
#if PLATFORM != PLAT_XBOX
    #define plog(lvl, ...) do {\
        lockMutex(&loglock);\
        plog__write(lvl, __func__, __FILE__, __LINE__, __VA_ARGS__);\
        unlockMutex(&loglock);\
    } while (0)
#else
    #include <pbkit/pbkit.h>
    #include <stdbool.h>
    extern bool plog__nodraw;
    extern bool plog__wrote;
    void plog__info(enum loglevel, const char*, const char*, unsigned);
    void plog__draw(void);
    #define plog(lvl, ...) do {\
        lockMutex(&loglock);\
        plog__info(lvl, __func__, __FILE__, __LINE__);\
        pb_print(__VA_ARGS__); pb_print("\n");\
        plog__wrote = true;\
        plog__write(lvl, __func__, __FILE__, __LINE__, __VA_ARGS__);\
        plog__draw();\
        unlockMutex(&loglock);\
    } while (0)
#endif
bool plog_setfile(char*);

#endif
