#ifndef PSRC_COMMON_TIME_H
#define PSRC_COMMON_TIME_H

#include "../platform.h"

#include <stdint.h>
#if PLATFORM == PLAT_WIN32 || PLATFORM == PLAT_UWP || PLATFORM == PLAT_NXDK
    #include <windows.h>
#endif

void microwait(uint64_t);
uint64_t altutime(void);

#if PLATFORM == PLAT_WIN32 || PLATFORM == PLAT_UWP
extern LARGE_INTEGER perfctfreq;
extern uint64_t perfctmul;
#elif PLATFORM == PLAT_NXDK
extern uint64_t perfctfreq;
#endif

#endif
