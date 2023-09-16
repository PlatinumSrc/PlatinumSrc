#ifndef GAME_TIME_H
#define GAME_TIME_H

#include "../platform.h"

#include <stdint.h>

void microwait(uint64_t);
uint64_t altutime(void);

#if PLATFORM == PLAT_WINDOWS
extern LARGE_INTEGER perfctfreq;
#endif

#endif
