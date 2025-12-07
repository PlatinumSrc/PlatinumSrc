#ifndef PSRC_LOGGING_H
#define PSRC_LOGGING_H

#include "platform.h"
#include "threading.h"
#include "attribs.h"

#include <stdbool.h>
#include <string.h>
#include <errno.h>

PACKEDENUM loglevel {
    LL_PLAIN,            // Print without a prefix
    LL_INFO,             // Give info or warning
    LL_MS,               // A milestone was achieved
    LL_WARN,             // Something happened that could cause issues
    LL_ERROR,            // A recoverable error occurred
    LL_CRIT,             // An irrecoverable error occurred
    LF_FUNC = 1 << 8,    // Display the function plog() was called from
    LF_FUNCLN = 3 << 8,  // Display the function and line plog() was called on
    LF_MSGBOX = 1 << 10, // Display a message box on platforms that support it
    LF_DEBUG = 1 << 11   // Is for debugging, or only appears if DEBUG(1)
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

extern char* logpath;
#if PSRC_MTLVL >= 2
extern mutex_t loglock;
#endif

bool initLogging(void);
void plog(enum loglevel lvl, const char* fn, const char* f, unsigned l, const char* s, ...);
#define plog(lvl, ...) plog(lvl, __func__, __FILE__, __LINE__, __VA_ARGS__)
bool plog_setfile(const char*);

#endif
