#ifndef PSRC_TIME_H
#define PSRC_TIME_H

#include "platform.h"

#include <stdint.h>
#if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    #include <windows.h>
#endif

void microwait(uint64_t);
uint64_t altutime(void);

#if PLATFORM == PLAT_NXDK
extern uint64_t perfctfreq;
#elif (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
extern LARGE_INTEGER perfctfreq;
extern uint64_t perfctmul;
#endif

#endif
