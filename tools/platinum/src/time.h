#ifndef PLATINUM_TIME_H
#define PLATINUM_TIME_H

#include <stdint.h>
#if _WIN32
    #include <windows.h>
#endif

void microwait(uint64_t);
uint64_t altutime(void);

#ifdef _WIN32
extern LARGE_INTEGER perfctfreq;
extern uint64_t perfctmul;
#endif

#endif
