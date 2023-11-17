#ifndef PSRC_COMMON_TIME_H
#define PSRC_COMMON_TIME_H

#include "../platform.h"

#include <stdint.h>
#if PLATFORM == PLAT_WINDOWS || PLATFORM == PLAT_NXDK
    #include <windows.h>
#endif

void microwait(uint64_t);
uint64_t altutime(void);

#if PLATFORM == PLAT_WINDOWS || PLATFORM == PLAT_NXDK
extern LARGE_INTEGER perfctfreq;
#endif

#endif
