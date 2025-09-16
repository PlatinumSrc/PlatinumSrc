#ifdef PSRC_MODULE_SERVER

#include "server.h"

#ifndef PSRC_DEFAULTLOGO
    #define PSRC_DEFAULTLOGO internal:server/icon
#endif

int bootstrap(void) {
    plog(LL_MS, "Starting server...");

    plog(LL_MS, "Almost there...");

    plog(LL_MS, "All systems go!");

    return 0;
}

void unstrap(void) {
    plog(LL_MS, "Stopping server...");

    plog(LL_MS, "Done");
}

void loop(void) {
    
}

int parseargs(int argc, char** argv) {
    return 0;
}

#else

typedef int empty;

#endif
