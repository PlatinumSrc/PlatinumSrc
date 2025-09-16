#ifdef PSRC_MODULE_EDITOR

#include "editor.h"

#ifndef PSRC_DEFAULTLOGO
    #define PSRC_DEFAULTLOGO internal:editor/icon
#endif

int bootstrap(void) {
    plog(LL_MS, "Starting editor...");

    plog(LL_MS, "Almost there...");

    plog(LL_MS, "All systems go!");

    return 0;
}

void unstrap(void) {
    plog(LL_MS, "Stopping editor...");

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
