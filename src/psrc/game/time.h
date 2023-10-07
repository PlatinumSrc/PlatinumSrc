#ifndef PSRC_GAME_TIME_H
#define PSRC_GAME_TIME_H

#include "../platform.h"

#include <stdint.h>
#if PLATFORM == PLAT_WINDOWS || PLATFORM == PLAT_XBOX
    #include <windows.h>
#endif

void microwait(uint64_t);
uint64_t altutime(void);

#if PLATFORM == PLAT_WINDOWS || PLATFORM == PLAT_XBOX
extern LARGE_INTEGER perfctfreq;
#endif

#endif
