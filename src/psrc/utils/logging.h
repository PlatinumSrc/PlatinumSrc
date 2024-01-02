#ifndef PSRC_UTILS_LOGGING_H
#define PSRC_UTILS_LOGGING_H

#include "../platform.h"
#include "threading.h"

#include <stdbool.h>
#include <string.h>
#include <errno.h>

enum __attribute__((packed)) loglevel {
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
#define LE_EOF "Unexpected EOF"
#define LE_RECVNULL "Received NULL"

#define LW_SPECIALFILE(p) "%s is a special file", (p)

#define LP_INFO "(i): "
#define LP_WARN "/!\\: "
#define LP_ERROR "[E]: "
#define LP_CRIT "{X}: "
#define LP_DEBUG "<D>: "

#ifndef PSRC_NOMT
extern mutex_t loglock;
#endif
#if PLATFORM == PLAT_NXDK
    extern bool plog__nodraw;
    extern bool plog__wrote;
#endif

bool initLogging(void);
void plog(enum loglevel lvl, const char* fn, const char* f, unsigned l, char* s, ...);
#define plog(lvl, ...) plog(lvl, __func__, __FILE__, __LINE__, __VA_ARGS__)
bool plog_setfile(char*);

#endif
