#include "time.h"

#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#if PLATFORM == PLAT_NXDK
    #include <xboxkrnl/xboxkrnl.h>
#endif

#if PLATFORM == PLAT_WIN32
LARGE_INTEGER perfctfreq;
uint64_t perfctmul = 1000000;
#elif PLATFORM == PLAT_NXDK
uint64_t perfctfreq;
#endif

uint64_t altutime(void) {
    #if PLATFORM == PLAT_WIN32
    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);
    return time.QuadPart * perfctmul / perfctfreq.QuadPart;
    #elif PLATFORM == PLAT_NXDK
    uint64_t time = KeQueryPerformanceCounter();
    return time * 1000000 / perfctfreq;
    #else
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec * 1000000 + time.tv_nsec / 1000;
    #endif
}

void microwait(uint64_t d) {
    #if PLATFORM == PLAT_WIN32
    static __thread HANDLE timer = NULL;
    if (!timer) timer = CreateWaitableTimer(NULL, true, NULL);
    LARGE_INTEGER _d = {.QuadPart = d * -10};
    SetWaitableTimer(timer, &_d, 0, NULL, NULL, false);
    WaitForSingleObject(timer, INFINITE);
    #elif PLATFORM == PLAT_NXDK
    LARGE_INTEGER _d = {.QuadPart = d * -10};
    KeDelayExecutionThread(UserMode, true, &_d);
    #else
    struct timespec dts;
    dts.tv_sec = d / 1000000;
    dts.tv_nsec = (d % 1000000) * 1000;
    nanosleep(&dts, NULL);
    #endif
}
